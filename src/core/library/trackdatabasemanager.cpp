/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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
    TrackList tracks;

    if(m_trackDatabase.getAllTracks(tracks)) {
        emit gotTracks(tracks);
    }
}
} // namespace Fooyin

#include "moc_trackdatabasemanager.cpp"
