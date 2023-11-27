/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Fooyin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Fooyin.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "unifiedmusiclibrary.h"

#include "librarythreadhandler.h"

#include <core/coresettings.h>
#include <core/library/libraryinfo.h>
#include <core/library/librarymanager.h>
#include <core/library/tracksort.h>
#include <utils/async.h>
#include <utils/helpers.h>
#include <utils/settings/settingsmanager.h>

#include <QDir>
#include <QTimer>

#include <QCoroCore>

#include <chrono>
#include <ranges>
#include <vector>

using namespace std::chrono_literals;

namespace {
QCoro::Task<Fooyin::TrackList> recalSortFields(QString sort, Fooyin::TrackList tracks)
{
    co_return co_await Fooyin::Utils::asyncExec(
        [&sort, &tracks]() { return Fooyin::Sorting::calcSortFields(sort, tracks); });
}

QCoro::Task<Fooyin::TrackList> resortTracks(Fooyin::TrackList tracks)
{
    co_return co_await Fooyin::Utils::asyncExec([&tracks]() { return Fooyin::Sorting::sortTracks(tracks); });
}
} // namespace

namespace Fooyin {
struct UnifiedMusicLibrary::Private
{
    UnifiedMusicLibrary* self;

    LibraryManager* libraryManager;
    Database* database;
    SettingsManager* settings;

    LibraryThreadHandler threadHandler;

    TrackList tracks;

    Private(UnifiedMusicLibrary* self, LibraryManager* libraryManager, Database* database, SettingsManager* settings)
        : self{self}
        , libraryManager{libraryManager}
        , database{database}
        , settings{settings}
        , threadHandler{database}
    { }

    QCoro::Task<void> loadTracks(TrackList trackToLoad)
    {
        tracks = co_await addTracks(trackToLoad);
        tracks = co_await resortTracks(tracks);
        QMetaObject::invokeMethod(self, "tracksLoaded", Q_ARG(const TrackList&, trackToLoad));
    }

    QCoro::Task<TrackList> addTracks(TrackList newTracks)
    {
        std::unordered_map<int, QDir> libraryDirs;
        const auto& libraries = libraryManager->allLibraries();
        for(const auto& [id, info] : libraries) {
            libraryDirs.emplace(id, QDir{info.path});
        }

        TrackList sortedNewTracks
            = co_await recalSortFields(settings->value<Settings::Core::LibrarySortScript>(), newTracks);

        for(Track& track : sortedNewTracks) {
            const int libraryId = track.libraryId();
            if(libraryId < 0) {
                track.setRelativePath(track.filepath());
            }
            else if(libraryDirs.contains(libraryId)) {
                track.setRelativePath(libraryDirs.at(libraryId).relativeFilePath(track.filepath()));
            }
        }

        co_return presortedTracks;
        sortedNewTracks = co_await resortTracks(sortedNewTracks);
        co_return sortedNewTracks;
    }

    QCoro::Task<TrackList> updateTracks(TrackList tracksToUpdate)
    {
        TrackList updatedTracks
            = co_await recalSortFields(settings->value<Settings::Core::LibrarySortScript>(), tracksToUpdate);

        std::ranges::for_each(updatedTracks, [this](Track track) {
            std::ranges::replace_if(
                tracks, [track](Track libraryTrack) { return libraryTrack.id() == track.id(); }, track);
        });

        updatedTracks = co_await resortTracks(updatedTracks);
        co_return updatedTracks;
    }

    QCoro::Task<void> handleScanResult(ScanResult result)
    {
        if(!result.addedTracks.empty()) {
            const TrackList addedTracks = co_await addTracks(result.addedTracks);
            QMetaObject::invokeMethod(self, "tracksAdded", Q_ARG(const TrackList&, addedTracks));
            std::ranges::copy(addedTracks, std::back_inserter(tracks));
        }

        if(!result.updatedTracks.empty()) {
            const TrackList updatedTracks = co_await updateTracks(result.updatedTracks);
            QMetaObject::invokeMethod(self, "tracksUpdated", Q_ARG(const TrackList&, updatedTracks));
        }

        tracks = co_await resortTracks(tracks);
    }

    void removeTracks(const TrackList& tracksToRemove)
    {
        std::erase_if(tracks, [&tracksToRemove](const Track& libraryTrack) {
            return std::ranges::any_of(std::as_const(tracksToRemove),
                                       [libraryTrack](const Track& track) { return libraryTrack.id() == track.id(); });
        });

        QMetaObject::invokeMethod(self, "tracksDeleted", Q_ARG(const TrackList&, tracksToRemove));
    }

