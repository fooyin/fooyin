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
#include <utils/settings/settingsmanager.h>

#include <ranges>

using namespace std::chrono_literals;

namespace {
QFuture<Fooyin::TrackList> recalSortTracks(const QString& sort, const Fooyin::TrackList& tracks)
{
    return Fooyin::Utils::asyncExec([sort, tracks]() { return Fooyin::Sorting::calcSortTracks(sort, tracks); });
}

QFuture<Fooyin::TrackList> resortTracks(const Fooyin::TrackList& tracks)
{
    return Fooyin::Utils::asyncExec([tracks]() { return Fooyin::Sorting::sortTracks(tracks); });
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
    std::unordered_map<QString, Track> pendingStatUpdates;

    Private(UnifiedMusicLibrary* self_, LibraryManager* libraryManager_, DbConnectionPoolPtr dbPool_,
            PlaylistParserRegistry* parserRegistry, SettingsManager* settings_)
        : self{self_}
        , libraryManager{libraryManager_}
        , dbPool{std::move(dbPool_)}
        , settings{settings_}
        , threadHandler{dbPool, self, parserRegistry, settings}
    { }

    void loadTracks(const TrackList& trackToLoad)
    {
        if(trackToLoad.empty()) {
            emit self->tracksLoaded({});
            return;
        }

        auto sortTracks = recalSortTracks(settings->value<Settings::Core::LibrarySortScript>(), trackToLoad);

        sortTracks.then(self, [this](const TrackList& sortedTracks) {
            tracks = sortedTracks;
            emit self->tracksLoaded(tracks);
        });
    }

    QFuture<void> addTracks(const TrackList& newTracks)
    {
        auto sortTracks = recalSortTracks(settings->value<Settings::Core::LibrarySortScript>(), newTracks);

        return sortTracks.then(self, [this](const TrackList& sortedTracks) {
            std::ranges::copy(sortedTracks, std::back_inserter(tracks));

            resortTracks(tracks).then(self, [this, sortedTracks](const TrackList& sortedLibraryTracks) {
                tracks = sortedLibraryTracks;

                emit self->tracksAdded(sortedTracks);
            });
        });
    }

    void updateLibraryTracks(const TrackList& updatedTracks)
    {
        for(const auto& track : updatedTracks) {
            auto trackIt
                = std::ranges::find_if(tracks, [&track](const Track& oldTrack) { return oldTrack.id() == track.id(); });
            if(trackIt != tracks.end()) {
                *trackIt = track;
                trackIt->clearWasModified();
            }
        }
    }

    QFuture<void> updateTracks(const TrackList& tracksToUpdate)
    {
        auto sortTracks = recalSortTracks(settings->value<Settings::Core::LibrarySortScript>(), tracksToUpdate);

        return sortTracks.then(self, [this](const TrackList& sortedTracks) {
            updateLibraryTracks(sortedTracks);

            resortTracks(tracks).then(self, [this, sortedTracks](const TrackList& sortedLibraryTracks) {
                tracks = sortedLibraryTracks;
                emit self->tracksUpdated(sortedTracks);
            });
        });
    }

    QFuture<void> updatePlayedTracks(const TrackList& tracksToUpdate)
    {
        auto sortTracks = recalSortTracks(settings->value<Settings::Core::LibrarySortScript>(), tracksToUpdate);

        return sortTracks.then(self, [this](const TrackList& sortedTracks) {
            updateLibraryTracks(sortedTracks);

            resortTracks(tracks).then(self, [this, sortedTracks](const TrackList& sortedLibraryTracks) {
                tracks = sortedLibraryTracks;
                emit self->tracksPlayed(sortedTracks);
            });
        });
    }

    void handleScanResult(const ScanResult& result)
    {
        if(!result.addedTracks.empty()) {
            addTracks(result.addedTracks).then(self, [this, result]() {
                if(!result.updatedTracks.empty()) {
                    updateTracks(result.updatedTracks);
                }
            });
        }
        else if(!result.updatedTracks.empty()) {
            updateTracks(result.updatedTracks);
        }
    }

    void scannedTracks(int id, const TrackList& newTracks, const TrackList& existingTracks)
    {
        addTracks(newTracks).then([this, id, newTracks, existingTracks]() {
            TrackList scannedTracks{newTracks};
            scannedTracks.insert(scannedTracks.end(), existingTracks.cbegin(), existingTracks.cend());
            recalSortTracks(settings->value<Settings::Core::LibrarySortScript>(), scannedTracks)
                .then(self, [this, id](const TrackList& sortedScannedTracks) {
                    emit self->tracksScanned(id, sortedScannedTracks);
                });
        });
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

    void changeSort(const QString& sort)
    {
        recalSortTracks(sort, tracks).then(self, [this](const TrackList& sortedTracks) {
            tracks = sortedTracks;
            emit self->tracksSorted(tracks);
        });
    }
};

UnifiedMusicLibrary::UnifiedMusicLibrary(LibraryManager* libraryManager, DbConnectionPoolPtr dbPool,
                                         PlaylistParserRegistry* parserRegistry, SettingsManager* settings,
                                         QObject* parent)
    : MusicLibrary{parent}
    , p{std::make_unique<Private>(this, libraryManager, std::move(dbPool), parserRegistry, settings)}
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
            [this](int id, const TrackList& newTracks, const TrackList& existingTracks) {
                p->scannedTracks(id, newTracks, existingTracks);
            });
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
                refreshAll();
            }
        },
        Qt::QueuedConnection);

    p->settings->subscribe<Settings::Core::Internal::MonitorLibraries>(
        this, [this](bool enabled) { p->threadHandler.setupWatchers(p->libraryManager->allLibraries(), enabled); });
}

