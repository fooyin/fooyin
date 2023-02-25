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

#include "librarydatabasemanager.h"

#include "core/coresettings.h"
#include "core/database/database.h"
#include "core/database/librarydatabase.h"
#include "core/tagging/tags.h"

#include <utils/settings/settingsmanager.h>

namespace Core::Library {
LibraryDatabaseManager::LibraryDatabaseManager(DB::Database* database, Utils::SettingsManager* settings,
                                               QObject* parent)
    : Worker{parent}
    , m_database{database}
    , m_libraryDatabase{m_database->libraryDatabase()}
    , m_settings{settings}
{ }

LibraryDatabaseManager::~LibraryDatabaseManager()
{
    m_database->closeDatabase();
}

void LibraryDatabaseManager::getAllTracks(SortOrder order)
{
    TrackList tracks;
    const int limit = m_settings->value<Settings::LazyTracks>();
    int offset      = 0;

    if(limit > 0) {
        while(m_libraryDatabase->getAllTracks(tracks, order, offset, limit)) {
            offset += limit;
            emit gotTracks(tracks);
        }
    }
    else {
        if(m_libraryDatabase->getAllTracks(tracks, order)) {
            emit gotTracks(tracks);
        }
    }
}

void LibraryDatabaseManager::updateTracks(const TrackPtrList& tracks)
{
    for(const auto& track : tracks) {
        const bool saved = Tagging::writeMetaData(*track);
        if(saved) {
            m_libraryDatabase->updateTrack(*track);
        }
    }
}
} // namespace Core::Library