    void libraryStatusChanged(const LibraryInfo& library) const
    {
        libraryManager->updateLibraryStatus(library);
    }

    QCoro::Task<void> changeSort(QString sort)
    {
        const TrackList recalTracks  = co_await recalSortFields(sort, tracks);
        const TrackList sortedTracks = co_await resortTracks(recalTracks);
        tracks                       = sortedTracks;

        QMetaObject::invokeMethod(self, "tracksSorted", Q_ARG(const TrackList&, tracks));
    }
};

UnifiedMusicLibrary::UnifiedMusicLibrary(LibraryManager* libraryManager, Database* database, SettingsManager* settings,
                                         QObject* parent)
    : MusicLibrary{parent}
    , p{std::make_unique<Private>(this, libraryManager, database, settings)}
{
    connect(p->libraryManager, &LibraryManager::libraryAdded, this, &MusicLibrary::reload);
    connect(p->libraryManager, &LibraryManager::libraryAdded, this, &MusicLibrary::libraryAdded);
    connect(p->libraryManager, &LibraryManager::libraryRemoved, this, &MusicLibrary::removeLibrary);
    connect(this, &UnifiedMusicLibrary::libraryRemoved, &p->threadHandler, &LibraryThreadHandler::libraryRemoved);

    connect(&p->threadHandler, &LibraryThreadHandler::progressChanged, this, &UnifiedMusicLibrary::scanProgress);

    connect(&p->threadHandler, &LibraryThreadHandler::statusChanged, this,
            [this](const LibraryInfo& library) { p->libraryStatusChanged(library); });
    connect(&p->threadHandler, &LibraryThreadHandler::scanUpdate, this,
            [this](const ScanResult& result) { p->handleScanResult(result); });
    connect(&p->threadHandler, &LibraryThreadHandler::tracksDeleted, this,
            [this](const TrackList& tracks) { p->removeTracks(tracks); });

    connect(&p->threadHandler, &LibraryThreadHandler::gotTracks, this,
            [this](const TrackList& tracks) { p->loadTracks(tracks); });

    p->settings->subscribe<Settings::Core::LibrarySortScript>(this,
                                                              [this](const QString& sort) { p->changeSort(sort); });

    if(p->settings->value<Settings::Core::AutoRefresh>()) {
        QTimer::singleShot(3s, this, &UnifiedMusicLibrary::reloadAll);
    }
}

UnifiedMusicLibrary::~UnifiedMusicLibrary() = default;

void UnifiedMusicLibrary::loadLibrary()
{
    p->threadHandler.getAllTracks();
}

void UnifiedMusicLibrary::reloadAll()
{
    const LibraryInfoMap& libraries = p->libraryManager->allLibraries();
    for(const auto& library : libraries | std::views::filter([](const auto& lib) { return lib.second.id >= 0; })) {
        reload(library.second);
    }
}

void UnifiedMusicLibrary::reload(const LibraryInfo& library)
{
    p->threadHandler.scanLibrary(library, tracks());
}

void UnifiedMusicLibrary::rescan()
{
    p->threadHandler.getAllTracks();
    ;
}

bool UnifiedMusicLibrary::hasLibrary() const
{
    return p->libraryManager->hasLibrary();
}

bool UnifiedMusicLibrary::isEmpty() const
{
    return p->tracks.empty();
}

TrackList UnifiedMusicLibrary::tracks() const
{
    return p->tracks;
}

TrackList UnifiedMusicLibrary::tracksForIds(const TrackIds& ids) const
{
    TrackList tracks;
    tracks.reserve(ids.size());

    for(const int id : ids) {
        auto trackIt
            = std::ranges::find_if(std::as_const(p->tracks), [id](const Track& track) { return track.id() == id; });
        if(trackIt != p->tracks.cend()) {
            tracks.push_back(*trackIt);
        }
    }

    return tracks;
}

void UnifiedMusicLibrary::updateTrackMetadata(const TrackList& tracks)
{
    TrackList result{p->tracks};

    for(const Track& track : tracks) {
        auto it = std::ranges::find_if(
            result, [track](const Track& originalTrack) { return track.id() == originalTrack.id(); });
        if(it != tracks.end()) {
            *it = track;
        }
    }
    p->tracks = result;

    p->threadHandler.saveUpdatedTracks(tracks);

    emit tracksUpdated(tracks);
}

void UnifiedMusicLibrary::removeLibrary(int id)
{
    auto filtered = p->tracks | std::views::filter([id](const Track& track) { return track.libraryId() != id; });

    p->tracks = TrackList{filtered.begin(), filtered.end()};

    emit libraryRemoved(id);
}
} // namespace Fooyin

#include "moc_unifiedmusiclibrary.cpp"
