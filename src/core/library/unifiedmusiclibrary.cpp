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
#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>

#include <QDateTime>

#include <ranges>

using namespace std::chrono_literals;

namespace Fooyin {
class UnifiedMusicLibraryPrivate
{
public:
    UnifiedMusicLibraryPrivate(UnifiedMusicLibrary* self, LibraryManager* libraryManager, DbConnectionPoolPtr dbPool,
                               std::shared_ptr<PlaylistLoader> playlistLoader, std::shared_ptr<TagLoader> tagLoader,
                               SettingsManager* settings);

    void loadTracks(const TrackList& trackToLoad);
    QFuture<void> addTracks(const TrackList& newTracks);
    void updateLibraryTracks(const TrackList& updatedTracks);
    QFuture<void> updateTracks(const TrackList& tracksToUpdate);
    QFuture<void> updatePlayedTracks(const TrackList& tracksToUpdate);

    void handleScanResult(const ScanResult& result);
    void scannedTracks(int id, const TrackList& newTracks, const TrackList& existingTracks);

    void removeLibrary(const LibraryInfo& library, const std::set<int>& tracksRemoved);
    void libraryStatusChanged(const LibraryInfo& library) const;

    void changeSort(const QString& sort);
    QFuture<TrackList> recalSortTracks(const QString& sort, const TrackList& tracks);
    QFuture<TrackList> resortTracks(const TrackList& tracks);

    void handleTracksLoaded();

    UnifiedMusicLibrary* m_self;

    LibraryManager* m_libraryManager;
    DbConnectionPoolPtr m_dbPool;
    SettingsManager* m_settings;

    LibraryThreadHandler m_threadHandler;
    TrackSorter m_sorter;

    TrackList m_tracks;
};

UnifiedMusicLibraryPrivate::UnifiedMusicLibraryPrivate(UnifiedMusicLibrary* self, LibraryManager* libraryManager,
                                                       DbConnectionPoolPtr dbPool,
                                                       std::shared_ptr<PlaylistLoader> playlistLoader,
                                                       std::shared_ptr<TagLoader> tagLoader, SettingsManager* settings)
    : m_self{self}
    , m_libraryManager{libraryManager}
    , m_dbPool{std::move(dbPool)}
    , m_settings{settings}
    , m_threadHandler{m_dbPool, m_self, std::move(playlistLoader), std::move(tagLoader), m_settings}
    , m_sorter{m_libraryManager}
{
    m_settings->subscribe<Settings::Core::LibrarySortScript>(m_self, [this](const QString& sort) { changeSort(sort); });
    m_settings->subscribe<Settings::Core::Internal::MonitorLibraries>(
        m_self, [this](bool enabled) { m_threadHandler.setupWatchers(m_libraryManager->allLibraries(), enabled); });
}

void UnifiedMusicLibraryPrivate::loadTracks(const TrackList& trackToLoad)
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

QFuture<void> UnifiedMusicLibraryPrivate::addTracks(const TrackList& newTracks)
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

QFuture<void> UnifiedMusicLibraryPrivate::updateTracks(const TrackList& tracksToUpdate)
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

QFuture<void> UnifiedMusicLibraryPrivate::updatePlayedTracks(const TrackList& tracksToUpdate)
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

void UnifiedMusicLibraryPrivate::handleScanResult(const ScanResult& result)
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

void UnifiedMusicLibraryPrivate::scannedTracks(int id, const TrackList& newTracks, const TrackList& existingTracks)
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
    emit m_self->tracksUpdated(updatedTracks);
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

QFuture<TrackList> UnifiedMusicLibraryPrivate::recalSortTracks(const QString& sort, const TrackList& tracks)
{
    return Utils::asyncExec([this, sort, tracks]() { return m_sorter.calcSortTracks(sort, tracks); });
}

QFuture<TrackList> UnifiedMusicLibraryPrivate::resortTracks(const TrackList& tracks)
{
    return Utils::asyncExec([tracks]() { return TrackSorter::sortTracks(tracks); });
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
                                         std::shared_ptr<TagLoader> tagLoader, SettingsManager* settings,
                                         QObject* parent)
    : MusicLibrary{parent}
    , p{std::make_unique<UnifiedMusicLibraryPrivate>(this, libraryManager, std::move(dbPool), std::move(playlistLoader),
                                                     std::move(tagLoader), settings)}
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
                     [this](int id, const TrackList& newTracks, const TrackList& existingTracks) {
                         p->scannedTracks(id, newTracks, existingTracks);
                     });
    QObject::connect(&p->m_threadHandler, &LibraryThreadHandler::tracksUpdated, this,
                     [this](const TrackList& tracks) { p->updateTracks(tracks); });
    QObject::connect(&p->m_threadHandler, &LibraryThreadHandler::gotTracks, this,
                     [this](const TrackList& tracks) { p->loadTracks(tracks); });

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
    return p->m_threadHandler.scanTracks(tracks);
}

ScanRequest UnifiedMusicLibrary::scanFiles(const QList<QUrl>& files)
{
    return p->m_threadHandler.scanFiles(files);
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
}

void UnifiedMusicLibrary::writeTrackMetadata(const TrackList& tracks)
{
    p->m_threadHandler.writeUpdatedTracks(tracks);
}

void UnifiedMusicLibrary::updateTrackStats(const TrackList& tracks)
{
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
    const int playCount = track.playCount() + 1;

    TrackList tracksToUpdate;
    for(const auto& libraryTrack : p->m_tracks) {
        if(libraryTrack.hash() == hash) {
            Track sameHashTrack{libraryTrack};
            sameHashTrack.setFirstPlayed(currTime);
            sameHashTrack.setLastPlayed(currTime);
            sameHashTrack.setPlayCount(playCount);

            tracksToUpdate.emplace_back(sameHashTrack);
        }
    }

    p->m_threadHandler.saveUpdatedTrackStats(tracksToUpdate);
    p->updatePlayedTracks(tracksToUpdate);
}

void UnifiedMusicLibrary::cleanupTracks()
{
    p->m_threadHandler.cleanupTracks();
}
} // namespace Fooyin

#include "moc_unifiedmusiclibrary.cpp"
