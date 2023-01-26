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

#include "filtermanager.h"

#include "filterdatabasemanager.h"

#include <core/app/threadmanager.h>
#include <core/library/musiclibrary.h>
#include <utils/helpers.h>

#include <utility>

namespace Filters {
struct FilterManager::Private
{
    Core::ThreadManager* threadManager;
    Core::Library::MusicLibrary* library;

    FilterDatabaseManager databaseManager;

    Core::TrackPtrList filteredTracks;
    LibraryFilters filters;
    ActiveFilters activeFilters;
    QString searchFilter;

    Private(Core::ThreadManager* threadManager, Core::DB::Database* database, Core::Library::MusicLibrary* library)
        : threadManager{threadManager}
        , library{library}
        , databaseManager{database}
    {
        threadManager->moveToNewThread(&databaseManager);
    }
};

FilterManager::FilterManager(Core::ThreadManager* threadManager, Core::DB::Database* database,
                             Core::Library::MusicLibrary* library, QObject* parent)
    : MusicLibraryInteractor{parent}
    , p{std::make_unique<Private>(threadManager, database, library)}
{
    connect(this, &FilterManager::loadAllItems, &p->databaseManager, &FilterDatabaseManager::getAllItems);
    connect(this, &FilterManager::loadItemsByFilter, &p->databaseManager, &FilterDatabaseManager::getItemsByFilter);
    connect(&p->databaseManager, &FilterDatabaseManager::gotItems, this, &FilterManager::itemsHaveLoaded);

    connect(this, &FilterManager::loadFilteredTracks, &p->databaseManager, &FilterDatabaseManager::filterTracks);
    connect(&p->databaseManager, &FilterDatabaseManager::tracksFiltered, this, &FilterManager::filteredTracksLoaded);

    connect(this, &FilterManager::filteredTracks, p->library, &Core::Library::MusicLibrary::tracksChanged);
    connect(p->library, &Core::Library::MusicLibrary::tracksUpdated, this, [this] {
        emit filteredItems();
    });
    connect(p->library, &Core::Library::MusicLibrary::tracksAdded, this, [this] {
        emit filteredItems();
    });
    connect(p->library, &Core::Library::MusicLibrary::tracksDeleted, this, [this] {
        emit filteredItems();
    });
    connect(p->library, &Core::Library::MusicLibrary::libraryRemoved, this, [this] {
        emit filteredItems();
    });

    p->library->addInteractor(this);
}

FilterManager::~FilterManager() = default;

Core::TrackPtrList FilterManager::tracks() const
{
    return p->filteredTracks;
}

bool FilterManager::hasTracks() const
{
    return !p->filteredTracks.empty() || !p->searchFilter.isEmpty() || !p->activeFilters.empty();
}

LibraryFilters FilterManager::filters() const
{
    return p->filters;
}

bool FilterManager::hasFilter(Filters::FilterType type) const
{
    return std::any_of(p->filters.cbegin(), p->filters.cend(), [type](const LibraryFilter& filter) {
        return filter.type == type;
    });
}

bool FilterManager::filterIsActive(FilterType type) const
{
    return p->activeFilters.count(type);
}

LibraryFilter FilterManager::findFilter(Filters::FilterType type) const
{
    auto it = std::find_if(p->filters.cbegin(), p->filters.cend(), [type](const LibraryFilter& filter) {
        return filter.type == type;
    });
    if(it != p->filters.end()) {
        return *it;
    }
    return {};
}

int FilterManager::registerFilter(Filters::FilterType type)
{
    LibraryFilter filter;
    if(hasFilter(type)) {
        filter = findFilter(type);
    }
    else {
        filter.sortOrder = Core::Library::SortOrder::TitleAsc;
        filter.type      = type;
        filter.index     = static_cast<int>(p->filters.size());
        p->filters.emplace_back(filter);
    }
    return filter.index;
}

void FilterManager::unregisterFilter(Filters::FilterType type)
{
    p->filters.erase(std::remove_if(p->filters.begin(), p->filters.end(),
                                    [type](const auto& filter) {
                                        return filter.type == type;
                                    }),
                     p->filters.end());
    p->activeFilters.erase(type);
    emit filteredItems(-1);
    getFilteredTracks();
}

void FilterManager::changeFilter(FilterType oldType, FilterType type)
{
    const LibraryFilter filter = findFilter(oldType);
    if(filterIsActive(oldType)) {
        p->activeFilters.erase(oldType);
    }
    int index = filter.index;
    std::for_each(p->filters.begin(), p->filters.end(), [oldType, type, this, &index](LibraryFilter& filter) {
        if(filter.type == oldType) {
            filter.type = type;
        }
        if(filter.index > index) {
            if(filterIsActive(filter.type)) {
                p->activeFilters.erase(filter.type);
            }
        }
    });
    emit filteredItems(index);
}

void FilterManager::resetFilter(Filters::FilterType type)
{
    emit filterReset(type, p->activeFilters.at(type));
}

Core::Library::SortOrder FilterManager::filterOrder(Filters::FilterType type) const
{
    for(const auto& filter : p->filters) {
        if(filter.type == type) {
            return filter.sortOrder;
        }
    }
    return {};
}

void FilterManager::changeFilterOrder(Filters::FilterType type, Core::Library::SortOrder order)
{
    for(auto& filter : p->filters) {
        if(filter.type == type) {
            filter.sortOrder = order;
        }
    }
    emit orderedFilter(type);
}

void FilterManager::items(Filters::FilterType type)
{
    if(!p->activeFilters.empty() || !p->searchFilter.isEmpty()) {
        getItemsByFilter(type, findFilter(type).sortOrder);
    }
    else {
        getAllItems(type, findFilter(type).sortOrder);
    }
}

void FilterManager::getAllItems(Filters::FilterType type, Core::Library::SortOrder order)
{
    emit loadAllItems(type, order);
}

void FilterManager::getItemsByFilter(Filters::FilterType type, Core::Library::SortOrder order)
{
    emit loadItemsByFilter(type, p->activeFilters, p->searchFilter, order);
}

void FilterManager::getFilteredTracks()
{
    p->filteredTracks.clear();
    emit loadFilteredTracks(p->library->allTracks(), p->activeFilters, p->searchFilter);
}

bool FilterManager::tracksHaveFiltered()
{
    return !p->filteredTracks.empty();
}

void FilterManager::changeSelection(const Core::IdSet& indexes, Filters::FilterType type, int index)
{
    for(const auto& filter : p->filters) {
        if(index < filter.index) {
            p->activeFilters.erase(filter.type);
        }
    }

    if(Utils::contains(indexes, -1)) {
        p->activeFilters.erase(type);
    }
    else {
        p->activeFilters[type] = indexes;
    }

    if(!p->activeFilters.empty() || !p->searchFilter.isEmpty()) {
        getFilteredTracks();
    }
    else {
        p->filteredTracks.clear();
        emit filteredTracks();
    }
}

void FilterManager::selectionChanged(const Core::IdSet& indexes, Filters::FilterType type, int index)
{
    if(indexes.empty()) {
        return;
    }

    changeSelection(indexes, type, index);

    emit filteredItems(index);
}

void FilterManager::changeSearch(const QString& search)
{
    p->searchFilter = search;
    p->filteredTracks.clear();
    if(search.isEmpty()) {
        if(!p->activeFilters.empty()) {
            getFilteredTracks();
        }
        else {
            emit filteredTracks();
        }
        emit filteredItems(-1);
    }
    else {
        getFilteredTracks();
        emit filteredItems(-1);
    }
}

void FilterManager::searchChanged(const QString& search)
{
    changeSearch(search);
}

void FilterManager::itemsHaveLoaded(Filters::FilterType type, FilterEntries result)
{
    emit itemsLoaded(type, std::move(result));
}

void FilterManager::filteredTracksLoaded(Core::TrackPtrList tracks)
{
    p->filteredTracks = std::move(tracks);
    emit filteredTracks();
}
} // namespace Filters
