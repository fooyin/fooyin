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

#include "filterdatabasemanager.h"

#include "filterdatabase.h"

#include <core/database/database.h>
#include <utils/helpers.h>

namespace Filters {
void filterByType(Core::Track* track, const ActiveFilters& filters, int& matches)
{
    for(const auto& [filter, ids] : filters) {
        switch(filter) {
            case(Filters::FilterType::AlbumArtist): {
                if(Utils::contains(ids, track->albumArtistId())) {
                    matches += 1;
                }
                break;
            }
            case(Filters::FilterType::Artist): {
                const Core::IdSet artistIds{track->artistIds()};
                for(const auto artistId : artistIds) {
                    if(Utils::contains(ids, artistId)) {
                        matches += 1;
                    }
                }
                break;
            }
            case(Filters::FilterType::Album): {
                if(Utils::contains(ids, track->albumId())) {
                    matches += 1;
                }
                break;
            }
            case(Filters::FilterType::Year): {
                if(Utils::contains(ids, track->year())) {
                    matches += 1;
                }
                break;
            }
            case(Filters::FilterType::Genre): {
                const Core::IdSet genreIds{track->genreIds()};
                for(const int& genreId : genreIds) {
                    if(Utils::contains(ids, genreId)) {
                        matches += 1;
                    }
                }
                break;
            }
        }
    }
}

FilterDatabaseManager::FilterDatabaseManager(Core::DB::Database* database, QObject* parent)
    : Worker{parent}
    , m_database{database}
    , m_filterDatabase{std::make_unique<FilterDatabase>(m_database->connectionName())}
{ }

FilterDatabaseManager::~FilterDatabaseManager()
{
    m_database->closeDatabase();
}

void FilterDatabaseManager::getAllItems(Filters::FilterType type, Core::Library::SortOrder order)
{
    FilterEntries items;
    const bool success = m_filterDatabase->getAllItems(type, order, items);
    if(success) {
        emit gotItems(type, items);
    }
}

void FilterDatabaseManager::getItemsByFilter(Filters::FilterType type, const ActiveFilters& filters,
                                             const QString& search, Core::Library::SortOrder order)
{
    FilterEntries items;
    const bool success = m_filterDatabase->getItemsByFilter(type, filters, search, order, items);
    if(success) {
        emit gotItems(type, items);
    }
}

void FilterDatabaseManager::filterTracks(const Core::TrackPtrList& tracks, const ActiveFilters& filters,
                                         const QString& search)
{
    Core::TrackPtrList filteredTracks;

    for(const auto& track : tracks) {
        int matches     = 0;
        const int total = static_cast<int>(filters.size()) + (search.isEmpty() ? 0 : 1);

        if(!filters.empty()) {
            filterByType(track, filters, matches);
        }

        if(!search.isEmpty()) {
            bool hasArtistMatch = false;
            for(const auto& artist : track->artists()) {
                if(artist.contains(search, Qt::CaseInsensitive)) {
                    hasArtistMatch = true;
                }
            }

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
} // namespace Filters
