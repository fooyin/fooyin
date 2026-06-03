/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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
#include <core/network/remoteioservice.h>
#include <core/trackmetadatastore.h>
#include <utils/async.h>
#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>

#include <QCoro/QCoroFuture>
#include <QCoro/QCoroTask>

#include <QDateTime>
#include <QLoggingCategory>
#include <deque>
#include <functional>
#include <optional>
#include <ranges>
#include <unordered_map>
#include <unordered_set>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(LIBRARY, "fy.library")

namespace {
enum class LibraryTrackUpdateType : uint8_t
{
    Availability = 0,
    Stats,
};

int updatedTrackCount(const Fooyin::TrackList& tracks)
{
    return static_cast<int>(std::ranges::count_if(
        tracks, [](const Fooyin::Track& track) { return track.isEnabled() && track.isInLibrary(); }));
}

int removedTrackCount(const Fooyin::TrackList& tracks)
{
    return static_cast<int>(std::ranges::count_if(
        tracks, [](const Fooyin::Track& track) { return !track.isEnabled() || !track.isInLibrary(); }));
}

void logScanSummary(int id, const Fooyin::ScanRequest::Type type, const Fooyin::ScanSummaryCounts& summary)
{
    if(type != Fooyin::ScanRequest::Library || id < 0) {
        return;
    }

    qCInfo(LIBRARY) << "Library scan finished:" << "id=" << id << "added=" << summary.added
                    << "updated=" << summary.updated << "removed=" << summary.removed;
}

Fooyin::Track mergeTrackUpdate(const Fooyin::Track& currentTrack, const Fooyin::Track& updatedTrack,
                               LibraryTrackUpdateType updateType)
{
    Fooyin::Track mergedTrack{currentTrack};

    switch(updateType) {
        case LibraryTrackUpdateType::Availability:
            mergedTrack.setIsEnabled(updatedTrack.isEnabled());
            break;
        case LibraryTrackUpdateType::Stats:
            mergedTrack.setPlayCount(updatedTrack.playCount());
            mergedTrack.setFirstPlayed(updatedTrack.firstPlayed());
            mergedTrack.setLastPlayed(updatedTrack.lastPlayed());
            mergedTrack.setRating(updatedTrack.rating());
            mergedTrack.setModifiedTime(updatedTrack.modifiedTime());
            break;
    }

    return mergedTrack;
}

} // namespace

namespace Fooyin {
class UnifiedMusicLibraryPrivate
{
public:
    using CommitOperation = std::function<QCoro::Task<>()>;

    UnifiedMusicLibraryPrivate(UnifiedMusicLibrary* self, LibraryManager* libraryManager, DbConnectionPoolPtr dbPool,
                               std::shared_ptr<PlaylistLoader> playlistLoader, std::shared_ptr<AudioLoader> audioLoader,
                               std::shared_ptr<RemoteIoService> remoteIo, SettingsManager* settings);

    [[nodiscard]] QString librarySortScript() const;
    [[nodiscard]] QString externalSortScript() const;

    QCoro::Task<TrackList> sortTracks(QString sort, TrackList tracks);
    QFuture<TrackList> sortTracksFuture(const QString& sort, TrackList tracks);
    void attachMetadataStore(TrackList& tracks) const;
    QCoro::Task<> resortLibraryTracks();

    void enqueueCommit(CommitOperation operation);
    void processNextCommit();

    void handleTracksLoaded();
    void loadTracks(TrackList tracksToLoad);
    void changeSort(const QString& sort);

    QCoro::Task<> commitLoadTracks(TrackList tracksToLoad);
    QCoro::Task<> commitChangeSort(QString sort);
    QCoro::Task<> commitAddTracks(TrackList newTracks);
    void updateLibraryTracks(const TrackList& updatedTracks);
    void updateTracksMetadata(TrackList tracksToUpdate);
    void updateTracksAvailability(TrackList tracksToUpdate);
    void updateTracksStats(TrackList tracksToUpdate);
    void updateTracks(TrackList tracksToUpdate);
    QCoro::Task<> commitUpdateTracksMetadata(TrackList tracksToUpdate);
    QCoro::Task<> commitUpdateTracksAvailability(TrackList tracksToUpdate);
    QCoro::Task<> commitUpdateTracksStats(TrackList tracksToUpdate);
    QCoro::Task<> commitUpdateTracks(TrackList tracksToUpdate);
    QCoro::Task<> commitRemoveTracks(TrackList tracksToRemove);
    void removeTracks(const TrackList& tracksToRemove);

