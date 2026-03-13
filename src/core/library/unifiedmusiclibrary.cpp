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
#include "library/librarymanager.h"
#include "librarythreadhandler.h"

#include <core/coresettings.h>
#include <core/library/libraryinfo.h>
#include <core/library/tracksort.h>
#include <core/trackmetadatastore.h>
#include <utils/async.h>
#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>

#include <QDateTime>
#include <QLoggingCategory>

#include <ranges>
#include <unordered_set>

using namespace std::chrono_literals;

Q_LOGGING_CATEGORY(LIBRARY, "fy.library")

namespace Fooyin {
class UnifiedMusicLibraryPrivate
{
public:
    UnifiedMusicLibraryPrivate(UnifiedMusicLibrary* self, LibraryManager* libraryManager, DbConnectionPoolPtr dbPool,
                               std::shared_ptr<PlaylistLoader> playlistLoader, std::shared_ptr<AudioLoader> audioLoader,
                               SettingsManager* settings);

    void loadTracks(TrackList tracksToLoad);
    QFuture<void> addTracks(TrackList newTracks);
    void updateLibraryTracks(const TrackList& updatedTracks);
    QFuture<void> updateTracksMetadata(TrackList tracksToUpdate);
    QFuture<void> updateTracks(TrackList tracksToUpdate);
    void removeTracks(const TrackList& tracksToRemove);

    void handleScanResult(const ScanResult& result);
    void scannedTracks(int id, TrackList tracks);
    void playlistLoaded(int id, TrackList tracks);

    void removeLibrary(const LibraryInfo& library, const std::set<int>& tracksRemoved);
    void libraryStatusChanged(const LibraryInfo& library) const;

    void changeSort(const QString& sort);
    QFuture<TrackList> recalSortTracks(const QString& sort, TrackList tracks);
    void attachMetadataStore(TrackList& tracks) const;

    void handleTracksLoaded();

    UnifiedMusicLibrary* m_self;

    LibraryManager* m_libraryManager;
    DbConnectionPoolPtr m_dbPool;
    SettingsManager* m_settings;
    std::shared_ptr<TrackMetadataStore> m_metadataStore;

    LibraryThreadHandler m_threadHandler;
    TrackSorter m_sorter;

