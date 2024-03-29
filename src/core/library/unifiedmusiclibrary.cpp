/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "internalcoresettings.h"
#include "library/libraryinfo.h"
#include "library/librarymanager.h"
#include "librarythreadhandler.h"

#include <core/coresettings.h>
#include <core/library/tracksort.h>
#include <utils/async.h>
#include <utils/helpers.h>
#include <utils/settings/settingsmanager.h>

#include <QCoro/QCoroCore>

#include <ranges>
#include <vector>

using namespace std::chrono_literals;

namespace {
QCoro::Task<Fooyin::TrackList> recalSortTracks(QString sort, Fooyin::TrackList tracks)
{
    co_return co_await Fooyin::Utils::asyncExec(
        [&sort, &tracks]() { return Fooyin::Sorting::calcSortTracks(sort, tracks); });
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
    DbConnectionPoolPtr dbPool;
    SettingsManager* settings;

    LibraryThreadHandler threadHandler;

    TrackList tracks;

    Private(UnifiedMusicLibrary* self_, LibraryManager* libraryManager_, DbConnectionPoolPtr dbPool_,
            SettingsManager* settings_)
        : self{self_}
        , libraryManager{libraryManager_}
        , dbPool{std::move(dbPool_)}
        , settings{settings_}
        , threadHandler{dbPool, self, settings}
    { }

    QCoro::Task<void> loadTracks(TrackList trackToLoad)
    {
        TrackList sortedTracks
            = co_await recalSortTracks(settings->value<Settings::Core::LibrarySortScript>(), trackToLoad);
        tracks = std::move(sortedTracks);
        emit self->tracksLoaded(tracks);
    }

    QCoro::Task<void> addTracks(TrackList newTracks)
    {
        const TrackList sortedTracks
            = co_await recalSortTracks(settings->value<Settings::Core::LibrarySortScript>(), newTracks);

        std::ranges::copy(sortedTracks, std::back_inserter(tracks));
        tracks = co_await resortTracks(tracks);

        emit self->tracksAdded(sortedTracks);
    }

    QCoro::Task<void> updateTracks(TrackList tracksToUpdate)
    {
        tracksToUpdate = co_await recalSortTracks(settings->value<Settings::Core::LibrarySortScript>(), tracksToUpdate);

        std::ranges::for_each(tracksToUpdate, [this](const Track& track) {
            std::ranges::replace_if(
                tracks, [track](const Track& libraryTrack) { return libraryTrack.id() == track.id(); }, track);
        });

        tracks         = co_await resortTracks(tracks);
        tracksToUpdate = co_await resortTracks(tracksToUpdate);

        emit self->tracksUpdated(tracksToUpdate);
    }

    QCoro::Task<void> handleScanResult(ScanResult result)
    {
        if(!result.addedTracks.empty()) {
            co_await addTracks(result.addedTracks);
        }
        if(!result.updatedTracks.empty()) {
            co_await updateTracks(result.updatedTracks);
        }
    }

    QCoro::Task<void> scannedTracks(int id, TrackList tracksScanned)
    {
        tracksScanned = co_await recalSortTracks(settings->value<Settings::Core::LibrarySortScript>(), tracksScanned);

        addTracks(tracksScanned);

        emit self->tracksScanned(id, tracksScanned);
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

        emit self->tracksDeleted(removedTracks);
        emit self->tracksUpdated(updatedTracks);
    }

    void libraryStatusChanged(const LibraryInfo& library) const
    {
        libraryManager->updateLibraryStatus(library);
    }

    QCoro::Task<void> changeSort(QString sort)
    {
        TrackList sortedTracks = co_await recalSortTracks(sort, tracks);
        tracks                 = std::move(sortedTracks);

        emit self->tracksSorted(tracks);
    }
};

UnifiedMusicLibrary::UnifiedMusicLibrary(LibraryManager* libraryManager, DbConnectionPoolPtr dbPool,
                                         SettingsManager* settings, QObject* parent)
    : MusicLibrary{parent}
    , p{std::make_unique<Private>(this, libraryManager, std::move(dbPool), settings)}
{
    connect(p->libraryManager, &LibraryManager::libraryAdded, this, &MusicLibrary::rescan);
    connect(p->libraryManager, &LibraryManager::libraryRemoved, this,
            [this](int id, const std::set<int>& tracksRemoved) { p->removeLibrary(id, tracksRemoved); });

    connect(&p->threadHandler, &LibraryThreadHandler::progressChanged, this, &UnifiedMusicLibrary::scanProgress);

    connect(&p->threadHandler, &LibraryThreadHandler::statusChanged, this,
            [this](const LibraryInfo& library) { p->libraryStatusChanged(library); });
    connect(&p->threadHandler, &LibraryThreadHandler::scanUpdate, this,
            [this](const ScanResult& result) { p->handleScanResult(result); });
    connect(&p->threadHandler, &LibraryThreadHandler::scannedTracks, this,
            [this](int id, const TrackList& tracks) { p->scannedTracks(id, tracks); });
    connect(&p->threadHandler, &LibraryThreadHandler::tracksUpdated, this,
            [this](const TrackList& tracks) { p->updateTracks(tracks); });
    connect(&p->threadHandler, &LibraryThreadHandler::gotTracks, this,
            [this](const TrackList& tracks) { p->loadTracks(tracks); });

    p->settings->subscribe<Settings::Core::LibrarySortScript>(this,
                                                              [this](const QString& sort) { p->changeSort(sort); });

    connect(
        this, &MusicLibrary::tracksLoaded, this,
        [this]() {
            p->threadHandler.setupWatchers(p->libraryManager->allLibraries(),
                                           p->settings->value<Settings::Core::Internal::MonitorLibraries>());
            if(p->settings->value<Settings::Core::AutoRefresh>()) {
                rescanAll();
            }
        },
        Qt::QueuedConnection);

    p->settings->subscribe<Settings::Core::Internal::MonitorLibraries>(
        this, [this](bool enabled) { p->threadHandler.setupWatchers(p->libraryManager->allLibraries(), enabled); });
}

UnifiedMusicLibrary::~UnifiedMusicLibrary() = default;

void UnifiedMusicLibrary::loadAllTracks()
{
    p->threadHandler.getAllTracks();
}

void UnifiedMusicLibrary::rescanAll()
{
    const LibraryInfoMap& libraries = p->libraryManager->allLibraries();
    for(const auto& library : libraries | std::views::values) {
        rescan(library);
    }
}

ScanRequest UnifiedMusicLibrary::rescan(const LibraryInfo& library)
{
    return p->threadHandler.scanLibrary(library);
}

ScanRequest UnifiedMusicLibrary::scanTracks(const TrackList& tracks)
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
