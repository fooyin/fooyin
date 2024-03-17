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
#include "tagging/tagwriter.h"

#include <core/track.h>
#include <utils/database/dbconnectionhandler.h>

namespace Fooyin {
TrackDatabaseManager::TrackDatabaseManager(DbConnectionPoolPtr dbPool, QObject* parent)
    : Worker{parent}
    , m_dbPool{dbPool}
{ }

void TrackDatabaseManager::initialiseThread()
{
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
        if(Tagging::writeMetaData(track) && m_trackDatabase.updateTrack(track)) {
            Track updatedTrack{track};

            if(m_trackDatabase.updateTrackStats(updatedTrack)) {
                const TrackList hashTracks = m_trackDatabase.tracksByHash(updatedTrack.hash());
                std::ranges::copy(hashTracks, std::back_inserter(tracksUpdated));
            }
            else {
                tracksUpdated.push_back(updatedTrack);
            }
        }
    }

    if(!tracksUpdated.empty()) {
        emit updatedTracks(tracksUpdated);
    }
}

void TrackDatabaseManager::updateTrackStats(const Track& track)
{
    TrackList tracksUpdated;

    Track updatedTrack{track};
    if(!track.isInDatabase() || m_trackDatabase.updateTrackStats(updatedTrack)) {
        const TrackList hashTracks = m_trackDatabase.tracksByHash(updatedTrack.hash());
        std::ranges::copy(hashTracks, std::back_inserter(tracksUpdated));
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
