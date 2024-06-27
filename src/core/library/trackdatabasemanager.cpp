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

#include <core/tagging/tagloader.h>
#include <core/track.h>
#include <utils/database/dbconnectionhandler.h>

#include <QFileInfo>

namespace Fooyin {
TrackDatabaseManager::TrackDatabaseManager(DbConnectionPoolPtr dbPool, std::shared_ptr<TagLoader> tagLoader,
                                           QObject* parent)
    : Worker{parent}
    , m_dbPool{std::move(dbPool)}
    , m_tagLoader{std::move(tagLoader)}
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

void TrackDatabaseManager::updateTracks(const TrackList& tracks)
{
    TrackList tracksUpdated;

    for(const Track& track : tracks) {
        Track updatedTrack{track};
        if(auto* parser = m_tagLoader->parserForTrack(updatedTrack)) {
            if(parser->writeMetaData(updatedTrack) && m_trackDatabase.updateTrack(updatedTrack)) {
                const QDateTime modifiedTime = QFileInfo{updatedTrack.filepath()}.lastModified();
                updatedTrack.setModifiedTime(modifiedTime.isValid() ? modifiedTime.toMSecsSinceEpoch() : 0);
                tracksUpdated.emplace_back(updatedTrack);
            }
        }
    }

    if(!tracksUpdated.empty()) {
        emit updatedTracks(tracksUpdated);
    }
}

void TrackDatabaseManager::updateTrackStats(const TrackList& tracks)
{
    m_trackDatabase.updateTrackStats(tracks);
}

void TrackDatabaseManager::cleanupTracks()
{
    m_trackDatabase.cleanupTracks();
}
} // namespace Fooyin

#include "moc_trackdatabasemanager.cpp"
