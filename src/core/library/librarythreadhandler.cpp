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

#include "librarythreadhandler.h"

#include "internalcoresettings.h"
#include "librarymonitor.h"
#include "libraryscanner.h"
#include "libraryscanutils.h"
#include "trackdatabasemanager.h"

#include <core/coresettings.h>
#include <core/engine/audioinput.h>
#include <core/library/musiclibrary.h>
#include <core/trackmetadatastore.h>
#include <utils/settings/settingsmanager.h>

#include <QBasicTimer>
#include <QLoggingCategory>
#include <QPromise>
#include <QThread>
#include <QTimerEvent>
#include <QUrl>

#include <deque>
#include <optional>
#include <ranges>
#include <stop_token>
#include <unordered_map>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(LIB_THREAD, "fy.librarythreadhandler")

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
constexpr auto StatsUpdateInterval = 100ms;
#else
constexpr auto StatsUpdateInterval = 100;
#endif

namespace {
int nextRequestId()
{
    static int requestId{0};
    return requestId++;
}
} // namespace

namespace Fooyin {
namespace {
LibraryScanConfig currentScanConfig(SettingsManager* settings)
{
    using namespace Settings::Core::Internal;

    return {
        .libraryRestrictExt = normaliseExtensions(settings->fileValue(LibraryRestrictTypes).toStringList()),
        .libraryExcludeExt
        = normaliseExtensions(settings->fileValue(LibraryExcludeTypes, QStringList{u"cue"_s}).toStringList()),
        .externalRestrictExt = normaliseExtensions(settings->fileValue(ExternalRestrictTypes).toStringList()),
        .externalExcludeExt  = normaliseExtensions(settings->fileValue(ExternalExcludeTypes).toStringList()),
        .playlistSkipMissing = settings->value<Settings::Core::PlaylistSkipMissing>(),
    };
}

struct LibraryScanRequest
{
    int id;
    ScanRequest::Type type;
    LibraryInfo library;
    QStringList dirs;
    QList<QUrl> files;
    TrackList libraryTracks;
    TrackList tracks;
    bool onlyModified{true};
};
} // namespace

class LibraryThreadHandlerPrivate
{
public:
    struct PendingWatcherSetup
    {
        LibraryInfoMap libraries;
        bool enabled{false};
    };

    struct PendingTrackStatsUpdate
    {
        Track track;
        std::optional<float> rating;
        std::optional<int> playCount;
        std::optional<uint64_t> firstPlayed;
        std::optional<uint64_t> lastPlayed;
        bool writeRating{false};
        bool writePlaycount{false};
    };

    struct WriteOperation
    {
        std::stop_source stopSource;
        std::shared_ptr<QPromise<WriteResult>> promise;
    };

    struct WriteOperationToken
    {
        int id{-1};
        std::stop_token stopToken;
        WriteRequest request;
    };

    LibraryThreadHandlerPrivate(LibraryThreadHandler* self, DbConnectionPoolPtr dbPool, MusicLibrary* library,
                                std::shared_ptr<PlaylistLoader> playlistLoader,
                                std::shared_ptr<TrackMetadataStore> metadataStore,
                                const std::shared_ptr<AudioLoader>& audioLoader, SettingsManager* settings);

    void scanLibrary(const LibraryScanRequest& request);
    void scanTracks(const LibraryScanRequest& request);
    void scanFiles(const LibraryScanRequest& request);
    void scanDirectory(const LibraryScanRequest& request);
    void scanPlaylist(const LibraryScanRequest& request);

    ScanRequest addLibraryScanRequest(const LibraryInfo& libraryInfo, bool onlyModified);
    ScanRequest addTracksScanRequest(const TrackList& tracks, bool onlyModified);
    ScanRequest addFilesScanRequest(const QList<QUrl>& files);
    ScanRequest addDirectoryScanRequest(const LibraryInfo& libraryInfo, const QStringList& dirs);
    ScanRequest addPlaylistRequest(const QList<QUrl>& files);

    [[nodiscard]] std::optional<LibraryScanRequest> currentRequest() const;
    void execNextRequest();
    void setupWatchers(const LibraryInfoMap& libraries, bool enabled);
    void applyPendingWatcherSetup();

