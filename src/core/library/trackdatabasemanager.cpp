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
#include <core/track.h>
#include <utils/database/dbconnectionhandler.h>
#include <utils/settings/settingsmanager.h>

#include <QFileInfo>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(TRK_DBMAN, "TrackDatabaseManager")

namespace Fooyin {
TrackDatabaseManager::TrackDatabaseManager(DbConnectionPoolPtr dbPool, std::shared_ptr<AudioLoader> audioLoader,
                                           SettingsManager* settings, QObject* parent)
    : Worker{parent}
    , m_dbPool{std::move(dbPool)}
    , m_audioLoader{std::move(audioLoader)}
    , m_settings{settings}
{ }

void TrackDatabaseManager::initialiseThread()
{
    Worker::initialiseThread();

    m_dbHandler = std::make_unique<DbConnectionHandler>(m_dbPool);
    m_trackDatabase.initialise(DbConnectionProvider{m_dbPool});
}

void TrackDatabaseManager::getAllTracks()
{
    TrackList tracks = m_trackDatabase.getAllTracks();

    if(m_settings->fileValue(Settings::Core::Internal::MarkUnavailableStartup, false).toBool()) {
        std::ranges::for_each(tracks, [](auto& track) { track.setIsEnabled(track.exists()); });
    }

    emit gotTracks(tracks);
}

void TrackDatabaseManager::updateTracks(const TrackList& tracks, bool write)
{
    TrackList tracksUpdated;

    AudioReader::WriteOptions options;
    if(m_settings->value<Settings::Core::SaveRatingToMetadata>()) {
        options |= AudioReader::Rating;
    }
    if(m_settings->value<Settings::Core::SavePlaycountToMetadata>()) {
        options |= AudioReader::Playcount;
    }

    for(const Track& track : tracks) {
        Track updatedTrack{track};

        if(write) {
            if(m_audioLoader->writeTrackMetadata(updatedTrack, options)) {
                const QDateTime modifiedTime = QFileInfo{updatedTrack.filepath()}.lastModified();
                updatedTrack.setModifiedTime(modifiedTime.isValid() ? modifiedTime.toMSecsSinceEpoch() : 0);
            }
            else {
                qCWarning(TRK_DBMAN) << "Failed to write metadata to file:" << updatedTrack.filepath();
                continue;
            }
        }

        if(m_trackDatabase.updateTrack(updatedTrack) && m_trackDatabase.updateTrackStats(updatedTrack)) {
            tracksUpdated.emplace_back(updatedTrack);
        }
    }

    if(!tracksUpdated.empty()) {
        emit updatedTracks(tracksUpdated);
    }
}

void TrackDatabaseManager::updateTrackStats(const TrackList& tracks)
{
    TrackList tracksUpdated;

    AudioReader::WriteOptions options;
    if(m_settings->value<Settings::Core::SaveRatingToMetadata>()) {
        options |= AudioReader::Rating;
    }
    if(m_settings->value<Settings::Core::SavePlaycountToMetadata>()) {
        options |= AudioReader::Playcount;
    }

    const bool writeToFile = options & AudioReader::Rating || options & AudioReader::Playcount;

    for(const Track& track : tracks) {
        Track updatedTrack{track};
        bool success{true};
        if(writeToFile) {
            success = m_audioLoader->writeTrackMetadata(updatedTrack, options);
        }
        if(success && m_trackDatabase.updateTrackStats(updatedTrack)) {
            const QDateTime modifiedTime = QFileInfo{updatedTrack.filepath()}.lastModified();
            updatedTrack.setModifiedTime(modifiedTime.isValid() ? modifiedTime.toMSecsSinceEpoch() : 0);
            tracksUpdated.emplace_back(updatedTrack);
        }
        else {
            qCWarning(TRK_DBMAN) << "Failed to update track playback statistics:" << updatedTrack.filepath();
        }
    }

    if(!tracksUpdated.empty()) {
        emit updatedTracksStats(tracksUpdated);
    }
}

void TrackDatabaseManager::cleanupTracks()
{
    m_trackDatabase.cleanupTracks();
}
} // namespace Fooyin

#include "moc_trackdatabasemanager.cpp"
