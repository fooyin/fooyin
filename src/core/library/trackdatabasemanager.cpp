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

#include "trackdatabasemanager.h"

#include "database/trackdatabase.h"
#include "internalcoresettings.h"

#include <core/coresettings.h>
#include <core/engine/audioloader.h>
#include <core/library/musiclibrary.h>
#include <core/track.h>
#include <utils/database/dbconnectionhandler.h>
#include <utils/settings/settingsmanager.h>

#include <QFileInfo>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(TRK_DBMAN, "fy.trackdbmanager")

namespace {
bool shouldContinue(const std::stop_token& stopToken)
{
    return !stopToken.stop_requested();
}
} // namespace

namespace Fooyin {
TrackDatabaseManager::TrackDatabaseManager(DbConnectionPoolPtr dbPool, std::shared_ptr<AudioLoader> audioLoader,
                                           SettingsManager* settings, std::shared_ptr<TrackMetadataStore> metadataStore,
                                           QObject* parent)
    : Worker{parent}
    , m_dbPool{std::move(dbPool)}
    , m_audioLoader{std::move(audioLoader)}
    , m_settings{settings}
    , m_metadataStore{std::move(metadataStore)}
{ }

void TrackDatabaseManager::initialiseThread()
{
    Worker::initialiseThread();

    m_dbHandler = std::make_unique<DbConnectionHandler>(m_dbPool);
    m_trackDatabase.initialise(DbConnectionProvider{m_dbPool});
    m_trackDatabase.setMetadataStore(m_metadataStore);
}

void TrackDatabaseManager::getAllTracks()
{
    setState(Running);

    TrackList tracks = m_trackDatabase.getAllTracks();

    if(m_settings->fileValue(Settings::Core::Internal::MarkUnavailableStartup, false).toBool()) {
        std::ranges::for_each(tracks, [](auto& track) { track.setIsEnabled(track.exists()); });
    }

    emit gotTracks(tracks);

    setState(Idle);
}

void TrackDatabaseManager::updateTracks(const TrackList& tracks, bool write)
{
    updateTracks(tracks, write, -1, {});
}

void TrackDatabaseManager::updateTracks(const TrackList& tracks, bool write, int operationId, std::stop_token stopToken)
{
    setState(Running);

    TrackList tracksToUpdate{tracks};
    TrackList tracksUpdated;
    int failedCount{0};
    bool cancelled{false};

    AudioReader::WriteOptions options;

    if(write) {
        options |= AudioReader::Metadata;
        if(m_settings->value<Settings::Core::SaveRatingToMetadata>()) {
            options |= AudioReader::Rating;
        }
        if(m_settings->value<Settings::Core::SavePlaycountToMetadata>()) {
            options |= AudioReader::Playcount;
        }
        if(m_settings->value<Settings::Core::PreserveTimestamps>()) {
            options |= AudioReader::PreserveTimestamps;
        }
    }

    for(const Track& track : std::as_const(tracksToUpdate)) {
        if(!shouldContinue(stopToken) || !mayRun()) {
            cancelled = true;
            break;
        }

        Track updatedTrack{track};

        if(write) {
            if(m_audioLoader->writeTrackMetadata(updatedTrack, options)) {
                const QDateTime modifiedTime = QFileInfo{updatedTrack.filepath()}.lastModified();
                updatedTrack.setModifiedTime(modifiedTime.isValid() ? modifiedTime.toMSecsSinceEpoch() : 0);
            }
            else {
                qCWarning(TRK_DBMAN) << "Failed to write metadata to file:" << updatedTrack.filepath();
                ++failedCount;
                continue;
            }
        }

        if(m_trackDatabase.updateTrack(updatedTrack) && m_trackDatabase.updateTrackStats(updatedTrack)) {
            tracksUpdated.push_back(updatedTrack);
        }
        else {
            ++failedCount;
        }
    }

    emit updatedTracks(tracksUpdated);
    if(operationId >= 0) {
        emit trackWriteCompleted(operationId, tracksUpdated, failedCount, cancelled);
    }

    setState(Idle);
}

void TrackDatabaseManager::updateTrackStats(const TrackList& tracks, AudioReader::WriteOptions requestedWriteOptions)
{
    setState(Running);

    TrackList tracksToUpdate{tracks};
    TrackList tracksUpdated;

    AudioReader::WriteOptions options{AudioReader::None};

    if(m_settings->value<Settings::Core::SaveRatingToMetadata>()) {
        options |= AudioReader::Rating;
    }
    if(m_settings->value<Settings::Core::SavePlaycountToMetadata>()) {
        options |= AudioReader::Playcount;
    }

    AudioReader::WriteOptions writeOptions = requestedWriteOptions & options;
    if(writeOptions != AudioReader::None && m_settings->value<Settings::Core::PreserveTimestamps>()) {
        writeOptions |= AudioReader::PreserveTimestamps;
    }

    for(const Track& track : std::as_const(tracksToUpdate)) {
        if(!mayRun()) {
            break;
        }

        Track updatedTrack{track};
        bool success{true};
        if(!track.isInArchive() && writeOptions != AudioReader::None) {
            success = m_audioLoader->writeTrackMetadata(updatedTrack, writeOptions);
        }
        if(success && m_trackDatabase.updateTrackStats(updatedTrack)) {
            const QDateTime modifiedTime = QFileInfo{updatedTrack.filepath()}.lastModified();
            updatedTrack.setModifiedTime(modifiedTime.isValid() ? modifiedTime.toMSecsSinceEpoch() : 0);
            tracksUpdated.push_back(updatedTrack);
        }
        else {
            qCWarning(TRK_DBMAN) << "Failed to update track playback statistics:" << updatedTrack.filepath();
        }
    }

    if(!tracksUpdated.empty()) {
        emit updatedTracksStats(tracksUpdated);
    }

    setState(Idle);
}

void TrackDatabaseManager::writeCovers(const TrackCoverData& tracks)
{
    writeCovers(tracks, -1, {});
}

void TrackDatabaseManager::writeCovers(const TrackCoverData& tracks, int operationId, std::stop_token stopToken)
{
    setState(Running);

    TrackList tracksToUpdate{tracks.tracks};
    TrackList tracksUpdated;
    int failedCount{0};
    bool cancelled{false};

    AudioReader::WriteOptions options;
    if(m_settings->value<Settings::Core::PreserveTimestamps>()) {
        options |= AudioReader::PreserveTimestamps;
    }

    for(const auto& track : std::as_const(tracksToUpdate)) {
        if(!shouldContinue(stopToken) || !mayRun()) {
            cancelled = true;
            break;
        }

        if(track.isInArchive()) {
            ++failedCount;
            continue;
        }

        Track updatedTrack{track};
        if(m_audioLoader->writeTrackCover(updatedTrack, tracks.coverData, options)) {
            const QDateTime modifiedTime = QFileInfo{updatedTrack.filepath()}.lastModified();
            updatedTrack.setModifiedTime(modifiedTime.isValid() ? modifiedTime.toMSecsSinceEpoch() : 0);

            if(m_trackDatabase.updateTrack(updatedTrack)) {
                tracksUpdated.push_back(updatedTrack);
            }
            else {
                ++failedCount;
            }
        }
        else {
            qCWarning(TRK_DBMAN) << "Failed to update track covers:" << updatedTrack.filepath();
            ++failedCount;
        }
    }

    emit updatedTracks(tracksUpdated);
    if(operationId >= 0) {
        emit trackCoverWriteCompleted(operationId, tracksUpdated, failedCount, cancelled);
    }

    setState(Idle);
}

void TrackDatabaseManager::removeUnavailbleTracks(const TrackList& tracks)
{
    removeUnavailbleTracks(tracks, -1, {});
}

void TrackDatabaseManager::removeUnavailbleTracks(const TrackList& tracks, int operationId, std::stop_token stopToken)
{
    setState(Running);

    TrackList trackstoRemove;
    int failedCount{0};
    bool cancelled{false};

    for(const Track& track : tracks) {
        if(!shouldContinue(stopToken) || !mayRun()) {
            cancelled = true;
            break;
        }

        if(!QFileInfo::exists(track.isInArchive() ? track.archivePath() : track.filepath())) {
            trackstoRemove.push_back(track);
        }
    }

    if(!trackstoRemove.empty() && m_trackDatabase.deleteTracks(trackstoRemove)) {
        emit removedTracks(trackstoRemove);
    }
    else if(!trackstoRemove.empty()) {
        failedCount = static_cast<int>(trackstoRemove.size());
    }

    if(operationId >= 0) {
        emit unavailableTracksRemoved(operationId, trackstoRemove, failedCount, cancelled);
    }

    setState(Idle);
}

void TrackDatabaseManager::cleanupTracks()
{
    setState(Running);

    m_settings->set<Settings::Core::ActiveTrackId>(-2);
    m_trackDatabase.cleanupTracks();

    setState(Idle);
}
} // namespace Fooyin

#include "moc_trackdatabasemanager.cpp"
