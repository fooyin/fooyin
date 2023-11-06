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
QCoro::Task<Fy::Core::TrackList> recalSortFields(QString sort, Fy::Core::TrackList tracks)
{
    co_return co_await Fy::Utils::asyncExec(
        [&sort, &tracks]() { return Fy::Core::Library::Sorting::calcSortFields(sort, tracks); });
}

QCoro::Task<Fy::Core::TrackList> resortTracks(Fy::Core::TrackList tracks)
{
    co_return co_await Fy::Utils::asyncExec([&tracks]() { return Fy::Core::Library::Sorting::sortTracks(tracks); });
}
} // namespace

namespace Fy::Core::Library {
struct UnifiedMusicLibrary::Private
{
    UnifiedMusicLibrary* self;

    LibraryManager* libraryManager;
    DB::Database* database;
    Utils::SettingsManager* settings;

    LibraryThreadHandler threadHandler;

    TrackList tracks;

    Private(UnifiedMusicLibrary* self, LibraryManager* libraryManager, DB::Database* database,
            Utils::SettingsManager* settings)
        : self{self}
        , libraryManager{libraryManager}
        , database{database}
        , settings{settings}
        , threadHandler{database}
    { }

    void getAllTracks()
    {
        QMetaObject::invokeMethod(&threadHandler, &LibraryThreadHandler::getAllTracks);
    }

    QCoro::Task<void> loadTracks(TrackList trackToLoad)
    {
        co_await addTracks(trackToLoad);
        QMetaObject::invokeMethod(self, "tracksLoaded", Q_ARG(const Core::TrackList&, trackToLoad));
    }

    QCoro::Task<TrackList> addTracks(TrackList newTracks)
    {
        tracks.reserve(tracks.size() + newTracks.size());

        std::unordered_map<int, QDir> libraryDirs;
        const auto& libraries = libraryManager->allLibraries();
        for(const auto& [id, info] : libraries) {
            libraryDirs.emplace(id, QDir{info.path});
        }

        TrackList presortedTracks = co_await recalSortFields(settings->value<Settings::LibrarySortScript>(), newTracks);
        for(Track& track : presortedTracks) {
            const int libraryId = track.libraryId();
            if(libraryDirs.contains(libraryId)) {
                track.setRelativePath(libraryDirs.at(libraryId).relativeFilePath(track.filepath()));
            }
        }
        std::ranges::copy(presortedTracks, std::back_inserter(tracks));

        tracks = co_await resortTracks(tracks);

        co_return presortedTracks;
    }

    QCoro::Task<void> newTracks(TrackList newTracks)
    {
        TrackList addedTracks = co_await addTracks(newTracks);
        QMetaObject::invokeMethod(self, "addedTracks", Q_ARG(const Core::TrackList&, addedTracks));
    }

    QCoro::Task<void> updateTracks(TrackList tracksToUpdate)
    {
        TrackList updatedTracks
            = co_await recalSortFields(settings->value<Settings::LibrarySortScript>(), tracksToUpdate);
        updatedTracks = co_await resortTracks(updatedTracks);

        std::ranges::for_each(updatedTracks, [this](Track track) {
            std::ranges::replace_if(
                tracks, [track](Track libraryTrack) { return libraryTrack.id() == track.id(); }, track);
        });

        tracks = co_await resortTracks(tracks);

        QMetaObject::invokeMethod(self, "tracksUpdated", Q_ARG(const Core::TrackList&, updatedTracks));
    }

    void removeTracks(const TrackList& tracksToRemove)
    {
        std::erase_if(tracks, [&tracksToRemove](const Track& libraryTrack) {
            return std::ranges::any_of(std::as_const(tracksToRemove),
                                       [libraryTrack](const Track& track) { return libraryTrack.id() == track.id(); });
        });

        QMetaObject::invokeMethod(self, "tracksDeleted", Q_ARG(const Core::TrackList&, tracksToRemove));
    }

    void libraryStatusChanged(const LibraryInfo& library) const
    {
        libraryManager->updateLibraryStatus(library);
    }
};

UnifiedMusicLibrary::UnifiedMusicLibrary(LibraryManager* libraryManager, DB::Database* database,
                                         Utils::SettingsManager* settings, QObject* parent)
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
    connect(&p->threadHandler, &LibraryThreadHandler::addedTracks, this,
            [this](const TrackList& tracks) { p->newTracks(tracks); });
    connect(&p->threadHandler, &LibraryThreadHandler::updatedTracks, this,
            [this](const TrackList& tracks) { p->updateTracks(tracks); });
    connect(&p->threadHandler, &LibraryThreadHandler::tracksDeleted, this,
            [this](const TrackList& tracks) { p->removeTracks(tracks); });

    connect(&p->threadHandler, &LibraryThreadHandler::gotTracks, this,
            [this](const TrackList& tracks) { p->loadTracks(tracks); });

    p->settings->subscribe<Settings::LibrarySortScript>(this, &UnifiedMusicLibrary::changeSort);

    if(p->settings->value<Settings::AutoRefresh>()) {
        QTimer::singleShot(3s, this, &Library::UnifiedMusicLibrary::reloadAll);
    }
}

UnifiedMusicLibrary::~UnifiedMusicLibrary() = default;

void UnifiedMusicLibrary::loadLibrary()
{
    p->getAllTracks();
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
    QMetaObject::invokeMethod(&p->threadHandler, "scanLibrary", Q_ARG(const LibraryInfo&, library),
                              Q_ARG(const Core::TrackList&, tracks()));
}

void UnifiedMusicLibrary::rescan()
{
    p->getAllTracks();
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

    emit tracksUpdated(tracks);
}

QCoro::Task<void> UnifiedMusicLibrary::changeSort(QString sort)
{
    const TrackList recalTracks  = co_await recalSortFields(sort, p->tracks);
    const TrackList sortedTracks = co_await resortTracks(recalTracks);
    p->tracks                    = sortedTracks;

    emit tracksSorted(p->tracks);
}

void UnifiedMusicLibrary::removeLibrary(int id)
{
    auto filtered = p->tracks | std::views::filter([id](const Track& track) { return track.libraryId() != id; });

    p->tracks = TrackList{filtered.begin(), filtered.end()};

    emit libraryRemoved(id);
}
} // namespace Fy::Core::Library
