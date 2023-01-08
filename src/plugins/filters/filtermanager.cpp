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

#include "filtermanager.h"

#include "filterdatabasemanager.h"

#include <core/app/threadmanager.h>
#include <core/library/musiclibrary.h>
#include <pluginsystem/pluginmanager.h>
#include <utility>
#include <utils/helpers.h>

namespace Filters {
struct FilterManager::Private
{
    FilterDatabaseManager databaseManager;
    Core::ThreadManager* threadManager;
    Core::Library::MusicLibrary* library;
    Core::TrackPtrList filteredTracks;
    LibraryFilters filters;
    ActiveFilters activeFilters;
    QString searchFilter;

    Private()
        : threadManager{PluginSystem::object<Core::ThreadManager>()}
        , library{PluginSystem::object<Core::Library::MusicLibrary>()}
    {
        threadManager->moveToNewThread(&databaseManager);
    }
};

FilterManager::FilterManager(QObject* parent)
    : MusicLibraryInteractor{parent}
    , p{std::make_unique<Private>()}
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
    return std::any_of(p->filters.cbegin(), p->filters.cend(), [type](const LibraryFilter& f) {
        return f.type == type;
    });
}

LibraryFilter FilterManager::findFilter(Filters::FilterType type) const
{
    for(const auto& filter : p->filters) {
        if(filter.type == type) {
            return filter;
        }
    }
    return {};
};

FilterManager::~FilterManager() = default;

int FilterManager::registerFilter(Filters::FilterType type)
{
    LibraryFilter filter;
    if(hasFilter(type)) {
        filter = findFilter(type);
    }
    else {
        filter.sortOrder = Core::Library::SortOrder::TitleAsc;
        filter.type = type;
        filter.index = static_cast<int>(p->filters.size());
        p->filters.emplace_back(filter);
    }
    return filter.index;
}

void FilterManager::unregisterFilter(Filters::FilterType type)
{
    p->filters.erase(std::remove_if(p->filters.begin(), p->filters.end(),
                                    [type](const auto& f) {
                                        return f.type == type;
                                    }),
                     p->filters.end());
    p->activeFilters.erase(type);
    emit filteredItems(-1);
    getFilteredTracks();
}

void FilterManager::changeFilter(int index)
{
    if(!p->activeFilters.empty()) {
        for(const auto& filter : p->filters) {
            if(index <= filter.index) {
                p->activeFilters.erase(filter.type);
            }
        }
    }
    emit filteredItems(index - 1);
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
    const bool filterAll = Utils::contains(indexes, -1);

    for(const auto& filter : p->filters) {
        if(index < filter.index) {
            p->activeFilters.erase(filter.type);
        }
    }

    if(filterAll) {
        p->activeFilters.erase(type);
    }
    else {
        p->activeFilters[type] = indexes;
    }

    if((!p->activeFilters.empty() && !filterAll) || !p->searchFilter.isEmpty()) {
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
}; // namespace Filters