    void handleScanResult(int id, ScanRequest::Type type, const ScanResult& result);
    void handleScanFinished(int id, ScanRequest::Type type, bool cancelled);

    QCoro::Task<> commitApplyScanResult(int id, ScanResult result);
    QCoro::Task<> commitPublishScannedTracks(int id, TrackList tracks);
    QCoro::Task<> commitPublishPlaylistTracks(int id, TrackList tracks);
    void scannedTracks(int id, TrackList tracks);
    void playlistLoaded(int id, TrackList tracks);
    [[nodiscard]] TrackList mergeTrackUpdates(const TrackList& tracksToUpdate, LibraryTrackUpdateType updateType) const;

    QCoro::Task<> commitRemoveLibrary(LibraryInfo library, std::set<int> tracksRemoved);
    void removeLibrary(const LibraryInfo& library, const std::set<int>& tracksRemoved);
    void libraryStatusChanged(const LibraryInfo& library) const;

    UnifiedMusicLibrary* m_self;

    LibraryManager* m_libraryManager;
    DbConnectionPoolPtr m_dbPool;
    SettingsManager* m_settings;
    std::shared_ptr<TrackMetadataStore> m_metadataStore;

    LibraryThreadHandler m_threadHandler;
    TrackSorter m_sorter;

    TrackList m_tracks;
    std::deque<CommitOperation> m_commitQueue;
    std::optional<QCoro::Task<>> m_activeCommitTask;
    std::unordered_map<int, ScanSummaryCounts> m_scanSummaries;
};

UnifiedMusicLibraryPrivate::UnifiedMusicLibraryPrivate(UnifiedMusicLibrary* self, LibraryManager* libraryManager,
                                                       DbConnectionPoolPtr dbPool,
                                                       std::shared_ptr<PlaylistLoader> playlistLoader,
                                                       std::shared_ptr<AudioLoader> audioLoader,
                                                       std::shared_ptr<RemoteIoService> remoteIo,
                                                       SettingsManager* settings)
    : m_self{self}
    , m_libraryManager{libraryManager}
    , m_dbPool{std::move(dbPool)}
    , m_settings{settings}
    , m_metadataStore{std::make_shared<TrackMetadataStore>()}
    , m_threadHandler{m_dbPool,
                      m_self,
                      std::move(playlistLoader),
                      m_metadataStore,
                      std::move(audioLoader),
                      std::move(remoteIo),
                      m_settings}
    , m_sorter{m_libraryManager}
{
    m_settings->subscribe<Settings::Core::LibrarySortScript>(m_self, [this](const QString& sort) { changeSort(sort); });
    m_settings->subscribe<Settings::Core::Internal::MonitorLibraries>(
        m_self, [this](bool enabled) { m_threadHandler.setupWatchers(m_libraryManager->allLibraries(), enabled); });
}

QString UnifiedMusicLibraryPrivate::librarySortScript() const
{
    return m_settings->value<Settings::Core::LibrarySortScript>();
}

QString UnifiedMusicLibraryPrivate::externalSortScript() const
{
    return m_settings->value<Settings::Core::ExternalSortScript>();
}

QCoro::Task<TrackList> UnifiedMusicLibraryPrivate::sortTracks(QString sort, TrackList tracks)
{
    co_return co_await sortTracksFuture(sort, std::move(tracks));
}

QFuture<TrackList> UnifiedMusicLibraryPrivate::sortTracksFuture(const QString& sort, TrackList tracks)
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

QCoro::Task<> UnifiedMusicLibraryPrivate::resortLibraryTracks()
{
    m_tracks = co_await sortTracks(librarySortScript(), std::move(m_tracks));
}

void UnifiedMusicLibraryPrivate::enqueueCommit(CommitOperation operation)
{
    m_commitQueue.push_back(std::move(operation));
    processNextCommit();
}

void UnifiedMusicLibraryPrivate::processNextCommit()
{
    if(m_activeCommitTask || m_commitQueue.empty()) {
        return;
    }

    auto operation = std::move(m_commitQueue.front());
    m_commitQueue.pop_front();

    m_activeCommitTask = operation().then([this]() {
        QMetaObject::invokeMethod(
            m_self,
            [this]() {
                m_activeCommitTask.reset();
                processNextCommit();
            },
            Qt::QueuedConnection);
    });
}

void UnifiedMusicLibraryPrivate::handleTracksLoaded()
{
    const bool autoRefresh = m_settings->value<Settings::Core::AutoRefresh>();

    if(autoRefresh) {
        m_self->refreshAll();
    }

    m_threadHandler.setupWatchers(m_libraryManager->allLibraries(),
                                  m_settings->value<Settings::Core::Internal::MonitorLibraries>());

    if(m_settings->fileValue(Settings::Core::Internal::MarkUnavailableStartup, false).toBool() && !m_tracks.empty()) {
        if(autoRefresh) {
            // The library scan will take care of any library-backed tracks
            TrackList unmanagedTracks;
            unmanagedTracks.reserve(m_tracks.size());

            for(const auto& track : m_tracks) {
                if(!track.isInLibrary()) {
                    unmanagedTracks.push_back(track);
                }
            }

            if(!unmanagedTracks.empty()) {
                m_threadHandler.checkTrackAvailability(unmanagedTracks);
            }
        }
        else {
            m_threadHandler.checkTrackAvailability(m_tracks);
        }
    }
}

void UnifiedMusicLibraryPrivate::loadTracks(TrackList tracksToLoad)
{
    enqueueCommit(
        [this, tracksToLoad = std::move(tracksToLoad)]() mutable { return commitLoadTracks(std::move(tracksToLoad)); });
}

void UnifiedMusicLibraryPrivate::changeSort(const QString& sort)
{
    enqueueCommit([this, sort]() mutable { return commitChangeSort(std::move(sort)); });
}

QCoro::Task<> UnifiedMusicLibraryPrivate::commitLoadTracks(TrackList tracksToLoad)
{
    if(tracksToLoad.empty()) {
        m_tracks.clear();
        Q_EMIT m_self->tracksLoaded({});
        co_return;
    }

    attachMetadataStore(tracksToLoad);
    m_tracks = co_await sortTracks(librarySortScript(), std::move(tracksToLoad));
    Q_EMIT m_self->tracksLoaded(m_tracks);
}

QCoro::Task<> UnifiedMusicLibraryPrivate::commitChangeSort(QString sort)
{
    m_tracks = co_await sortTracks(std::move(sort), std::move(m_tracks));
    Q_EMIT m_self->tracksSorted(m_tracks);
}

QCoro::Task<> UnifiedMusicLibraryPrivate::commitAddTracks(TrackList newTracks)
{
    attachMetadataStore(newTracks);

    const TrackList sortedTracks = co_await sortTracks(librarySortScript(), std::move(newTracks));

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
    TrackList updatedTracks;
    updatedTracks.reserve(sortedTracks.size());

    for(const Track& track : sortedTracks) {
        if(auto it = findExistingTrack(track); it != m_tracks.end()) {
            *it = track;
            it->clearWasModified();
            updatedTracks.emplace_back(track);
        }
        else {
            addedTracks.push_back(track);
            m_tracks.push_back(track);
        }
    }

    co_await resortLibraryTracks();

    if(!updatedTracks.empty()) {
        Q_EMIT m_self->tracksMetadataChanged(updatedTracks);
    }
    if(!addedTracks.empty()) {
        Q_EMIT m_self->tracksAdded(addedTracks);
    }
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

void UnifiedMusicLibraryPrivate::updateTracksMetadata(TrackList tracksToUpdate)
{
    enqueueCommit([this, tracksToUpdate = std::move(tracksToUpdate)]() mutable {
        return commitUpdateTracksMetadata(std::move(tracksToUpdate));
    });
}

void UnifiedMusicLibraryPrivate::updateTracksAvailability(TrackList tracksToUpdate)
{
    enqueueCommit([this, tracksToUpdate = std::move(tracksToUpdate)]() mutable {
        return commitUpdateTracksAvailability(std::move(tracksToUpdate));
    });
}

void UnifiedMusicLibraryPrivate::updateTracksStats(TrackList tracksToUpdate)
{
    enqueueCommit([this, tracksToUpdate = std::move(tracksToUpdate)]() mutable {
        return commitUpdateTracksStats(std::move(tracksToUpdate));
    });
}

void UnifiedMusicLibraryPrivate::updateTracks(TrackList tracksToUpdate)
{
    enqueueCommit([this, tracksToUpdate = std::move(tracksToUpdate)]() mutable {
        return commitUpdateTracks(std::move(tracksToUpdate));
    });
}

QCoro::Task<> UnifiedMusicLibraryPrivate::commitUpdateTracksMetadata(TrackList tracksToUpdate)
{
    if(tracksToUpdate.empty()) {
        Q_EMIT m_self->tracksMetadataChanged({});
        co_return;
    }

    attachMetadataStore(tracksToUpdate);

    const TrackList sortedTracks = co_await sortTracks(librarySortScript(), std::move(tracksToUpdate));

    updateLibraryTracks(sortedTracks);
    co_await resortLibraryTracks();
    Q_EMIT m_self->tracksMetadataChanged(sortedTracks);
}

TrackList UnifiedMusicLibraryPrivate::mergeTrackUpdates(const TrackList& tracksToUpdate,
                                                        LibraryTrackUpdateType updateType) const
{
    TrackList mergedTracks;
    mergedTracks.reserve(tracksToUpdate.size());

    for(const auto& track : tracksToUpdate) {
        auto trackIt
            = std::ranges::find_if(m_tracks, [&track](const Track& oldTrack) { return oldTrack.id() == track.id(); });
        if(trackIt != m_tracks.end()) {
            mergedTracks.emplace_back(mergeTrackUpdate(*trackIt, track, updateType));
        }
        else {
            mergedTracks.emplace_back(track);
        }
    }

    return mergedTracks;
}

QCoro::Task<> UnifiedMusicLibraryPrivate::commitUpdateTracksAvailability(TrackList tracksToUpdate)
{
    attachMetadataStore(tracksToUpdate);
    TrackList mergedTracks = mergeTrackUpdates(tracksToUpdate, LibraryTrackUpdateType::Availability);

    const TrackList sortedTracks = co_await sortTracks(librarySortScript(), std::move(mergedTracks));

    updateLibraryTracks(sortedTracks);
    co_await resortLibraryTracks();
    Q_EMIT m_self->tracksUpdated(sortedTracks);
}

QCoro::Task<> UnifiedMusicLibraryPrivate::commitUpdateTracksStats(TrackList tracksToUpdate)
{
    attachMetadataStore(tracksToUpdate);
    TrackList mergedTracks = mergeTrackUpdates(tracksToUpdate, LibraryTrackUpdateType::Stats);

    const TrackList sortedTracks = co_await sortTracks(librarySortScript(), std::move(mergedTracks));

    updateLibraryTracks(sortedTracks);
    co_await resortLibraryTracks();
    Q_EMIT m_self->tracksUpdated(sortedTracks);
}

QCoro::Task<> UnifiedMusicLibraryPrivate::commitUpdateTracks(TrackList tracksToUpdate)
{
    attachMetadataStore(tracksToUpdate);

    const TrackList sortedTracks = co_await sortTracks(librarySortScript(), std::move(tracksToUpdate));

    updateLibraryTracks(sortedTracks);
    co_await resortLibraryTracks();
    Q_EMIT m_self->tracksUpdated(sortedTracks);
}

QCoro::Task<> UnifiedMusicLibraryPrivate::commitRemoveTracks(TrackList tracksToRemove)
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

    Q_EMIT m_self->tracksDeleted(tracksToRemove);
    co_return;
}

void UnifiedMusicLibraryPrivate::removeTracks(const TrackList& tracksToRemove)
{
    enqueueCommit([this, tracksToRemove]() { return commitRemoveTracks(tracksToRemove); });
}

void UnifiedMusicLibraryPrivate::handleScanResult(int id, const ScanRequest::Type type, const ScanResult& result)
{
    if(id < 0 || (type != ScanRequest::Library && type != ScanRequest::Tracks)) {
        return;
    }

    auto& summary = m_scanSummaries[id];
    summary.added += static_cast<int>(result.addedTracks.size());
    summary.updated += updatedTrackCount(result.updatedTracks);
    summary.removed += removedTrackCount(result.updatedTracks);

    enqueueCommit([this, id, result]() mutable { return commitApplyScanResult(id, std::move(result)); });
}

QCoro::Task<> UnifiedMusicLibraryPrivate::commitApplyScanResult(int id, ScanResult result)
{
    if(!result.addedTracks.empty()) {
        co_await commitAddTracks(std::move(result.addedTracks));
    }
    if(!result.updatedTracks.empty()) {
        co_await commitUpdateTracksMetadata(std::move(result.updatedTracks));
    }

    Q_EMIT m_self->scanApplyCompleted(id);
}

QCoro::Task<> UnifiedMusicLibraryPrivate::commitPublishScannedTracks(int id, TrackList tracks)
{
    TrackList scannedTracks{tracks};

    co_await commitAddTracks(std::move(tracks));
    const TrackList sortedScannedTracks = co_await sortTracks(externalSortScript(), std::move(scannedTracks));
    Q_EMIT m_self->tracksScanned(id, sortedScannedTracks);

    m_threadHandler.acknowledgeTracksScanned(id);
}

QCoro::Task<> UnifiedMusicLibraryPrivate::commitPublishPlaylistTracks(int id, TrackList tracks)
{
    const TrackList playlistTracks{tracks};

    co_await commitAddTracks(std::move(tracks));
    Q_EMIT m_self->tracksScanned(id, playlistTracks);

    m_threadHandler.acknowledgeTracksScanned(id);
}

void UnifiedMusicLibraryPrivate::handleScanFinished(int id, const ScanRequest::Type type, const bool cancelled)
{
    const auto summary = m_scanSummaries.contains(id) ? m_scanSummaries.at(id) : ScanSummaryCounts{};
    m_scanSummaries.erase(id);

    if(!cancelled) {
        logScanSummary(id, type, summary);
        Q_EMIT m_self->scanSummary(id, type, summary);
    }
    Q_EMIT m_self->scanFinished(id, type, cancelled);
}

void UnifiedMusicLibraryPrivate::scannedTracks(int id, TrackList tracks)
{
    enqueueCommit(
        [this, id, tracks = std::move(tracks)]() mutable { return commitPublishScannedTracks(id, std::move(tracks)); });
}

void UnifiedMusicLibraryPrivate::playlistLoaded(int id, TrackList tracks)
{
    enqueueCommit([this, id, tracks = std::move(tracks)]() mutable {
        return commitPublishPlaylistTracks(id, std::move(tracks));
    });
}

QCoro::Task<> UnifiedMusicLibraryPrivate::commitRemoveLibrary(LibraryInfo library, std::set<int> tracksRemoved)
{
    if(library.id < 0) {
        co_return;
    }

    TrackList removedTracks;
    TrackList updatedTracks;
    TrackList remainingTracks;

    remainingTracks.reserve(m_tracks.size());
    removedTracks.reserve(tracksRemoved.size());

    for(auto& track : m_tracks) {
        const bool isInRemovedLibrary = track.libraryId() == library.id;

        if(isInRemovedLibrary && tracksRemoved.contains(track.id())) {
            removedTracks.push_back(track);
            continue;
        }

        if(isInRemovedLibrary) {
            track.setLibraryId(-1);
            updatedTracks.push_back(track);
        }

        remainingTracks.push_back(track);
    }

    m_tracks = std::move(remainingTracks);

    if(!removedTracks.empty()) {
        Q_EMIT m_self->tracksDeleted(removedTracks);
    }
    if(!updatedTracks.empty()) {
        Q_EMIT m_self->tracksMetadataChanged(updatedTracks);
    }

    co_return;
}

void UnifiedMusicLibraryPrivate::removeLibrary(const LibraryInfo& library, const std::set<int>& tracksRemoved)
{
    enqueueCommit([this, library, tracksRemoved]() mutable {
        return commitRemoveLibrary(std::move(library), std::move(tracksRemoved));
    });
}

void UnifiedMusicLibraryPrivate::libraryStatusChanged(const LibraryInfo& library) const
{
    m_libraryManager->updateLibraryStatus(library);
}

UnifiedMusicLibrary::UnifiedMusicLibrary(LibraryManager* libraryManager, DbConnectionPoolPtr dbPool,
                                         std::shared_ptr<PlaylistLoader> playlistLoader,
                                         std::shared_ptr<AudioLoader> audioLoader,
                                         std::shared_ptr<RemoteIoService> remoteIo, SettingsManager* settings,
                                         QObject* parent)
    : MusicLibrary{parent}
    , p{std::make_unique<UnifiedMusicLibraryPrivate>(this, libraryManager, std::move(dbPool), std::move(playlistLoader),
                                                     std::move(audioLoader), std::move(remoteIo), settings)}
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
    QObject::connect(
        &p->m_threadHandler, &LibraryThreadHandler::scanFinished, this,
        [this](int id, const ScanRequest::Type type, bool cancelled) { p->handleScanFinished(id, type, cancelled); });
    QObject::connect(this, &MusicLibrary::scanApplyCompleted, &p->m_threadHandler,
                     &LibraryThreadHandler::acknowledgeScanResultApplied);

