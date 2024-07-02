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
    UnifiedMusicLibrary* m_self;

    LibraryManager* m_libraryManager;
    DbConnectionPoolPtr m_dbPool;
    SettingsManager* m_settings;

    LibraryThreadHandler m_threadHandler;

    TrackList m_tracks;
    std::unordered_map<QString, Track> m_pendingStatUpdates;

    Private(UnifiedMusicLibrary* self, LibraryManager* libraryManager, DbConnectionPoolPtr dbPool,
            std::shared_ptr<PlaylistLoader> playlistLoader, std::shared_ptr<TagLoader> tagLoader,
            SettingsManager* settings)
        : m_self{self}
        , m_libraryManager{libraryManager}
        , m_dbPool{std::move(dbPool)}
        , m_settings{settings}
        , m_threadHandler{m_dbPool, m_self, std::move(playlistLoader), std::move(tagLoader), m_settings}
    { }

    void loadTracks(const TrackList& trackToLoad)
    {
        if(trackToLoad.empty()) {
            emit m_self->tracksLoaded({});
            return;
        }

        auto sortTracks = recalSortTracks(m_settings->value<Settings::Core::LibrarySortScript>(), trackToLoad);

        sortTracks.then(m_self, [this](const TrackList& sortedTracks) {
            m_tracks = sortedTracks;
            emit m_self->tracksLoaded(m_tracks);
        });
    }

    QFuture<void> addTracks(const TrackList& newTracks)
    {
        auto sortTracks = recalSortTracks(m_settings->value<Settings::Core::LibrarySortScript>(), newTracks);

        return sortTracks.then(m_self, [this](const TrackList& sortedTracks) {
            std::ranges::copy(sortedTracks, std::back_inserter(m_tracks));

            resortTracks(m_tracks).then(m_self, [this, sortedTracks](const TrackList& sortedLibraryTracks) {
                m_tracks = sortedLibraryTracks;

                emit m_self->tracksAdded(sortedTracks);
            });
        });
    }

    void updateLibraryTracks(const TrackList& updatedTracks)
    {
        for(const auto& track : updatedTracks) {
            auto trackIt = std::ranges::find_if(
                m_tracks, [&track](const Track& oldTrack) { return oldTrack.id() == track.id(); });
            if(trackIt != m_tracks.end()) {
                *trackIt = track;
                trackIt->clearWasModified();
            }
        }
    }

    QFuture<void> updateTracks(const TrackList& tracksToUpdate)
    {
        auto sortTracks = recalSortTracks(m_settings->value<Settings::Core::LibrarySortScript>(), tracksToUpdate);

        return sortTracks.then(m_self, [this](const TrackList& sortedTracks) {
            updateLibraryTracks(sortedTracks);

            resortTracks(m_tracks).then(m_self, [this, sortedTracks](const TrackList& sortedLibraryTracks) {
                m_tracks = sortedLibraryTracks;
                emit m_self->tracksUpdated(sortedTracks);
            });
        });
    }

    QFuture<void> updatePlayedTracks(const TrackList& tracksToUpdate)
    {
        auto sortTracks = recalSortTracks(m_settings->value<Settings::Core::LibrarySortScript>(), tracksToUpdate);

        return sortTracks.then(m_self, [this](const TrackList& sortedTracks) {
            updateLibraryTracks(sortedTracks);

            resortTracks(m_tracks).then(m_self, [this, sortedTracks](const TrackList& sortedLibraryTracks) {
                m_tracks = sortedLibraryTracks;
                emit m_self->tracksPlayed(sortedTracks);
            });
        });
    }

    void handleScanResult(const ScanResult& result)
    {
        if(!result.addedTracks.empty()) {
            addTracks(result.addedTracks).then(m_self, [this, result]() {
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
            recalSortTracks(m_settings->value<Settings::Core::LibrarySortScript>(), scannedTracks)
                .then(m_self, [this, id](const TrackList& sortedScannedTracks) {
                    emit m_self->tracksScanned(id, sortedScannedTracks);
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

        for(auto& track : m_tracks) {
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

        m_tracks = newTracks;

        m_threadHandler.libraryRemoved(id);

        emit m_self->tracksDeleted(removedTracks);
        emit m_self->tracksUpdated(updatedTracks);
    }

    void libraryStatusChanged(const LibraryInfo& library) const
    {
        m_libraryManager->updateLibraryStatus(library);
    }

    void changeSort(const QString& sort)
    {
        recalSortTracks(sort, m_tracks).then(m_self, [this](const TrackList& sortedTracks) {
            m_tracks = sortedTracks;
            emit m_self->tracksSorted(m_tracks);
        });
    }
};

UnifiedMusicLibrary::UnifiedMusicLibrary(LibraryManager* libraryManager, DbConnectionPoolPtr dbPool,
                                         std::shared_ptr<PlaylistLoader> playlistLoader,
                                         std::shared_ptr<TagLoader> tagLoader, SettingsManager* settings,
                                         QObject* parent)
    : MusicLibrary{parent}
    , p{std::make_unique<Private>(this, libraryManager, std::move(dbPool), std::move(playlistLoader),
                                  std::move(tagLoader), settings)}
{
    connect(p->m_libraryManager, &LibraryManager::libraryAdded, this, &MusicLibrary::rescan);
    connect(p->m_libraryManager, &LibraryManager::libraryRemoved, this,
            [this](int id, const std::set<int>& tracksRemoved) { p->removeLibrary(id, tracksRemoved); });

    connect(&p->m_threadHandler, &LibraryThreadHandler::progressChanged, this, &UnifiedMusicLibrary::scanProgress);

    connect(&p->m_threadHandler, &LibraryThreadHandler::statusChanged, this,
            [this](const LibraryInfo& library) { p->libraryStatusChanged(library); });
    connect(&p->m_threadHandler, &LibraryThreadHandler::scanUpdate, this,
            [this](const ScanResult& result) { p->handleScanResult(result); });
    connect(&p->m_threadHandler, &LibraryThreadHandler::scannedTracks, this,
            [this](int id, const TrackList& newTracks, const TrackList& existingTracks) {
                p->scannedTracks(id, newTracks, existingTracks);
            });
    connect(&p->m_threadHandler, &LibraryThreadHandler::tracksUpdated, this,
            [this](const TrackList& tracks) { p->updateTracks(tracks); });
    connect(&p->m_threadHandler, &LibraryThreadHandler::gotTracks, this,
            [this](const TrackList& tracks) { p->loadTracks(tracks); });

    p->m_settings->subscribe<Settings::Core::LibrarySortScript>(this,
                                                                [this](const QString& sort) { p->changeSort(sort); });

    connect(
        this, &MusicLibrary::tracksLoaded, this,
        [this]() {
            p->m_threadHandler.setupWatchers(p->m_libraryManager->allLibraries(),
                                             p->m_settings->value<Settings::Core::Internal::MonitorLibraries>());
            if(p->m_settings->value<Settings::Core::AutoRefresh>()) {
                refreshAll();
            }
        },
        Qt::QueuedConnection);

    p->m_settings->subscribe<Settings::Core::Internal::MonitorLibraries>(
        this, [this](bool enabled) { p->m_threadHandler.setupWatchers(p->m_libraryManager->allLibraries(), enabled); });
}

UnifiedMusicLibrary::~UnifiedMusicLibrary()
{
    if(!p->m_pendingStatUpdates.empty()) {
        TrackList tracksToUpdate;
        for(const Track& track : p->m_pendingStatUpdates | std::views::values) {
            tracksToUpdate.emplace_back(track);
        }
        p->m_threadHandler.saveUpdatedTrackStats(tracksToUpdate);
    }
}

void UnifiedMusicLibrary::loadAllTracks()
{
    p->m_threadHandler.getAllTracks();
}

void UnifiedMusicLibrary::refreshAll()
{
    const LibraryInfoMap& libraries = p->m_libraryManager->allLibraries();
    for(const auto& library : libraries | std::views::values) {
        refresh(library);
    }
}

void UnifiedMusicLibrary::rescanAll()
{
    const LibraryInfoMap& libraries = p->m_libraryManager->allLibraries();
    for(const auto& library : libraries | std::views::values) {
        rescan(library);
    }
}

ScanRequest UnifiedMusicLibrary::refresh(const LibraryInfo& library)
{
    return p->m_threadHandler.refreshLibrary(library);
}

ScanRequest UnifiedMusicLibrary::rescan(const LibraryInfo& library)
{
    return p->m_threadHandler.scanLibrary(library);
}

ScanRequest UnifiedMusicLibrary::scanTracks(const TrackList& tracks)
{
    return p->m_threadHandler.scanTracks(tracks);
}

ScanRequest UnifiedMusicLibrary::scanFiles(const QList<QUrl>& files)
{
    return p->m_threadHandler.scanFiles(files);
}

bool UnifiedMusicLibrary::hasLibrary() const
{
    return p->m_libraryManager->hasLibrary();
}

bool UnifiedMusicLibrary::isEmpty() const
{
    return p->m_tracks.empty();
}

TrackList UnifiedMusicLibrary::tracks() const
{
    return p->m_tracks;
}

Track UnifiedMusicLibrary::trackForId(int id) const
{
    auto trackIt = std::ranges::find_if(p->m_tracks, [id](const Track& track) { return track.id() == id; });
    if(trackIt != p->m_tracks.cend()) {
        return *trackIt;
    }
    return {};
}

TrackList UnifiedMusicLibrary::tracksForIds(const TrackIds& ids) const
{
    TrackList tracks;
    tracks.reserve(ids.size());

    for(const int id : ids) {
        auto trackIt = std::ranges::find_if(p->m_tracks, [id](const Track& track) { return track.id() == id; });
        if(trackIt != p->m_tracks.cend()) {
            tracks.push_back(*trackIt);
        }
    }

    return tracks;
}

void UnifiedMusicLibrary::updateTrackMetadata(const TrackList& tracks)
{
    p->m_threadHandler.saveUpdatedTracks(tracks);
    p->m_threadHandler.saveUpdatedTrackStats(tracks);
}

void UnifiedMusicLibrary::updateTrackStats(const Track& track)
{
    p->m_threadHandler.saveUpdatedTrackStats({track});
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
        p->m_pendingStatUpdates.emplace(hash, updatedTrack);
        isPending = true;
    }

    TrackList tracksToUpdate;
    for(const auto& libraryTrack : p->m_tracks) {
        if(libraryTrack.hash() == hash) {
            Track sameHashTrack{libraryTrack};
            sameHashTrack.setFirstPlayed(currTime);
            sameHashTrack.setLastPlayed(currTime);
            sameHashTrack.setPlayCount(playCount > 0 ? playCount : sameHashTrack.playCount() + 1);

            tracksToUpdate.emplace_back(sameHashTrack);
            if(!isPending) {
                p->m_pendingStatUpdates.emplace(hash, sameHashTrack);
                isPending = true;
            }
        }
    }

    p->updatePlayedTracks(tracksToUpdate);
}

void UnifiedMusicLibrary::cleanupTracks()
{
    p->m_threadHandler.cleanupTracks();
}
} // namespace Fooyin

#include "moc_unifiedmusiclibrary.cpp"
