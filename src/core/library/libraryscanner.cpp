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

#include "libraryscanner.h"

#include "database/trackdatabase.h"
#include "librarycuetrackreloader.h"
#include "libraryscansession.h"
#include "libraryscanutils.h"

#include <core/engine/audioloader.h>
#include <core/playlist/playlistloader.h>
#include <core/track.h>
#include <core/trackmetadatastore.h>
#include <utils/database/dbconnectionhandler.h>
#include <utils/database/dbconnectionpool.h>
#include <utils/timer.h>
#include <utils/utils.h>

#include <QFileInfo>
#include <QLoggingCategory>

#include <algorithm>
#include <iterator>
#include <ranges>

Q_LOGGING_CATEGORY(LIB_SCANNER, "fy.scanner")

namespace Fooyin {
LibraryScanner::LibraryScanner(DbConnectionPoolPtr dbPool, std::shared_ptr<PlaylistLoader> playlistLoader,
                               std::shared_ptr<TrackMetadataStore> metadataStore,
                               std::shared_ptr<AudioLoader> audioLoader, std::shared_ptr<RemoteIoService> remoteIo,
                               QObject* parent)
    : Worker{parent}
    , m_dbPool{std::move(dbPool)}
    , m_playlistLoader{std::move(playlistLoader)}
    , m_metadataStore{std::move(metadataStore)}
    , m_audioLoader{std::move(audioLoader)}
    , m_remoteIo{std::move(remoteIo)}
    , m_requestId{-1}
    , m_progressOnlyModified{false}
{ }

LibraryScanner::~LibraryScanner() = default;

void LibraryScanner::initialiseThread()
{
    Worker::initialiseThread();

    m_dbHandler = std::make_unique<DbConnectionHandler>(m_dbPool);
    m_trackDatabase.initialise(DbConnectionProvider{m_dbPool});
}

bool LibraryScanner::stopRequested() const
{
    return Worker::stopRequested();
}

void LibraryScanner::reportProgress(const int current, const QString& file, const int total, const int phase,
                                    const int discovered)
{
    Q_EMIT progressChanged(makeProgress(current, file, total, static_cast<ScanProgress::Phase>(phase), discovered));
}

void LibraryScanner::reportScanUpdate(const ScanResult& result)
{
    Q_EMIT scanUpdate(m_requestId, result);
}

void LibraryScanner::scanLibrary(const LibraryInfo& library, const TrackList& tracks, const bool onlyModified,
                                 const LibraryScanConfig& config, std::stop_token token, int requestId)
{
    m_requestId = requestId;
    if(!startWork(token)) {
        Q_EMIT scanFinished(requestId);
        return;
    }

    const bool isMonitoring = library.status == LibraryInfo::Status::Monitoring;

    m_progressOnlyModified = onlyModified;
    startSession(config, library);
    changeLibraryStatus(LibraryInfo::Status::Scanning);

    const Timer timer;

    if(m_currentLibrary.id >= 0 && QFileInfo::exists(m_currentLibrary.path)) {
        m_session->scanLibrary(library, tracks, onlyModified);
    }

    if(state() == Paused && m_session->error().isEmpty()) {
        changeLibraryStatus(LibraryInfo::Status::Pending);
    }
    else {
        changeLibraryStatus(isMonitoring ? LibraryInfo::Status::Monitoring : LibraryInfo::Status::Idle);
        finishSession();
        setState(Idle);
        Q_EMIT scanFinished(m_requestId);
    }

    qCInfo(LIB_SCANNER) << "Scan of" << library.name << "took" << timer.elapsedFormatted();

    clearSession();
}

void LibraryScanner::scanLibraryDirectoies(const LibraryInfo& library, const QStringList& dirs, const TrackList& tracks,
                                           const LibraryScanConfig& config, std::stop_token token, int requestId)
{
    m_requestId = requestId;
    if(!startWork(token)) {
        Q_EMIT scanFinished(requestId);
        return;
    }

    const bool isMonitoring = library.status == LibraryInfo::Status::Monitoring;

    m_progressOnlyModified = true;
    startSession(config, library);
    changeLibraryStatus(LibraryInfo::Status::Scanning);

    m_session->scanDirectories(library, dirs, tracks);

    if(state() == Paused && m_session->error().isEmpty()) {
        changeLibraryStatus(LibraryInfo::Status::Pending);
    }
    else {
        changeLibraryStatus(isMonitoring ? LibraryInfo::Status::Monitoring : LibraryInfo::Status::Idle);
        finishSession();

        setState(Idle);
        Q_EMIT scanFinished(m_requestId);
    }

    clearSession();
}

void LibraryScanner::scanTracks(const TrackList& tracks, const bool onlyModified, const LibraryScanConfig& config,
                                std::stop_token token, int requestId)
{
    m_requestId = requestId;
    if(!startWork(token)) {
        Q_EMIT scanFinished(requestId);
        return;
    }

    m_progressOnlyModified = onlyModified;

    const auto shouldContinue = [this, token]() {
        return mayRun(token);
    };

    const Timer timer;

    Q_EMIT progressChanged(
        makeProgress(0, {}, static_cast<int>(tracks.size()), ScanProgress::Phase::ReadingMetadata, 0));

    TrackList tracksToAdd;
    TrackList tracksToUpdate;
    int processedTracks{0};

    TrackList cueTracksToReload;

    for(const Track& track : tracks) {
        if(!shouldContinue()) {
            Q_EMIT progressChanged(
                makeProgress(processedTracks, {}, processedTracks, ScanProgress::Phase::Finished, 0));
            if(state() != Paused) {
                setState(Idle);
                Q_EMIT scanFinished(m_requestId);
            }
            return;
        }

        if(track.hasCue()) {
            cueTracksToReload.push_back(track);
            continue;
        }

        if(onlyModified) {
            const QFileInfo info{physicalTrackPath(track)};
            const QDateTime lastModifiedTime{info.lastModified()};
            const uint64_t lastModified
                = lastModifiedTime.isValid() ? static_cast<uint64_t>(lastModifiedTime.toMSecsSinceEpoch()) : 0;

            if(track.modifiedTime() >= lastModified) {
                ++processedTracks;
                Q_EMIT progressChanged(makeProgress(processedTracks, track.filepath(), static_cast<int>(tracks.size()),
                                                    ScanProgress::Phase::ReadingMetadata, 0));
                continue;
            }
        }

        Track updatedTrack{track.filepath(), track.subsong(), track.metadataStore()};

        if(m_audioLoader->readTrackMetadata(updatedTrack)) {
            updatedTrack.setLibraryId(track.libraryId());
            updatedTrack.setAddedTime(track.addedTime());
            updatedTrack.setIsEnabled(track.isEnabled());
            mergeReloadedTrackStats(updatedTrack, track,
                                    {.overwriteRatingOnReload    = config.overwriteRatingOnReload,
                                     .overwritePlaycountOnReload = config.overwritePlaycountOnReload});

            const QFileInfo fileInfo{physicalTrackPath(updatedTrack)};
            if(updatedTrack.createdTime() == 0) {
                const QDateTime createdTime = fileInfo.birthTime();
                updatedTrack.setCreatedTime(createdTime.isValid() ? createdTime.toMSecsSinceEpoch() : 0);
            }
            if(updatedTrack.modifiedTime() == 0) {
                const QDateTime modifiedTime = fileInfo.lastModified();
                updatedTrack.setModifiedTime(modifiedTime.isValid() ? modifiedTime.toMSecsSinceEpoch() : 0);
            }
            if(updatedTrack.fileSize() == 0) {
                updatedTrack.setFileSize(fileInfo.size());
            }

            updatedTrack.generateHash();

            if(track.isInDatabase()) {
                updatedTrack.setId(track.id());
                tracksToUpdate.push_back(updatedTrack);
            }
            else {
                tracksToAdd.push_back(updatedTrack);
            }
        }

        ++processedTracks;
        Q_EMIT progressChanged(makeProgress(processedTracks, track.filepath(), static_cast<int>(tracks.size()),
                                            ScanProgress::Phase::ReadingMetadata, 0));
    }

    CueTrackReloader cueReloader{m_audioLoader.get(), m_metadataStore};
    CueTrackReloadResult cueResult = cueReloader.reload(
        cueTracksToReload, onlyModified,
        {.overwriteRatingOnReload    = config.overwriteRatingOnReload,
         .overwritePlaycountOnReload = config.overwritePlaycountOnReload},
        shouldContinue,
        [this, &processedTracks, total = static_cast<int>(tracks.size())](const int count, const QString& source) {
            processedTracks += count;
            Q_EMIT progressChanged(
                makeProgress(processedTracks, source, total, ScanProgress::Phase::ReadingMetadata, 0));
        });

    if(cueResult.cancelled) {
        Q_EMIT progressChanged(makeProgress(processedTracks, {}, processedTracks, ScanProgress::Phase::Finished, 0));
        if(state() != Paused) {
            setState(Idle);
            Q_EMIT scanFinished(m_requestId);
        }
        return;
    }

    tracksToAdd.insert(tracksToAdd.end(), std::make_move_iterator(cueResult.addedTracks.begin()),
                       std::make_move_iterator(cueResult.addedTracks.end()));
    tracksToUpdate.insert(tracksToUpdate.end(), std::make_move_iterator(cueResult.updatedTracks.begin()),
                          std::make_move_iterator(cueResult.updatedTracks.end()));

    QString error;
    if(!tracksToAdd.empty() || !tracksToUpdate.empty()) {
        Q_EMIT progressChanged(makeProgress(processedTracks, {}, static_cast<int>(tracks.size()),
                                            ScanProgress::Phase::WritingDatabase, 0));
        if(auto result = m_trackDatabase.writeScanBatch(tracksToAdd, tracksToUpdate); result) {
            if(!tracksToAdd.empty()) {
                Q_EMIT scannedTracks(m_requestId, tracksToAdd);
            }
            if(!tracksToUpdate.empty()) {
                Q_EMIT scanUpdate(m_requestId, {.addedTracks = {}, .updatedTracks = tracksToUpdate});
            }
        }
        else {
            error = std::move(result.error());
            qCWarning(LIB_SCANNER) << "Scan failed:" << error;
            Q_EMIT scanFailed(m_requestId, error);
        }
    }

    Q_EMIT progressChanged(makeProgress(processedTracks, {}, processedTracks, ScanProgress::Phase::Finished, 0));

    if(state() != Paused || !error.isEmpty()) {
        setState(Idle);
        Q_EMIT scanFinished(m_requestId);
    }

    qCInfo(LIB_SCANNER) << "Scan of" << tracks.size() << "tracks took" << timer.elapsedFormatted();
}

void LibraryScanner::scanFiles(const TrackList& libraryTracks, const QList<QUrl>& urls, const LibraryScanConfig& config,
                               std::stop_token token, int requestId)
{
    m_requestId = requestId;
    if(!startWork(token)) {
        Q_EMIT scanFinished(requestId);
        return;
    }

    m_progressOnlyModified = false;
    startSession(config);

    const Timer timer;

    LibraryScanFilesResult result;

    bool completed = m_session->scanFiles(libraryTracks, urls, result);

    if(completed && !result.playlistTracksScanned.empty()) {
        Q_EMIT progressChanged(
            makeProgress(static_cast<int>(m_session->progressCount()), {}, static_cast<int>(m_session->progressCount()),
                         ScanProgress::Phase::WritingDatabase, static_cast<int>(m_session->discoveredFiles())));
        completed = storeScannedTracks(result.playlistTracksScanned);
        if(completed) {
            Q_EMIT playlistLoaded(m_requestId, result.playlistTracksScanned);
        }
    }

    if(completed && !result.tracksScanned.empty()) {
        Q_EMIT progressChanged(
            makeProgress(static_cast<int>(m_session->progressCount()), {}, static_cast<int>(m_session->progressCount()),
                         ScanProgress::Phase::WritingDatabase, static_cast<int>(m_session->discoveredFiles())));
        if(storeScannedTracks(result.tracksScanned)) {
            Q_EMIT scannedTracks(m_requestId, result.tracksScanned);
        }
    }

    const int scannedFiles = static_cast<int>(m_session->filesScanned());

    finishSession();
    clearSession();

    setState(Idle);
    Q_EMIT scanFinished(m_requestId);

    qCInfo(LIB_SCANNER) << "Scan of" << scannedFiles << "files took" << timer.elapsedFormatted();
}

void LibraryScanner::scanPlaylist(const TrackList& libraryTracks, const QList<QUrl>& urls,
                                  const LibraryScanConfig& config, std::stop_token token, int requestId)
{
    m_requestId = requestId;
    if(!startWork(token)) {
        Q_EMIT scanFinished(requestId);
        return;
    }

    m_progressOnlyModified = false;
    startSession(config);

    auto timer = std::make_shared<Timer>();

    const auto finishPlaylistScan = [this, timer, requestId](bool completed, TrackList tracksScanned) {
        if(m_requestId != requestId || !m_session) {
            return;
        }
        if(!completed && !stopRequested() && m_session->error().isEmpty()) {
            m_session->fail(tr("Could not load the playlist."));
        }
        if(completed && !stopRequested() && !tracksScanned.empty()) {
            Q_EMIT progressChanged(makeProgress(
                static_cast<int>(m_session->progressCount()), {}, static_cast<int>(m_session->progressCount()),
                ScanProgress::Phase::WritingDatabase, static_cast<int>(m_session->discoveredFiles())));
            if(storeScannedTracks(tracksScanned)) {
                Q_EMIT playlistLoaded(m_requestId, tracksScanned);
            }
        }

        finishSession();
        clearSession();

        setState(Idle);
        Q_EMIT scanFinished(m_requestId);

        qCInfo(LIB_SCANNER) << "Scan of playlist took" << timer->elapsedFormatted();
    };

    const bool containsRemotePlaylist
        = std::ranges::any_of(urls, [](const QUrl& url) { return Track::isRemotePath(url.toString()); });
    if(!containsRemotePlaylist) {
        TrackList tracksScanned;
        const bool completed = m_session->scanPlaylist(libraryTracks, urls, tracksScanned);
        finishPlaylistScan(completed, std::move(tracksScanned));
        return;
    }

    m_session->scanPlaylistAsync(libraryTracks, urls, this, finishPlaylistScan, token);
}

ScanProgress LibraryScanner::makeProgress(const int current, const QString& file, const int total,
                                          const ScanProgress::Phase phase, const int discovered) const
{
    return {
        .type         = ScanRequest::Library,
        .info         = {},
        .id           = m_requestId,
        .total        = total,
        .current      = current,
        .discovered   = discovered,
        .onlyModified = m_progressOnlyModified,
        .phase        = phase,
        .file         = file,
    };
}

void LibraryScanner::changeLibraryStatus(const LibraryInfo::Status status)
{
    m_currentLibrary.status = status;
    Q_EMIT statusChanged(m_currentLibrary);
}

void LibraryScanner::startSession(const LibraryScanConfig& config, const LibraryInfo& library)
{
    m_currentLibrary = library;
    m_session = std::make_unique<LibraryScanSession>(&m_trackDatabase, m_playlistLoader.get(), m_audioLoader.get(),
                                                     m_remoteIo, m_metadataStore, config, this);
}

void LibraryScanner::finishSession()
{
    if(m_session) {
        m_session->finish();
        if(const auto error = m_session->error(); !error.isEmpty()) {
            qCWarning(LIB_SCANNER) << "Scan failed:" << error;
            Q_EMIT scanFailed(m_requestId, error);
        }
    }
}

void LibraryScanner::clearSession()
{
    m_session.reset();
}

bool LibraryScanner::storeScannedTracks(TrackList& tracks)
{
    TrackList updates;
    if(auto result = m_trackDatabase.writeScanBatch(tracks, updates); !result) {
        m_session->fail(std::move(result.error()));
        return false;
    }
    return true;
}
} // namespace Fooyin

#include "moc_libraryscanner.cpp"
