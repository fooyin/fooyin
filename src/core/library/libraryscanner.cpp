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

#include "libraryscanner.h"

#include "database/trackdatabase.h"
#include "libraryscansession.h"
#include "playlist/playlistloader.h"

#include <utils/database/dbconnectionhandler.h>
#include <utils/database/dbconnectionpool.h>
#include <utils/timer.h>
#include <utils/utils.h>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(LIB_SCANNER, "fy.scanner")

namespace Fooyin {
ScanProgress LibraryScanner::makeProgress(const int current, const QString& file, const int total,
                                          const ScanProgress::Phase phase, const int discovered) const
{
    return {
        .type         = ScanRequest::Library,
        .info         = {},
        .id           = -1,
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
    emit statusChanged(m_currentLibrary);
}

void LibraryScanner::startSession(const LibraryScanConfig& config, const LibraryInfo& library)
{
    m_currentLibrary = library;
    resetStopSource();
    m_session = std::make_unique<LibraryScanSession>(&m_trackDatabase, m_playlistLoader.get(), m_audioLoader.get(),
                                                     config, this);
}

void LibraryScanner::finishSession()
{
    if(m_session) {
        m_session->finish();
    }
}

void LibraryScanner::clearSession()
{
    m_session.reset();
}

LibraryScanner::LibraryScanner(DbConnectionPoolPtr dbPool, std::shared_ptr<PlaylistLoader> playlistLoader,
                               std::shared_ptr<AudioLoader> audioLoader, QObject* parent)
    : Worker{parent}
    , m_dbPool{std::move(dbPool)}
    , m_playlistLoader{std::move(playlistLoader)}
    , m_audioLoader{std::move(audioLoader)}
    , m_progressOnlyModified{false}
{ }

LibraryScanner::~LibraryScanner() = default;

void LibraryScanner::initialiseThread()
{
    Worker::initialiseThread();

    m_dbHandler = std::make_unique<DbConnectionHandler>(m_dbPool);
    m_trackDatabase.initialise(DbConnectionProvider{m_dbPool});
}

void LibraryScanner::stopThread()
{
    if(state() == Running && m_session) {
        const int scanned    = static_cast<int>(m_session->progressCount());
        const int discovered = static_cast<int>(m_session->discoveredFiles());

        QMetaObject::invokeMethod(
            this,
            [this, scanned, discovered]() {
                emit progressChanged(makeProgress(scanned, {}, scanned, ScanProgress::Phase::Finished, discovered));
            },
            Qt::QueuedConnection);
    }

    Worker::stopThread();
}

bool LibraryScanner::stopRequested() const
{
    return Worker::stopRequested();
}

void LibraryScanner::reportProgress(const int current, const QString& file, const int total, const int phase,
                                    const int discovered)
{
    emit progressChanged(makeProgress(current, file, total, static_cast<ScanProgress::Phase>(phase), discovered));
}

void LibraryScanner::reportScanUpdate(const ScanResult& result)
{
    emit scanUpdate(result);
}

void LibraryScanner::scanLibrary(const LibraryInfo& library, const TrackList& tracks, const bool onlyModified,
                                 const LibraryScanConfig& config)
{
    setState(Running);

    const bool isMonitoring = library.status == LibraryInfo::Status::Monitoring;

    m_progressOnlyModified = onlyModified;
    startSession(config, library);
    changeLibraryStatus(LibraryInfo::Status::Scanning);

    const Timer timer;

    if(m_currentLibrary.id >= 0 && QFileInfo::exists(m_currentLibrary.path)) {
        m_session->scanLibrary(library, tracks, onlyModified);
    }

    if(state() == Paused) {
        changeLibraryStatus(LibraryInfo::Status::Pending);
    }
    else {
        changeLibraryStatus(isMonitoring ? LibraryInfo::Status::Monitoring : LibraryInfo::Status::Idle);
        finishSession();
        setState(Idle);
        emit finished();
    }

    qCInfo(LIB_SCANNER) << "Scan of" << library.name << "took" << timer.elapsedFormatted();

    clearSession();
}

void LibraryScanner::scanLibraryDirectoies(const LibraryInfo& library, const QStringList& dirs, const TrackList& tracks,
                                           const LibraryScanConfig& config)
{
    setState(Running);

    const bool isMonitoring = library.status == LibraryInfo::Status::Monitoring;

    m_progressOnlyModified = true;
    startSession(config, library);
    changeLibraryStatus(LibraryInfo::Status::Scanning);

    m_session->scanDirectories(library, dirs, tracks);

    if(state() == Paused) {
        changeLibraryStatus(LibraryInfo::Status::Pending);
    }
    else {
        changeLibraryStatus(isMonitoring ? LibraryInfo::Status::Monitoring : LibraryInfo::Status::Idle);
        finishSession();

        setState(Idle);
        emit finished();
    }

    clearSession();
}

void LibraryScanner::scanTracks(const TrackList& tracks, const bool onlyModified)
{
    setState(Running);

    m_progressOnlyModified = onlyModified;
    resetStopSource();

    const auto shouldContinue = [this, stopToken = this->stopToken()]() {
        return !stopToken.stop_requested() && mayRun();
    };

    const Timer timer;

    emit progressChanged(makeProgress(0, {}, static_cast<int>(tracks.size()), ScanProgress::Phase::ReadingMetadata, 0));

    TrackList tracksToUpdate;
    int processedTracks{0};

    for(const Track& track : tracks) {
        if(!shouldContinue()) {
            emit progressChanged(makeProgress(processedTracks, {}, processedTracks, ScanProgress::Phase::Finished, 0));
            setState(Idle);
            emit finished();
            return;
        }

        if(track.hasCue()) {
            continue;
        }

        if(onlyModified) {
            const QFileInfo info{track.filepath()};
            const QDateTime lastModifiedTime{info.lastModified()};
            const uint64_t lastModified
                = lastModifiedTime.isValid() ? static_cast<uint64_t>(lastModifiedTime.toMSecsSinceEpoch()) : 0;

            if(track.modifiedTime() >= lastModified) {
                ++processedTracks;
                emit progressChanged(makeProgress(processedTracks, track.filepath(), static_cast<int>(tracks.size()),
                                                  ScanProgress::Phase::ReadingMetadata, 0));
                continue;
            }
        }

        Track updatedTrack{track.filepath()};

        if(m_audioLoader->readTrackMetadata(updatedTrack)) {
            updatedTrack.setId(track.id());
            updatedTrack.setLibraryId(track.libraryId());
            updatedTrack.setAddedTime(track.addedTime());

            const QFileInfo fileInfo{updatedTrack.filepath()};
            if(updatedTrack.modifiedTime() == 0) {
                const QDateTime modifiedTime = fileInfo.lastModified();
                updatedTrack.setModifiedTime(modifiedTime.isValid() ? modifiedTime.toMSecsSinceEpoch() : 0);
            }
            if(updatedTrack.fileSize() == 0) {
                updatedTrack.setFileSize(fileInfo.size());
            }

            updatedTrack.generateHash();
            tracksToUpdate.push_back(updatedTrack);
        }

        ++processedTracks;
        emit progressChanged(makeProgress(processedTracks, track.filepath(), static_cast<int>(tracks.size()),
                                          ScanProgress::Phase::ReadingMetadata, 0));
    }

    if(!tracksToUpdate.empty()) {
        emit progressChanged(makeProgress(processedTracks, {}, static_cast<int>(tracks.size()),
                                          ScanProgress::Phase::WritingDatabase, 0));
        m_trackDatabase.updateTracks(tracksToUpdate);
        m_trackDatabase.updateTrackStats(tracksToUpdate);
        emit scanUpdate({.addedTracks = {}, .updatedTracks = tracksToUpdate});
    }

    emit progressChanged(makeProgress(processedTracks, {}, processedTracks, ScanProgress::Phase::Finished, 0));

    setState(Idle);
    emit finished();

    qCInfo(LIB_SCANNER) << "Scan of" << tracks.size() << "tracks took" << timer.elapsedFormatted();
}

void LibraryScanner::scanFiles(const TrackList& libraryTracks, const QList<QUrl>& urls, const LibraryScanConfig& config)
{
    setState(Running);

    m_progressOnlyModified = false;
    startSession(config);

    const Timer timer;

    LibraryScanFilesResult result;

    const bool completed = m_session->scanFiles(libraryTracks, urls, result);

    if(completed && !result.playlistTracksScanned.empty()) {
        emit progressChanged(
            makeProgress(static_cast<int>(m_session->progressCount()), {}, static_cast<int>(m_session->progressCount()),
                         ScanProgress::Phase::WritingDatabase, static_cast<int>(m_session->discoveredFiles())));
        m_trackDatabase.storeTracks(result.playlistTracksScanned);
        emit playlistLoaded(result.playlistTracksScanned);
    }

    if(completed && !result.tracksScanned.empty()) {
        emit progressChanged(
            makeProgress(static_cast<int>(m_session->progressCount()), {}, static_cast<int>(m_session->progressCount()),
                         ScanProgress::Phase::WritingDatabase, static_cast<int>(m_session->discoveredFiles())));
        m_trackDatabase.storeTracks(result.tracksScanned);
        emit scannedTracks(result.tracksScanned);
    }

    const int scannedFiles = static_cast<int>(m_session->filesScanned());

    finishSession();
    clearSession();

    setState(Idle);
    emit finished();

    qCInfo(LIB_SCANNER) << "Scan of" << scannedFiles << "files took" << timer.elapsedFormatted();
}

void LibraryScanner::scanPlaylist(const TrackList& libraryTracks, const QList<QUrl>& urls,
                                  const LibraryScanConfig& config)
{
    setState(Running);

    m_progressOnlyModified = false;
    startSession(config);

    const Timer timer;

    TrackList tracksScanned;

    const bool completed = m_session->scanPlaylist(libraryTracks, urls, tracksScanned);

    if(completed && !tracksScanned.empty()) {
        emit progressChanged(
            makeProgress(static_cast<int>(m_session->progressCount()), {}, static_cast<int>(m_session->progressCount()),
                         ScanProgress::Phase::WritingDatabase, static_cast<int>(m_session->discoveredFiles())));
        m_trackDatabase.storeTracks(tracksScanned);
        emit playlistLoaded(tracksScanned);
    }

    finishSession();
    clearSession();

    setState(Idle);
    emit finished();

    qCInfo(LIB_SCANNER) << "Scan of playlist took" << timer.elapsedFormatted();
}
} // namespace Fooyin

#include "moc_libraryscanner.cpp"