    TrackList m_tracks;
};

UnifiedMusicLibraryPrivate::UnifiedMusicLibraryPrivate(UnifiedMusicLibrary* self, LibraryManager* libraryManager,
                                                       DbConnectionPoolPtr dbPool,
                                                       std::shared_ptr<PlaylistLoader> playlistLoader,
                                                       std::shared_ptr<AudioLoader> audioLoader,
                                                       SettingsManager* settings)
    : m_self{self}
    , m_libraryManager{libraryManager}
    , m_dbPool{std::move(dbPool)}
    , m_settings{settings}
    , m_metadataStore{std::make_shared<TrackMetadataStore>()}
    , m_threadHandler{m_dbPool, m_self, std::move(playlistLoader), m_metadataStore, std::move(audioLoader), m_settings}
    , m_sorter{m_libraryManager}
{
    m_settings->subscribe<Settings::Core::LibrarySortScript>(m_self, [this](const QString& sort) { changeSort(sort); });
    m_settings->subscribe<Settings::Core::Internal::MonitorLibraries>(
        m_self, [this](bool enabled) { m_threadHandler.setupWatchers(m_libraryManager->allLibraries(), enabled); });
}

void UnifiedMusicLibraryPrivate::loadTracks(TrackList tracksToLoad)
{
    if(tracksToLoad.empty()) {
        emit m_self->tracksLoaded({});
        return;
    }

    attachMetadataStore(tracksToLoad);
    auto sortTracks = recalSortTracks(m_settings->value<Settings::Core::LibrarySortScript>(), std::move(tracksToLoad));

    sortTracks.then(m_self, [this](TrackList sortedTracks) {
        m_tracks = std::move(sortedTracks);
        emit m_self->tracksLoaded(m_tracks);
    });
}

QFuture<void> UnifiedMusicLibraryPrivate::addTracks(TrackList newTracks)
{
    attachMetadataStore(newTracks);
    auto sortTracks = recalSortTracks(m_settings->value<Settings::Core::LibrarySortScript>(), std::move(newTracks));

    return sortTracks.then(m_self, [this](TrackList sortedTracks) {
        const auto findExistingTrack = [this](const Track& track) {
            return std::ranges::find_if(m_tracks, [&track](const Track& existingTrack) {
                if(track.id() >= 0 && existingTrack.id() >= 0) {
                    return existingTrack.id() == track.id();
                }

                return existingTrack.uniqueFilepath() == track.uniqueFilepath()
                    && existingTrack.subsong() == track.subsong();
            });
        };

        TrackList addedTracks;
        addedTracks.reserve(sortedTracks.size());

        for(const Track& track : sortedTracks) {
            if(auto it = findExistingTrack(track); it != m_tracks.end()) {
                *it = track;
                it->clearWasModified();
            }
            else {
                addedTracks.push_back(track);
                m_tracks.push_back(track);
            }
        }

        recalSortTracks(m_settings->value<Settings::Core::LibrarySortScript>(), m_tracks)
            .then(m_self, [this, addedTracks = std::move(addedTracks)](TrackList sortedLibraryTracks) mutable {
                m_tracks = std::move(sortedLibraryTracks);

                if(!addedTracks.empty()) {
                    emit m_self->tracksAdded(addedTracks);
                }
            });
    });
}

void UnifiedMusicLibraryPrivate::updateLibraryTracks(const TrackList& updatedTracks)
{
    for(const auto& track : updatedTracks) {
        auto trackIt
            = std::ranges::find_if(m_tracks, [&track](const Track& oldTrack) { return oldTrack.id() == track.id(); });
        if(trackIt != m_tracks.end()) {
            *trackIt = track;
            trackIt->clearWasModified();
        }
    }
}

QFuture<void> UnifiedMusicLibraryPrivate::updateTracksMetadata(TrackList tracksToUpdate)
{
    if(tracksToUpdate.empty()) {
        emit m_self->tracksMetadataChanged({});
        return {};
    }

    attachMetadataStore(tracksToUpdate);
    auto sortTracks
        = recalSortTracks(m_settings->value<Settings::Core::LibrarySortScript>(), std::move(tracksToUpdate));

    return sortTracks.then(m_self, [this](TrackList sortedTracks) {
        updateLibraryTracks(sortedTracks);

        recalSortTracks(m_settings->value<Settings::Core::LibrarySortScript>(), m_tracks)
            .then(m_self, [this, sortedTracks = std::move(sortedTracks)](TrackList sortedLibraryTracks) mutable {
                m_tracks = std::move(sortedLibraryTracks);
                emit m_self->tracksMetadataChanged(sortedTracks);
            });
    });
}

QFuture<void> UnifiedMusicLibraryPrivate::updateTracks(TrackList tracksToUpdate)
{
    attachMetadataStore(tracksToUpdate);
    auto sortTracks
        = recalSortTracks(m_settings->value<Settings::Core::LibrarySortScript>(), std::move(tracksToUpdate));

    return sortTracks.then(m_self, [this](TrackList sortedTracks) {
        updateLibraryTracks(sortedTracks);

        recalSortTracks(m_settings->value<Settings::Core::LibrarySortScript>(), m_tracks)
            .then(m_self, [this, sortedTracks = std::move(sortedTracks)](TrackList sortedLibraryTracks) mutable {
                m_tracks = std::move(sortedLibraryTracks);
                emit m_self->tracksUpdated(sortedTracks);
            });
    });
}

void UnifiedMusicLibraryPrivate::removeTracks(const TrackList& tracksToRemove)
{
    const std::unordered_set<Track, Track::TrackHash> toRemove(tracksToRemove.begin(), tracksToRemove.end());

    TrackList remainingTracks;
    remainingTracks.reserve(m_tracks.size());

    for(const auto& track : m_tracks) {
        if(!toRemove.contains(track)) {
            remainingTracks.push_back(track);
        }
    }

    m_tracks = std::move(remainingTracks);

    emit m_self->tracksDeleted(tracksToRemove);
}

void UnifiedMusicLibraryPrivate::handleScanResult(const ScanResult& result)
{
    if(!result.addedTracks.empty()) {
        addTracks(result.addedTracks).then(m_self, [this, result]() {
            if(!result.updatedTracks.empty()) {
                updateTracksMetadata(result.updatedTracks);
            }
        });
    }
    else if(!result.updatedTracks.empty()) {
        updateTracksMetadata(result.updatedTracks);
    }
}

void UnifiedMusicLibraryPrivate::scannedTracks(int id, TrackList tracks)
{
    TrackList scannedTracks{tracks};

    addTracks(std::move(tracks)).then([this, id, scannedTracks = std::move(scannedTracks)]() mutable {
        recalSortTracks(m_settings->value<Settings::Core::ExternalSortScript>(), std::move(scannedTracks))
            .then(m_self,
                  [this, id](TrackList sortedScannedTracks) { emit m_self->tracksScanned(id, sortedScannedTracks); });
    });
}

void UnifiedMusicLibraryPrivate::playlistLoaded(int id, TrackList tracks)
{
    TrackList playlistTracks{tracks};

    addTracks(std::move(tracks)).then(m_self, [this, id, playlistTracks = std::move(playlistTracks)]() mutable {
        emit m_self->tracksScanned(id, playlistTracks);
    });
}

void UnifiedMusicLibraryPrivate::removeLibrary(const LibraryInfo& library, const std::set<int>& tracksRemoved)
{
    if(library.id < 0) {
        return;
    }

    TrackList newTracks;
    TrackList removedTracks;
    TrackList updatedTracks;

    for(auto& track : m_tracks) {
        if(track.libraryId() == library.id) {
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

    emit m_self->tracksDeleted(removedTracks);
    emit m_self->tracksMetadataChanged(updatedTracks);
}

void UnifiedMusicLibraryPrivate::libraryStatusChanged(const LibraryInfo& library) const
{
    m_libraryManager->updateLibraryStatus(library);
}

void UnifiedMusicLibraryPrivate::changeSort(const QString& sort)
{
    recalSortTracks(sort, m_tracks).then(m_self, [this](const TrackList& sortedTracks) {
        m_tracks = sortedTracks;
        emit m_self->tracksSorted(m_tracks);
    });
}

QFuture<TrackList> UnifiedMusicLibraryPrivate::recalSortTracks(const QString& sort, TrackList tracks)
{
    return Utils::asyncExec([this, sort, tracks = std::move(tracks)]() mutable {
        return m_sorter.calcSortTracks(sort, std::move(tracks));
    });
}

void UnifiedMusicLibraryPrivate::attachMetadataStore(TrackList& tracks) const
{
    for(auto& track : tracks) {
        track.setMetadataStore(m_metadataStore);
    }
}

void UnifiedMusicLibraryPrivate::handleTracksLoaded()
{
    m_threadHandler.setupWatchers(m_libraryManager->allLibraries(),
                                  m_settings->value<Settings::Core::Internal::MonitorLibraries>());
    if(m_settings->value<Settings::Core::AutoRefresh>()) {
        m_self->refreshAll();
    }
}

UnifiedMusicLibrary::UnifiedMusicLibrary(LibraryManager* libraryManager, DbConnectionPoolPtr dbPool,
                                         std::shared_ptr<PlaylistLoader> playlistLoader,
                                         std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings,
                                         QObject* parent)
    : MusicLibrary{parent}
    , p{std::make_unique<UnifiedMusicLibraryPrivate>(this, libraryManager, std::move(dbPool), std::move(playlistLoader),
                                                     std::move(audioLoader), settings)}
{
    QObject::connect(p->m_libraryManager, &LibraryManager::libraryAdded, this, &MusicLibrary::rescan);
    QObject::connect(p->m_libraryManager, &LibraryManager::libraryAboutToBeRemoved, this,
                     [this](const LibraryInfo& library) { p->m_threadHandler.libraryRemoved(library.id); });
    QObject::connect(p->m_libraryManager, &LibraryManager::libraryRemoved, this,
                     [this](const LibraryInfo& library, const std::set<int>& tracksRemoved) {
                         p->removeLibrary(library, tracksRemoved);
                     });

    QObject::connect(&p->m_threadHandler, &LibraryThreadHandler::progressChanged, this,
                     &UnifiedMusicLibrary::scanProgress);

    QObject::connect(&p->m_threadHandler, &LibraryThreadHandler::statusChanged, this,
                     [this](const LibraryInfo& library) { p->libraryStatusChanged(library); });
    QObject::connect(&p->m_threadHandler, &LibraryThreadHandler::scanUpdate, this,
                     [this](const ScanResult& result) { p->handleScanResult(result); });
    QObject::connect(&p->m_threadHandler, &LibraryThreadHandler::scannedTracks, this,
                     [this](int id, TrackList tracks) { p->scannedTracks(id, std::move(tracks)); });
    QObject::connect(&p->m_threadHandler, &LibraryThreadHandler::playlistLoaded, this,
                     [this](int id, TrackList tracks) { p->playlistLoaded(id, std::move(tracks)); });
    QObject::connect(&p->m_threadHandler, &LibraryThreadHandler::tracksUpdated, this,
                     [this](const TrackList& tracks) { p->updateTracksMetadata(tracks); });
    QObject::connect(&p->m_threadHandler, &LibraryThreadHandler::tracksStatsUpdated, this,
                     [this](const TrackList& tracks) { p->updateTracks(tracks); });
    QObject::connect(&p->m_threadHandler, &LibraryThreadHandler::tracksRemoved, this,
                     [this](const TrackList& tracks) { p->removeTracks(tracks); });
    QObject::connect(&p->m_threadHandler, &LibraryThreadHandler::gotTracks, this,
                     [this](TrackList tracks) { p->loadTracks(std::move(tracks)); });

    QObject::connect(
        this, &MusicLibrary::tracksLoaded, this, [this]() { p->handleTracksLoaded(); }, Qt::QueuedConnection);
}

UnifiedMusicLibrary::~UnifiedMusicLibrary() = default;

bool UnifiedMusicLibrary::hasLibrary() const
{
    return p->m_libraryManager->hasLibrary();
}

std::optional<LibraryInfo> UnifiedMusicLibrary::libraryInfo(int id) const
{
    return p->m_libraryManager->libraryInfo(id);
}

std::optional<LibraryInfo> UnifiedMusicLibrary::libraryForPath(const QString& path) const
{
    const LibraryInfoMap& libraries = p->m_libraryManager->allLibraries();
    for(const auto& library : libraries | std::views::values) {
        if(Utils::File::isSubdir(path, library.path)) {
            return library;
        }
    }
    return {};
}

void UnifiedMusicLibrary::loadAllTracks()
{
    p->m_threadHandler.getAllTracks();
}

bool UnifiedMusicLibrary::isEmpty() const
{
    return p->m_tracks.empty();
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
    return p->m_threadHandler.scanTracks(tracks, false);
}

ScanRequest UnifiedMusicLibrary::scanModifiedTracks(const TrackList& tracks)
{
    return p->m_threadHandler.scanTracks(tracks, true);
}

ScanRequest UnifiedMusicLibrary::scanFiles(const QList<QUrl>& files)
{
    return p->m_threadHandler.scanFiles(files);
}

ScanRequest UnifiedMusicLibrary::loadPlaylist(const QList<QUrl>& files)
{
    return p->m_threadHandler.loadPlaylist(files);
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

std::shared_ptr<TrackMetadataStore> UnifiedMusicLibrary::metadataStore() const
{
    return p->m_metadataStore;
}

void UnifiedMusicLibrary::updateTrack(const Track& track)
{
    updateTracks({track});
}

void UnifiedMusicLibrary::updateTracks(const TrackList& tracks)
{
    p->updateTracks(tracks);
}

void UnifiedMusicLibrary::updateTrackMetadata(const TrackList& tracks)
{
    p->m_threadHandler.saveUpdatedTracks(tracks);
}

WriteRequest UnifiedMusicLibrary::writeTrackMetadata(const TrackList& tracks)
{
    return p->m_threadHandler.writeUpdatedTracks(tracks);
}

WriteRequest UnifiedMusicLibrary::writeTrackCovers(const TrackCoverData& tracks)
{
    return p->m_threadHandler.writeTrackCovers(tracks);
}

void UnifiedMusicLibrary::updateTrackStats(const TrackList& tracks)
{
    p->m_threadHandler.saveUpdatedTrackStats(tracks);
}

void UnifiedMusicLibrary::updateTrackStats(const Track& track)
{
    updateTrackStats(TrackList{track});
}

void UnifiedMusicLibrary::trackWasPlayed(const Track& track)
{
    const QString hash  = track.hash();
    const auto currTime = QDateTime::currentMSecsSinceEpoch();
    const int playCount = track.playCount() + 1;

    qCDebug(LIBRARY) << "Track played event received:" << "id=" << track.id() << "path=" << track.uniqueFilepath()
                     << "hash=" << hash << "currentPlayCount=" << track.playCount() << "nextPlayCount=" << playCount;

    TrackList tracksToUpdate;
    for(const auto& libraryTrack : p->m_tracks) {
        if(libraryTrack.hash() == hash) {
            Track sameHashTrack{libraryTrack};
            sameHashTrack.setFirstPlayed(currTime);
            sameHashTrack.setLastPlayed(currTime);
            sameHashTrack.setPlayCount(playCount);

            qCDebug(LIBRARY) << "Queueing playcount update:" << "id=" << sameHashTrack.id()
                             << "path=" << sameHashTrack.uniqueFilepath()
                             << "libraryPlayCount=" << libraryTrack.playCount()
                             << "queuedPlayCount=" << sameHashTrack.playCount();

            tracksToUpdate.emplace_back(sameHashTrack);
        }
    }

    p->m_threadHandler.saveUpdatedTrackPlaycounts(tracksToUpdate);
}

void UnifiedMusicLibrary::cleanupTracks()
{
    p->m_threadHandler.cleanupTracks();
}

WriteRequest UnifiedMusicLibrary::removeUnavailbleTracks()
{
    return p->m_threadHandler.removeUnavailbleTracks(p->m_tracks);
}
} // namespace Fooyin

#include "moc_unifiedmusiclibrary.cpp"
