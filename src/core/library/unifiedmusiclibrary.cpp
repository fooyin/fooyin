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

#include "library/libraryinfo.h"
#include "library/librarymanager.h"
#include "librarythreadhandler.h"

#include <core/coresettings.h>
#include <core/library/tracksort.h>
#include <utils/async.h>
#include <utils/helpers.h>
#include <utils/settings/settingsmanager.h>

#include <QTimer>

#include <QCoroCore>

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
        , threadHandler{database, self}
    { }

    void loadTracks(const TrackList& trackToLoad)
    {
        addTracks(trackToLoad);
        QMetaObject::invokeMethod(self, "tracksLoaded", Q_ARG(const TrackList&, trackToLoad));
    }

    QCoro::Task<void> addTracks(TrackList newTracks)
    {
        const TrackList unsortedTracks
            = co_await recalSortFields(settings->value<Settings::Core::LibrarySortScript>(), newTracks);

        const TrackList sortedTracks = co_await resortTracks(unsortedTracks);

        std::ranges::copy(sortedTracks, std::back_inserter(tracks));
        tracks = co_await resortTracks(tracks);

        QMetaObject::invokeMethod(self, "tracksAdded", Q_ARG(const TrackList&, sortedTracks));
    }

    QCoro::Task<void> updateTracks(TrackList tracksToUpdate)
    {
        TrackList updatedTracks
            = co_await recalSortFields(settings->value<Settings::Core::LibrarySortScript>(), tracksToUpdate);

        std::ranges::for_each(updatedTracks, [this](const Track& track) {
            std::ranges::replace_if(
                tracks, [track](const Track& libraryTrack) { return libraryTrack.id() == track.id(); }, track);
        });

        tracks        = co_await resortTracks(tracks);
        updatedTracks = co_await resortTracks(updatedTracks);

        QMetaObject::invokeMethod(self, "tracksUpdated", Q_ARG(const TrackList&, updatedTracks));
    }

    void handleScanResult(const ScanResult& result)
    {
        if(!result.addedTracks.empty()) {
            addTracks(result.addedTracks);
        }
        if(!result.updatedTracks.empty()) {
            updateTracks(result.updatedTracks);
        }
    }

    QCoro::Task<void> scannedTracks(TrackList tracksScanned)
    {
        addTracks(tracksScanned);
        tracks        = co_await resortTracks(tracks);
        tracksScanned = co_await resortTracks(tracksScanned);
        QMetaObject::invokeMethod(self, "tracksScanned", Q_ARG(const TrackList&, tracksScanned));
    }

    void removeTracks(const TrackList& tracksToRemove)
    {
        std::erase_if(tracks, [&tracksToRemove](const Track& libraryTrack) {
            return std::ranges::any_of(tracksToRemove,
                                       [libraryTrack](const Track& track) { return libraryTrack.id() == track.id(); });
        });

        QMetaObject::invokeMethod(self, "tracksDeleted", Q_ARG(const TrackList&, tracksToRemove));
    }

    void removeLibrary(int id, const std::set<int>& tracksRemoved)
    {
        if(id < 0) {
            return;
        }

        TrackList newTracks;
        TrackList removedTracks;
        TrackList updatedTracks;

        for(auto& track : tracks) {
            if(track.libraryId() == id) {
                if(tracksRemoved.contains(track.id())) {
                    removedTracks.push_back(track);
                    continue;
                }
                track.setLibraryId(-1);
                updatedTracks.push_back(track);
                newTracks.push_back(track);
            }
            newTracks.push_back(track);
        }

        tracks = newTracks;

        threadHandler.libraryRemoved(id);

        QMetaObject::invokeMethod(self, "tracksDeleted", Q_ARG(const TrackList&, removedTracks));
        QMetaObject::invokeMethod(self, "tracksUpdated", Q_ARG(const TrackList&, updatedTracks));
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
    connect(p->libraryManager, &LibraryManager::libraryAdded, this, &MusicLibrary::rescan);
    connect(p->libraryManager, &LibraryManager::libraryAdded, this, &MusicLibrary::libraryAdded);
    connect(p->libraryManager, &LibraryManager::libraryRemoved, this,
            [this](int id, const std::set<int>& tracksRemoved) { p->removeLibrary(id, tracksRemoved); });

    connect(&p->threadHandler, &LibraryThreadHandler::progressChanged, this, &UnifiedMusicLibrary::scanProgress);

    connect(&p->threadHandler, &LibraryThreadHandler::statusChanged, this,
            [this](const LibraryInfo& library) { p->libraryStatusChanged(library); });
    connect(&p->threadHandler, &LibraryThreadHandler::scanUpdate, this,
            [this](const ScanResult& result) { p->handleScanResult(result); });
    connect(&p->threadHandler, &LibraryThreadHandler::scannedTracks, this,
            [this](const TrackList& tracks) { p->scannedTracks(tracks); });
    connect(&p->threadHandler, &LibraryThreadHandler::tracksDeleted, this,
            [this](const TrackList& tracks) { p->removeTracks(tracks); });
    connect(&p->threadHandler, &LibraryThreadHandler::tracksUpdated, this,
            [this](const TrackList& tracks) { p->updateTracks(tracks); });

    connect(&p->threadHandler, &LibraryThreadHandler::gotTracks, this,
            [this](const TrackList& tracks) { p->loadTracks(tracks); });

    p->settings->subscribe<Settings::Core::LibrarySortScript>(this,
                                                              [this](const QString& sort) { p->changeSort(sort); });

    if(p->settings->value<Settings::Core::AutoRefresh>()) {
        QTimer::singleShot(3s, this, &UnifiedMusicLibrary::rescanAll);
    }
}

UnifiedMusicLibrary::~UnifiedMusicLibrary() = default;

void UnifiedMusicLibrary::loadAllTracks()
{
    p->threadHandler.getAllTracks();
}

void UnifiedMusicLibrary::rescanAll()
{
    const LibraryInfoMap& libraries = p->libraryManager->allLibraries();
    for(const auto& library : libraries | std::views::filter([](const auto& lib) { return lib.second.id > 0; })) {
        rescan(library.second);
    }
}

void UnifiedMusicLibrary::rescan(const LibraryInfo& library)
{
    p->threadHandler.scanLibrary(library);
}

ScanRequest* UnifiedMusicLibrary::scanTracks(const TrackList& tracks)
{
    return p->threadHandler.scanTracks(tracks);
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
    p->threadHandler.saveUpdatedTracks(tracks);
}

void UnifiedMusicLibrary::updateTrackStats(const Track& track)
{
    p->threadHandler.saveUpdatedTrackStats(track);
}

void UnifiedMusicLibrary::trackWasPlayed(const Track& track)
{
    Track updatedTrack{track};

    const auto dt = QDateTime::currentMSecsSinceEpoch();

    updatedTrack.setFirstPlayed(dt);
    updatedTrack.setLastPlayed(dt);
    updatedTrack.setPlayCount(track.playCount() + 1);

    updateTrackStats(updatedTrack);
}

void UnifiedMusicLibrary::cleanupTracks()
{
    p->threadHandler.cleanupTracks();
}
} // namespace Fooyin

#include "moc_unifiedmusiclibrary.cpp"