    void updateProgress(const ScanProgress& progress);
    void finishScanRequest();
    void completeCurrentScanRequest();
    void cancelScanRequest(int id);

    WriteOperationToken addWriteOperation();
    void finishWriteOperation(int operationId, int succeeded, int failed, bool cancelled);
    void cancelWriteOperations();

    void queueTrackStatsUpdates(const TrackList& tracks, bool writeRating, bool writePlaycount);
    [[nodiscard]] Track applyTrackStatsUpdate(const PendingTrackStatsUpdate& pendingUpdate) const;
    void flushTrackStatsUpdates();

    LibraryThreadHandler* m_self;

    DbConnectionPoolPtr m_dbPool;
    MusicLibrary* m_library;
    SettingsManager* m_settings;

    QThread m_thread;
    LibraryMonitor m_monitor;
    LibraryScanner m_scanner;
    TrackDatabaseManager m_trackDatabaseManager;

    QBasicTimer m_statsTimer;
    std::unordered_map<QString, PendingTrackStatsUpdate> m_pendingTrackStats;

    std::deque<LibraryScanRequest> m_scanRequests;
    int m_nextWriteOperationId{0};
    std::unordered_map<int, WriteOperation> m_writeOperations;
    int m_currentRequestId{-1};
    bool m_currentRequestFinished{false};
    bool m_currentRequestNeedsLibraryCompletion{false};
    bool m_currentRequestLibraryCompleted{false};
    bool m_currentRequestCancelled{false};
    std::optional<PendingWatcherSetup> m_pendingWatcherSetup;
};

LibraryThreadHandlerPrivate::LibraryThreadHandlerPrivate(LibraryThreadHandler* self, DbConnectionPoolPtr dbPool,
                                                         MusicLibrary* library,
                                                         std::shared_ptr<PlaylistLoader> playlistLoader,
                                                         std::shared_ptr<TrackMetadataStore> metadataStore,
                                                         const std::shared_ptr<AudioLoader>& audioLoader,
                                                         SettingsManager* settings)
    : m_self{self}
    , m_dbPool{std::move(dbPool)}
    , m_library{library}
    , m_settings{settings}
    , m_scanner{m_dbPool, std::move(playlistLoader), audioLoader}
    , m_trackDatabaseManager{m_dbPool, audioLoader, m_settings, std::move(metadataStore)}
{
    m_monitor.moveToThread(&m_thread);
    m_scanner.moveToThread(&m_thread);
    m_trackDatabaseManager.moveToThread(&m_thread);

    m_thread.start();
}

void LibraryThreadHandlerPrivate::scanLibrary(const LibraryScanRequest& request)
{
    const LibraryScanConfig config = currentScanConfig(m_settings);
    QMetaObject::invokeMethod(&m_scanner, [this, request, config]() {
        m_scanner.scanLibrary(request.library, request.libraryTracks, request.onlyModified, config);
    });
}

void LibraryThreadHandlerPrivate::scanTracks(const LibraryScanRequest& request)
{
    QMetaObject::invokeMethod(&m_scanner,
                              [this, request]() { m_scanner.scanTracks(request.tracks, request.onlyModified); });
}

void LibraryThreadHandlerPrivate::scanFiles(const LibraryScanRequest& request)
{
    const LibraryScanConfig config = currentScanConfig(m_settings);
    QMetaObject::invokeMethod(
        &m_scanner, [this, request, config]() { m_scanner.scanFiles(request.libraryTracks, request.files, config); });
}

void LibraryThreadHandlerPrivate::scanDirectory(const LibraryScanRequest& request)
{
    const LibraryScanConfig config = currentScanConfig(m_settings);
    QMetaObject::invokeMethod(&m_scanner, [this, request, config]() {
        m_scanner.scanLibraryDirectoies(request.library, request.dirs, request.libraryTracks, config);
    });
}

void LibraryThreadHandlerPrivate::scanPlaylist(const LibraryScanRequest& request)
{
    const LibraryScanConfig config = currentScanConfig(m_settings);
    QMetaObject::invokeMethod(&m_scanner, [this, request, config]() {
        m_scanner.scanPlaylist(request.libraryTracks, request.files, config);
    });
}

ScanRequest LibraryThreadHandlerPrivate::addLibraryScanRequest(const LibraryInfo& libraryInfo, bool onlyModified)
{
    const int id = nextRequestId();

    ScanRequest request{.type = ScanRequest::Library, .id = id, .cancel = [this, id]() { cancelScanRequest(id); }};

    LibraryScanRequest libraryRequest;
    libraryRequest.id           = id;
    libraryRequest.type         = ScanRequest::Library;
    libraryRequest.library      = libraryInfo;
    libraryRequest.onlyModified = onlyModified;

    m_scanRequests.emplace_back(libraryRequest);

    if(m_scanRequests.size() == 1) {
        execNextRequest();
    }

    return request;
}

ScanRequest LibraryThreadHandlerPrivate::addTracksScanRequest(const TrackList& tracks, bool onlyModified)
{
    const int id = nextRequestId();

    ScanRequest request{.type = ScanRequest::Tracks, .id = id, .cancel = [this, id]() { cancelScanRequest(id); }};

    LibraryScanRequest libraryRequest;
    libraryRequest.id           = id;
    libraryRequest.type         = ScanRequest::Tracks;
    libraryRequest.tracks       = tracks;
    libraryRequest.onlyModified = onlyModified;

    m_scanRequests.emplace_front(libraryRequest);

    // Track scans take precedence over library scans
    const auto currRequest = currentRequest();
    if(currRequest && currRequest->type == ScanRequest::Library) {
        m_scanner.pauseThread();
        execNextRequest();
    }
    else if(m_scanRequests.size() == 1) {
        execNextRequest();
    }

    return request;
}

ScanRequest LibraryThreadHandlerPrivate::addFilesScanRequest(const QList<QUrl>& files)
{
    const int id = nextRequestId();

    ScanRequest request{.type = ScanRequest::Files, .id = id, .cancel = [this, id]() { cancelScanRequest(id); }};

    LibraryScanRequest libraryRequest;
    libraryRequest.id    = id;
    libraryRequest.type  = ScanRequest::Files;
    libraryRequest.files = files;

    m_scanRequests.emplace_front(libraryRequest);

    // File scans take precedence over library and track scans
    const auto currRequest = currentRequest();
    if(currRequest && (currRequest->type == ScanRequest::Library || currRequest->type == ScanRequest::Tracks)) {
        m_scanner.pauseThread();
        execNextRequest();
    }
    else if(m_scanRequests.size() == 1) {
        execNextRequest();
    }

    return request;
}

ScanRequest LibraryThreadHandlerPrivate::addDirectoryScanRequest(const LibraryInfo& libraryInfo,
                                                                 const QStringList& dirs)
{
    const int id = nextRequestId();

    ScanRequest request{.type = ScanRequest::Library, .id = id, .cancel = [this, id]() { cancelScanRequest(id); }};

    LibraryScanRequest libraryRequest;
    libraryRequest.id      = id;
    libraryRequest.type    = ScanRequest::Library;
    libraryRequest.library = libraryInfo;
    libraryRequest.dirs    = dirs;

    m_scanRequests.emplace_back(libraryRequest);

    if(m_scanRequests.size() == 1) {
        execNextRequest();
    }

    return request;
}

ScanRequest LibraryThreadHandlerPrivate::addPlaylistRequest(const QList<QUrl>& files)
{
    const int id = nextRequestId();

    ScanRequest request{.type = ScanRequest::Playlist, .id = id, .cancel = [this, id]() { cancelScanRequest(id); }};

    LibraryScanRequest libraryRequest;
    libraryRequest.id    = id;
    libraryRequest.type  = ScanRequest::Playlist;
    libraryRequest.files = files;

    m_scanRequests.emplace_front(libraryRequest);

    // Playlist scans take precedence over library and track scans
    const auto currRequest = currentRequest();
    if(currRequest && (currRequest->type == ScanRequest::Library || currRequest->type == ScanRequest::Tracks)) {
        m_scanner.pauseThread();
        execNextRequest();
    }
    else if(m_scanRequests.size() == 1) {
        execNextRequest();
    }

    return request;
}

std::optional<LibraryScanRequest> LibraryThreadHandlerPrivate::currentRequest() const
{
    const auto requestIt = std::ranges::find_if(
        m_scanRequests, [this](const auto& request) { return request.id == m_currentRequestId; });
    if(requestIt != m_scanRequests.cend()) {
        return *requestIt;
    }
    return {};
}

void LibraryThreadHandlerPrivate::execNextRequest()
{
    if(m_scanRequests.empty()) {
        m_currentRequestId = -1;
        applyPendingWatcherSetup();
        return;
    }

    LibraryScanRequest requestToRun = m_scanRequests.front();
    requestToRun.libraryTracks      = m_library->tracks();

    m_currentRequestId                     = requestToRun.id;
    m_currentRequestFinished               = false;
    m_currentRequestNeedsLibraryCompletion = false;
    m_currentRequestLibraryCompleted       = false;
    m_currentRequestCancelled              = false;

    switch(requestToRun.type) {
        case ScanRequest::Files:
            scanFiles(requestToRun);
            break;
        case ScanRequest::Tracks:
            scanTracks(requestToRun);
            break;
        case ScanRequest::Library:
            if(requestToRun.dirs.isEmpty()) {
                scanLibrary(requestToRun);
            }
            else {
                scanDirectory(requestToRun);
            }
            break;
        case ScanRequest::Playlist:
            scanPlaylist(requestToRun);
            break;
    }
}

void LibraryThreadHandlerPrivate::updateProgress(const ScanProgress& scannerProgress)
{
    ScanProgress progress{scannerProgress};
    progress.id = m_currentRequestId;

    if(!m_scanRequests.empty()) {
        const auto& request = m_scanRequests.front();

        progress.type = request.type;
        progress.info = request.library;
    }

    emit m_self->progressChanged(progress);
}

void LibraryThreadHandlerPrivate::finishScanRequest()
{
    if(!currentRequest()) {
        m_currentRequestId = -1;
        execNextRequest();
        return;
    }

    m_currentRequestFinished = true;

    if(!m_currentRequestCancelled && m_currentRequestNeedsLibraryCompletion && !m_currentRequestLibraryCompleted) {
        return;
    }

    completeCurrentScanRequest();
}

void LibraryThreadHandlerPrivate::completeCurrentScanRequest()
{
    const auto request = currentRequest();
    if(!request) {
        m_currentRequestId = -1;
        execNextRequest();
        return;
    }

    emit m_self->scanFinished(request->id, request->type, m_currentRequestCancelled);

    std::erase_if(m_scanRequests,
                  [this](const auto& pendingRequest) { return pendingRequest.id == m_currentRequestId; });

    m_currentRequestId                     = -1;
    m_currentRequestFinished               = false;
    m_currentRequestNeedsLibraryCompletion = false;
    m_currentRequestLibraryCompleted       = false;
    m_currentRequestCancelled              = false;

    execNextRequest();
}

void LibraryThreadHandlerPrivate::setupWatchers(const LibraryInfoMap& libraries, const bool enabled)
{
    if(enabled && (!m_scanRequests.empty() || m_currentRequestId >= 0)) {
        m_pendingWatcherSetup = PendingWatcherSetup{.libraries = libraries, .enabled = enabled};
        return;
    }

    m_pendingWatcherSetup.reset();
    QMetaObject::invokeMethod(&m_monitor,
                              [this, libraries, enabled]() { m_monitor.setupWatchers(libraries, enabled); });
}

void LibraryThreadHandlerPrivate::applyPendingWatcherSetup()
{
    if(!m_pendingWatcherSetup || !m_scanRequests.empty() || m_currentRequestId >= 0) {
        return;
    }

    const auto pendingSetup = std::move(*m_pendingWatcherSetup);
    m_pendingWatcherSetup.reset();

    QMetaObject::invokeMethod(
        &m_monitor, [this, pendingSetup]() { m_monitor.setupWatchers(pendingSetup.libraries, pendingSetup.enabled); });
}

void LibraryThreadHandlerPrivate::cancelScanRequest(int id)
{
    if(m_currentRequestId == id) {
        m_currentRequestCancelled = true;
        m_scanner.stopThread();
    }
    else {
        const auto requestIt
            = std::ranges::find_if(m_scanRequests, [id](const auto& request) { return request.id == id; });
        if(requestIt == m_scanRequests.cend()) {
            return;
        }

        emit m_self->scanFinished(requestIt->id, requestIt->type, true);
        m_scanRequests.erase(requestIt);
    }
}

LibraryThreadHandlerPrivate::WriteOperationToken LibraryThreadHandlerPrivate::addWriteOperation()
{
    auto promise = std::make_shared<QPromise<WriteResult>>();
    promise->start();

    std::stop_source stopSource;

    const int id = m_nextWriteOperationId++;
    const auto [operationIt, inserted]
        = m_writeOperations.emplace(id, WriteOperation{.stopSource = std::move(stopSource), .promise = promise});

    auto& operation = operationIt->second;

    WriteRequest request;
    request.cancel = [stopSource = operation.stopSource]() mutable {
        stopSource.request_stop();
    };
    request.finished = promise->future();

    return {.id = id, .stopToken = operation.stopSource.get_token(), .request = std::move(request)};
}

void LibraryThreadHandlerPrivate::finishWriteOperation(int operationId, int succeeded, int failed, bool cancelled)
{
    const auto operationIt = m_writeOperations.find(operationId);
    if(operationIt == m_writeOperations.cend()) {
        return;
    }

    const auto promise = std::move(operationIt->second.promise);
    m_writeOperations.erase(operationIt);

    if(!promise) {
        return;
    }

    promise->addResult(WriteResult{
        .state     = cancelled ? WriteState::Cancelled : WriteState::Completed,
        .succeeded = succeeded,
        .failed    = failed,
    });
    promise->finish();
}

void LibraryThreadHandlerPrivate::cancelWriteOperations()
{
    for(auto& [id, operation] : m_writeOperations) {
        operation.stopSource.request_stop();
        if(operation.promise) {
            operation.promise->addResult(WriteResult{.state = WriteState::Cancelled});
            operation.promise->finish();
        }
    }

    m_writeOperations.clear();
}

void LibraryThreadHandlerPrivate::queueTrackStatsUpdates(const TrackList& tracks, bool writeRating, bool writePlaycount)
{
    for(const Track& track : tracks) {
        const QString key   = track.id() >= 0 ? QString::number(track.id()) : track.uniqueFilepath();
        auto [pendingIt, _] = m_pendingTrackStats.try_emplace(key);
        auto& pendingUpdate = pendingIt->second;

        pendingUpdate.track = track;

        if(writeRating) {
            pendingUpdate.rating      = track.rating();
            pendingUpdate.writeRating = true;
        }
        if(writePlaycount) {
            pendingUpdate.playCount      = track.playCount();
            pendingUpdate.firstPlayed    = track.firstPlayed();
            pendingUpdate.lastPlayed     = track.lastPlayed();
            pendingUpdate.writePlaycount = true;
        }

        qCDebug(LIB_THREAD) << "Queued track stats update:" << "key=" << key << "id=" << track.id()
                            << "path=" << track.uniqueFilepath() << "rating=" << track.rating()
                            << "playCount=" << track.playCount() << "firstPlayed=" << track.firstPlayed()
                            << "lastPlayed=" << track.lastPlayed() << "writeRating=" << writeRating
                            << "writePlaycount=" << writePlaycount;
    }

    m_statsTimer.start(StatsUpdateInterval, m_self);
}

Track LibraryThreadHandlerPrivate::applyTrackStatsUpdate(const PendingTrackStatsUpdate& pendingUpdate) const
{
    Track track{pendingUpdate.track};

    if(track.id() >= 0) {
        if(const Track currentTrack = m_library->trackForId(track.id()); currentTrack.isValid()) {
            track = currentTrack;
        }
    }

    if(pendingUpdate.rating) {
        track.setRating(*pendingUpdate.rating);
    }
    if(pendingUpdate.playCount) {
        track.setPlayCount(*pendingUpdate.playCount);
    }
    if(pendingUpdate.firstPlayed) {
        track.setFirstPlayed(*pendingUpdate.firstPlayed);
    }
    if(pendingUpdate.lastPlayed) {
        track.setLastPlayed(*pendingUpdate.lastPlayed);
    }

    return track;
}

void LibraryThreadHandlerPrivate::flushTrackStatsUpdates()
{
    if(m_pendingTrackStats.empty()) {
        return;
    }

    qCDebug(LIB_THREAD) << "Flushing pending track stats updates:" << m_pendingTrackStats.size();

    TrackList ratingTracks;
    TrackList playcountTracks;
    TrackList ratingAndPlaycountTracks;

    ratingTracks.reserve(m_pendingTrackStats.size());
    playcountTracks.reserve(m_pendingTrackStats.size());
    ratingAndPlaycountTracks.reserve(m_pendingTrackStats.size());

    for(const auto& pendingUpdate : m_pendingTrackStats | std::views::values) {
        const Track track = applyTrackStatsUpdate(pendingUpdate);

        qCDebug(LIB_THREAD) << "Resolved track stats update:" << "id=" << track.id()
                            << "path=" << track.uniqueFilepath() << "rating=" << track.rating()
                            << "playCount=" << track.playCount() << "firstPlayed=" << track.firstPlayed()
                            << "lastPlayed=" << track.lastPlayed() << "writeRating=" << pendingUpdate.writeRating
                            << "writePlaycount=" << pendingUpdate.writePlaycount;

        if(pendingUpdate.writeRating && pendingUpdate.writePlaycount) {
            ratingAndPlaycountTracks.push_back(track);
        }
        else if(pendingUpdate.writeRating) {
            ratingTracks.push_back(track);
        }
        else if(pendingUpdate.writePlaycount) {
            playcountTracks.push_back(track);
        }
    }

    m_pendingTrackStats.clear();

    QMetaObject::invokeMethod(&m_trackDatabaseManager,
                              [this, ratingAndPlaycountTracks = std::move(ratingAndPlaycountTracks),
                               ratingTracks = std::move(ratingTracks), playcountTracks = std::move(playcountTracks)]() {
                                  if(!ratingAndPlaycountTracks.empty()) {
                                      m_trackDatabaseManager.updateTrackStats(
                                          ratingAndPlaycountTracks, AudioReader::Rating | AudioReader::Playcount);
                                  }
                                  if(!ratingTracks.empty()) {
                                      m_trackDatabaseManager.updateTrackStats(ratingTracks, AudioReader::Rating);
                                  }
                                  if(!playcountTracks.empty()) {
                                      m_trackDatabaseManager.updateTrackStats(playcountTracks, AudioReader::Playcount);
                                  }
                              });
}

LibraryThreadHandler::LibraryThreadHandler(DbConnectionPoolPtr dbPool, MusicLibrary* library,
                                           std::shared_ptr<PlaylistLoader> playlistLoader,
                                           std::shared_ptr<TrackMetadataStore> metadataStore,
                                           std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings,
                                           QObject* parent)
    : QObject{parent}
    , p{std::make_unique<LibraryThreadHandlerPrivate>(this, std::move(dbPool), library, std::move(playlistLoader),
                                                      std::move(metadataStore), std::move(audioLoader), settings)}
{
    QObject::connect(&p->m_trackDatabaseManager, &TrackDatabaseManager::gotTracks, this,
                     &LibraryThreadHandler::gotTracks);
    QObject::connect(&p->m_trackDatabaseManager, &TrackDatabaseManager::updatedTracks, this,
                     &LibraryThreadHandler::tracksUpdated);
    QObject::connect(&p->m_trackDatabaseManager, &TrackDatabaseManager::availabilityChecked, this,
                     &LibraryThreadHandler::tracksAvailabilityUpdated);
    QObject::connect(&p->m_trackDatabaseManager, &TrackDatabaseManager::updatedTracksStats, this,
                     &LibraryThreadHandler::tracksStatsUpdated);
    QObject::connect(&p->m_trackDatabaseManager, &TrackDatabaseManager::removedTracks, this,
                     &LibraryThreadHandler::tracksRemoved);
    QObject::connect(&p->m_trackDatabaseManager, &TrackDatabaseManager::trackWriteCompleted, this,
                     [this](int operationId, const TrackList& tracks, int failed, bool cancelled) {
                         p->finishWriteOperation(operationId, static_cast<int>(tracks.size()), failed, cancelled);
                     });
    QObject::connect(&p->m_trackDatabaseManager, &TrackDatabaseManager::trackCoverWriteCompleted, this,
                     [this](int operationId, const TrackList& tracks, int failed, bool cancelled) {
                         p->finishWriteOperation(operationId, static_cast<int>(tracks.size()), failed, cancelled);
                     });
    QObject::connect(&p->m_trackDatabaseManager, &TrackDatabaseManager::unavailableTracksRemoved, this,
                     [this](int operationId, const TrackList& tracks, int failed, bool cancelled) {
                         p->finishWriteOperation(operationId, static_cast<int>(tracks.size()), failed, cancelled);
                     });
    QObject::connect(&p->m_scanner, &Worker::finished, this, [this]() { p->finishScanRequest(); });
    QObject::connect(&p->m_scanner, &LibraryScanner::progressChanged, this,
                     [this](const ScanProgress& progress) { p->updateProgress(progress); });
    QObject::connect(&p->m_scanner, &LibraryScanner::scannedTracks, this, [this](const TrackList& tracks) {
        p->m_currentRequestNeedsLibraryCompletion = true;
        emit scannedTracks(p->m_currentRequestId, tracks);
    });
    QObject::connect(&p->m_scanner, &LibraryScanner::playlistLoaded, this, [this](const TrackList& tracks) {
        p->m_currentRequestNeedsLibraryCompletion = true;
        emit playlistLoaded(p->m_currentRequestId, tracks);
    });
    QObject::connect(&p->m_scanner, &LibraryScanner::statusChanged, this, &LibraryThreadHandler::statusChanged);
    QObject::connect(&p->m_scanner, &LibraryScanner::scanUpdate, this, &LibraryThreadHandler::scanUpdate);
    QObject::connect(&p->m_monitor, &LibraryMonitor::statusChanged, this, &LibraryThreadHandler::statusChanged);
    QObject::connect(&p->m_monitor, &LibraryMonitor::directoriesChanged, this,
                     [this](const LibraryInfo& libraryInfo, const QStringList& dirs) {
                         p->addDirectoryScanRequest(libraryInfo, dirs);
                     });

    QMetaObject::invokeMethod(&p->m_scanner, &Worker::initialiseThread);
    QMetaObject::invokeMethod(&p->m_trackDatabaseManager, &Worker::initialiseThread);
}

LibraryThreadHandler::~LibraryThreadHandler()
{
    p->cancelWriteOperations();

    p->m_scanner.closeThread();
    p->m_scanner.stopThread();
    p->m_trackDatabaseManager.closeThread();
    p->m_trackDatabaseManager.stopThread();

    p->m_thread.quit();
    p->m_thread.wait();
}

void LibraryThreadHandler::getAllTracks()
{
    QMetaObject::invokeMethod(&p->m_trackDatabaseManager, &TrackDatabaseManager::getAllTracks);
}

void LibraryThreadHandler::setupWatchers(const LibraryInfoMap& libraries, bool enabled)
{
    p->setupWatchers(libraries, enabled);
}

bool LibraryThreadHandler::hasPendingLibraryScan(const int libraryId) const
{
    return std::ranges::any_of(p->m_scanRequests, [libraryId](const LibraryScanRequest& request) {
        return request.type == ScanRequest::Library && request.library.id == libraryId;
    });
}

ScanRequest LibraryThreadHandler::refreshLibrary(const LibraryInfo& library)
{
    return p->addLibraryScanRequest(library, true);
}

ScanRequest LibraryThreadHandler::scanLibrary(const LibraryInfo& library)
{
    return p->addLibraryScanRequest(library, false);
}

void LibraryThreadHandler::cancelScan(int id)
{
    p->cancelScanRequest(id);
}

ScanRequest LibraryThreadHandler::scanTracks(const TrackList& tracks, bool onlyModified)
{
    return p->addTracksScanRequest(tracks, onlyModified);
}

ScanRequest LibraryThreadHandler::scanFiles(const QList<QUrl>& files)
{
    return p->addFilesScanRequest(files);
}

ScanRequest LibraryThreadHandler::loadPlaylist(const QList<QUrl>& files)
{
    return p->addPlaylistRequest(files);
}

void LibraryThreadHandler::acknowledgeTracksScanned(const int id)
{
    if(id != p->m_currentRequestId) {
        return;
    }

    p->m_currentRequestLibraryCompleted = true;
    if(p->m_currentRequestFinished) {
        p->completeCurrentScanRequest();
    }
}

void LibraryThreadHandler::saveUpdatedTracks(const TrackList& tracks)
{
    QMetaObject::invokeMethod(&p->m_trackDatabaseManager,
                              [this, tracks]() { p->m_trackDatabaseManager.updateTracks(tracks, false); });
}

WriteRequest LibraryThreadHandler::writeUpdatedTracks(const TrackList& tracks)
{
    const auto operation = p->addWriteOperation();

    QMetaObject::invokeMethod(&p->m_trackDatabaseManager,
                              [this, tracks, operationId = operation.id, stopToken = operation.stopToken]() {
                                  p->m_trackDatabaseManager.updateTracks(tracks, true, operationId, stopToken);
                              });

    return operation.request;
}

WriteRequest LibraryThreadHandler::writeTrackCovers(const TrackCoverData& tracks)
{
    const auto operation = p->addWriteOperation();

    QMetaObject::invokeMethod(&p->m_trackDatabaseManager,
                              [this, tracks, operationId = operation.id, stopToken = operation.stopToken]() {
                                  p->m_trackDatabaseManager.writeCovers(tracks, operationId, stopToken);
                              });

    return operation.request;
}

void LibraryThreadHandler::saveUpdatedTrackStats(const TrackList& tracks)
{
    p->queueTrackStatsUpdates(tracks, true, false);
}

void LibraryThreadHandler::checkTrackAvailability(const TrackList& tracks)
{
    QMetaObject::invokeMethod(&p->m_trackDatabaseManager,
                              [this, tracks]() { p->m_trackDatabaseManager.checkTrackAvailability(tracks); });
}

void LibraryThreadHandler::saveUpdatedTrackPlaycounts(const TrackList& tracks)
{
    p->queueTrackStatsUpdates(tracks, false, true);
}

WriteRequest LibraryThreadHandler::removeUnavailbleTracks(const TrackList& tracks)
{
    const auto operation = p->addWriteOperation();

    QMetaObject::invokeMethod(&p->m_trackDatabaseManager,
                              [this, tracks, operationId = operation.id, stopToken = operation.stopToken]() {
                                  p->m_trackDatabaseManager.removeUnavailbleTracks(tracks, operationId, stopToken);
                              });

    return operation.request;
}

void LibraryThreadHandler::cleanupTracks()
{
    QMetaObject::invokeMethod(&p->m_trackDatabaseManager, &TrackDatabaseManager::cleanupTracks);
}

void LibraryThreadHandler::libraryRemoved(int id)
{
    if(p->m_scanRequests.empty()) {
        return;
    }

    const auto request = p->currentRequest();
    if(request && request->type == ScanRequest::Library && request->library.id == id) {
        p->m_scanner.stopThread();
    }
    else {
        std::erase_if(p->m_scanRequests, [id](const auto& pendingRequest) { return pendingRequest.library.id == id; });
    }
}

void LibraryThreadHandler::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == p->m_statsTimer.timerId()) {
        p->m_statsTimer.stop();
        p->flushTrackStatsUpdates();
    }

    QObject::timerEvent(event);
}
} // namespace Fooyin

#include "moc_librarythreadhandler.cpp"
