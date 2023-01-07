/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "filterdatabasemanager.h"

#include "filterdatabase.h"

#include <core/database/database.h>
#include <utils/helpers.h>

FilterDatabaseManager::FilterDatabaseManager(QObject* parent)
    : Worker(parent)
    , m_filterDatabase(new DB::FilterDatabase(DB::Database::instance()->connectionName()))
{ }

FilterDatabaseManager::~FilterDatabaseManager()
{
    DB::Database::instance()->closeDatabase();
}

void FilterDatabaseManager::getAllItems(Filters::FilterType type, Library::SortOrder order)
{
    FilterList items;
    bool success = m_filterDatabase->getAllItems(type, order, items);
    if(success) {
        emit gotItems(type, items);
    }
}

void FilterDatabaseManager::getItemsByFilter(Filters::FilterType type, const ActiveFilters& filters,
                                             const QString& search, Library::SortOrder order)
{
    FilterList items;
    bool success = m_filterDatabase->getItemsByFilter(type, filters, search, order, items);
    if(success) {
        emit gotItems(type, items);
    }
}

void FilterDatabaseManager::filterTracks(const TrackPtrList& tracks, const ActiveFilters& filters,
                                         const QString& search)
{
    TrackPtrList filteredTracks;

    for(const auto& track : tracks) {
        int matches = 0;
        int total = static_cast<int>(filters.size()) + (search.isEmpty() ? 0 : 1);

        if(!filters.isEmpty()) {
            for(const auto& [filter, ids] : asRange(filters)) {
                switch(filter) {
                    case(Filters::FilterType::AlbumArtist): {
                        if(ids.contains(track->albumArtistId())) {
                            ++matches;
                        }
                        break;
                    }
                    case(Filters::FilterType::Artist): {
                        const IdSet artistIds{track->artistIds()};
                        for(const auto artistId : artistIds) {
                            if(ids.contains(artistId)) {
                                ++matches;
                            }
                        }
                        break;
                    }
                    case(Filters::FilterType::Album): {
                        if(ids.contains(track->albumId())) {
                            ++matches;
                        }
                        break;
                    }
                    case(Filters::FilterType::Year): {
                        if(ids.contains(track->year())) {
                            ++matches;
                        }
                        break;
                    }
                    case(Filters::FilterType::Genre): {
                        const IdSet genreIds{track->genreIds()};
                        for(const int& genreId : genreIds) {
                            if(ids.contains(genreId)) {
                                ++matches;
                            }
                        }
                        break;
                    }
                }
            }
        }

        if(!search.isEmpty()) {
            bool hasArtistMatch = false;
            for(const auto& artist : track->artists()) {
                if(artist.contains(search, Qt::CaseInsensitive)) {
                    hasArtistMatch = true;
                }
            };

            if(track->title().contains(search, Qt::CaseInsensitive)
               || track->album().contains(search, Qt::CaseInsensitive)
               || track->albumArtist().contains(search, Qt::CaseInsensitive) || hasArtistMatch) {
                ++matches;
            }
        }
        if(matches == total) {
            filteredTracks.emplace_back(track);
        }
    }
    emit tracksFiltered(filteredTracks);
}
