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

#include <core/coresettings.h>
#include <core/tagging/tagloader.h>
#include <core/track.h>
#include <utils/database/dbconnectionhandler.h>
#include <utils/settings/settingsmanager.h>

#include <QFileInfo>

namespace Fooyin {
TrackDatabaseManager::TrackDatabaseManager(DbConnectionPoolPtr dbPool, std::shared_ptr<TagLoader> tagLoader,
                                           SettingsManager* settings, QObject* parent)
    : Worker{parent}
    , m_dbPool{std::move(dbPool)}
    , m_tagLoader{std::move(tagLoader)}
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
    const TrackList tracks = m_trackDatabase.getAllTracks();
    emit gotTracks(tracks);
}

void TrackDatabaseManager::updateTracks(const TrackList& tracks, bool write)
{
    TrackList tracksUpdated;

    TagParser::WriteOptions options;
    options.writeRating    = m_settings->value<Settings::Core::SaveRatingToMetadata>();
    options.writePlaycount = m_settings->value<Settings::Core::SavePlaycountToMetadata>();

    for(const Track& track : tracks) {
        Track updatedTrack{track};

        if(write) {
            if(auto* parser = m_tagLoader->parserForTrack(updatedTrack)) {
                if(!parser->writeMetaData(updatedTrack, options)) {
                    qWarning() << "Failed to write metadata to file:" << updatedTrack.filepath();
                    continue;
                }
                const QDateTime modifiedTime = QFileInfo{updatedTrack.filepath()}.lastModified();
                updatedTrack.setModifiedTime(modifiedTime.isValid() ? modifiedTime.toMSecsSinceEpoch() : 0);
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

    TagParser::WriteOptions options;
    options.writeRating    = m_settings->value<Settings::Core::SaveRatingToMetadata>();
    options.writePlaycount = m_settings->value<Settings::Core::SavePlaycountToMetadata>();

    const bool writeToFile = options.writeRating || options.writePlaycount;

    for(const Track& track : tracks) {
        Track updatedTrack{track};
        bool success{true};
        if(writeToFile) {
            if(auto* parser = m_tagLoader->parserForTrack(updatedTrack)) {
                success = parser->writeMetaData(updatedTrack, options);
            }
        }
        if(success && m_trackDatabase.updateTrackStats(updatedTrack)) {
            const QDateTime modifiedTime = QFileInfo{updatedTrack.filepath()}.lastModified();
            updatedTrack.setModifiedTime(modifiedTime.isValid() ? modifiedTime.toMSecsSinceEpoch() : 0);
            tracksUpdated.emplace_back(updatedTrack);
        }
        else {
            qWarning() << "Failed to update track playback statistics:" << updatedTrack.filepath();
        }
    }

    if(!tracksUpdated.empty()) {
        emit updatedTracks(tracksUpdated);
    }
}

void TrackDatabaseManager::cleanupTracks()
{
    m_trackDatabase.cleanupTracks();
}
} // namespace Fooyin

#include "moc_trackdatabasemanager.cpp"