    QObject::connect(&p->m_threadHandler, &LibraryThreadHandler::statusChanged, this,
                     [this](const LibraryInfo& library) { p->libraryStatusChanged(library); });
    QObject::connect(&p->m_threadHandler, &LibraryThreadHandler::scanUpdate, this,
                     [this](int id, const ScanRequest::Type type, const ScanResult& result) {
                         p->handleScanResult(id, type, result);
                     });
    QObject::connect(&p->m_threadHandler, &LibraryThreadHandler::scannedTracks, this,
                     [this](int id, TrackList tracks) { p->scannedTracks(id, std::move(tracks)); });
    QObject::connect(&p->m_threadHandler, &LibraryThreadHandler::playlistLoaded, this,
                     [this](int id, TrackList tracks) { p->playlistLoaded(id, std::move(tracks)); });
    QObject::connect(&p->m_threadHandler, &LibraryThreadHandler::tracksUpdated, this,
                     [this](const TrackList& tracks) { p->updateTracksMetadata(tracks); });
    QObject::connect(&p->m_threadHandler, &LibraryThreadHandler::tracksAvailabilityUpdated, this,
                     [this](const TrackList& tracks) { p->updateTracksAvailability(tracks); });
    QObject::connect(&p->m_threadHandler, &LibraryThreadHandler::tracksStatsUpdated, this,
                     [this](const TrackList& tracks) { p->updateTracksStats(tracks); });
    QObject::connect(&p->m_threadHandler, &LibraryThreadHandler::tracksRemoved, this,
                     [this](const TrackList& tracks) { p->removeTracks(tracks); });
    QObject::connect(&p->m_threadHandler, &LibraryThreadHandler::gotTracks, this,
                     [this](TrackList tracks) { p->loadTracks(std::move(tracks)); });

    QObject::connect(this, &MusicLibrary::tracksLoaded, this, [this]() { p->handleTracksLoaded(); });
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
        if(!p->m_threadHandler.hasPendingLibraryScan(library.id)) {
            refresh(library);
        }
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

void UnifiedMusicLibrary::cancelScan(int id)
{
    p->m_threadHandler.cancelScan(id);
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

TrackList UnifiedMusicLibrary::libraryTracks() const
{
    TrackList tracks;
    tracks.reserve(p->m_tracks.size());

    for(const Track& track : p->m_tracks) {
        if(track.isInLibrary()) {
            tracks.emplace_back(track);
        }
    }

    return tracks;
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

WriteRequest UnifiedMusicLibrary::deleteTracks(const TrackList& tracks)
{
    return p->m_threadHandler.deleteTracks(tracks);
}
} // namespace Fooyin

#include "moc_unifiedmusiclibrary.cpp"