UnifiedMusicLibrary::~UnifiedMusicLibrary()
{
    if(!p->pendingStatUpdates.empty()) {
        TrackList tracksToUpdate;
        for(const Track& track : p->pendingStatUpdates | std::views::values) {
            tracksToUpdate.emplace_back(track);
        }
        p->threadHandler.saveUpdatedTrackStats(tracksToUpdate);
    }
}

void UnifiedMusicLibrary::loadAllTracks()
{
    p->threadHandler.getAllTracks();
}

void UnifiedMusicLibrary::refreshAll()
{
    const LibraryInfoMap& libraries = p->libraryManager->allLibraries();
    for(const auto& library : libraries | std::views::values) {
        refresh(library);
    }
}

void UnifiedMusicLibrary::rescanAll()
{
    const LibraryInfoMap& libraries = p->libraryManager->allLibraries();
    for(const auto& library : libraries | std::views::values) {
        rescan(library);
    }
}

ScanRequest UnifiedMusicLibrary::refresh(const LibraryInfo& library)
{
    return p->threadHandler.refreshLibrary(library);
}

ScanRequest UnifiedMusicLibrary::rescan(const LibraryInfo& library)
{
    return p->threadHandler.scanLibrary(library);
}

ScanRequest UnifiedMusicLibrary::scanTracks(const TrackList& tracks)
{
    return p->threadHandler.scanTracks(tracks);
}

ScanRequest UnifiedMusicLibrary::scanFiles(const QList<QUrl>& files)
{
    return p->threadHandler.scanFiles(files);
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
    p->threadHandler.saveUpdatedTrackStats(tracks);
}

void UnifiedMusicLibrary::updateTrackStats(const Track& track)
{
    p->threadHandler.saveUpdatedTrackStats({track});
}

void UnifiedMusicLibrary::trackWasPlayed(const Track& track)
{
    const QString hash  = track.hash();
    const auto currTime = QDateTime::currentMSecsSinceEpoch();
    int playCount       = track.playCount();

    bool isPending{false};

    if(track.isInDatabase()) {
        Track updatedTrack{track};

        if(updatedTrack.firstPlayed() == 0) {
            updatedTrack.setFirstPlayed(currTime);
        }
        updatedTrack.setLastPlayed(currTime);
        updatedTrack.setPlayCount(track.playCount() + 1);

        playCount = updatedTrack.playCount();
        p->pendingStatUpdates.emplace(hash, updatedTrack);
        isPending = true;
    }

    TrackList tracksToUpdate;
    for(const auto& libraryTrack : p->tracks) {
        if(libraryTrack.hash() == hash) {
            Track sameHashTrack{libraryTrack};
            sameHashTrack.setFirstPlayed(currTime);
            sameHashTrack.setLastPlayed(currTime);
            sameHashTrack.setPlayCount(playCount > 0 ? playCount : sameHashTrack.playCount() + 1);

            tracksToUpdate.emplace_back(sameHashTrack);
            if(!isPending) {
                p->pendingStatUpdates.emplace(hash, sameHashTrack);
                isPending = true;
            }
        }
    }

    p->updatePlayedTracks(tracksToUpdate);
}

void UnifiedMusicLibrary::cleanupTracks()
{
    p->threadHandler.cleanupTracks();
}
} // namespace Fooyin

#include "moc_unifiedmusiclibrary.cpp"
