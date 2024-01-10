/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include "database/database.h"
#include "database/trackdatabase.h"

#include <core/track.h>

namespace Fooyin {
TrackDatabaseManager::TrackDatabaseManager(Database* database, QObject* parent)
    : Worker{parent}
    , m_database{database}
    , m_trackDatabase{database->connectionName()}
{ }

void TrackDatabaseManager::closeThread()
{
    m_database->closeDatabase();
}

void TrackDatabaseManager::getAllTracks()
{
    const TrackList tracks = m_trackDatabase.getAllTracks();

    if(!tracks.empty()) {
        emit gotTracks(tracks);
    }
}

void TrackDatabaseManager::updateTracks(const TrackList& tracks)
{
    TrackList tracksUpdated;

    for(const Track& track : tracks) {
        if(m_tagWriter.writeMetaData(track) && m_trackDatabase.updateTrack(track)) {
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
    if(m_trackDatabase.updateTrackStats(updatedTrack)) {
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
