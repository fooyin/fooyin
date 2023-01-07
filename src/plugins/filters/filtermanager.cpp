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

struct FilterManager::Private
{
    FilterDatabaseManager databaseManager;
    ThreadManager* threadManager;
    Library::MusicLibrary* library;
    TrackPtrList filteredTracks;
    std::vector<Filters::FilterType> filters;
    QMap<int, Filters::FilterType> filterIndexes;
    ActiveFilters activeFilters;
    QString searchFilter;
    FilterSortOrders filterSortOrders;

    Private()
        : threadManager{PluginSystem::object<ThreadManager>()}
        , library{PluginSystem::object<Library::MusicLibrary>()}
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

    connect(this, &FilterManager::filteredTracks, p->library, &Library::MusicLibrary::tracksChanged);
    connect(p->library, &Library::MusicLibrary::tracksChanged, this, [this] {
        emit filteredItems();
    });
    connect(p->library, &Library::MusicLibrary::tracksAdded, this, [this] {
        emit filteredItems();
    });
    connect(p->library, &Library::MusicLibrary::tracksDeleted, this, [this] {
        emit filteredItems();
    });
    connect(p->library, &Library::MusicLibrary::libraryRemoved, this, [this] {
        emit filteredItems();
    });
}

TrackPtrList FilterManager::tracks()
{
    return p->filteredTracks;
}

bool FilterManager::hasTracks()
{
    return !p->filteredTracks.empty() || !p->searchFilter.isEmpty() || !p->activeFilters.empty();
}

QList<Filters::FilterType> FilterManager::filters()
{
    return p->filterIndexes.values();
};

FilterManager::~FilterManager() = default;

int FilterManager::registerFilter(Filters::FilterType type)
{
    p->filters.emplace_back(type);
    p->filterSortOrders.insert(type, Library::SortOrder::TitleAsc);
    p->library->addInteractor(this);
    return static_cast<int>(p->filters.size() - 1);
}

void FilterManager::unregisterFilter(int index)
{
    auto idx = p->filterIndexes.take(index);
    p->activeFilters.remove(idx);
    emit filteredItems(-1);
    getFilteredTracks();
}

void FilterManager::changeFilter(int index)
{
    if(!p->activeFilters.isEmpty()) {
        for(const auto& [filterIndex, filter] : asRange(p->filterIndexes)) {
            if(index <= filterIndex) {
                p->activeFilters.remove(filter);
            }
        }
    }
    emit filteredItems(index - 1);
}

void FilterManager::resetFilter(Filters::FilterType type)
{
    emit filterReset(type, p->activeFilters.value(type));
}

Library::SortOrder FilterManager::filterOrder(Filters::FilterType type)
{
    return p->filterSortOrders.value(type);
}

void FilterManager::changeFilterOrder(Filters::FilterType type, Library::SortOrder order)
{
    p->filterSortOrders.insert(type, order);
    emit orderedFilter(type);
}

void FilterManager::items(Filters::FilterType type)
{
    if(!p->activeFilters.isEmpty() || !p->searchFilter.isEmpty()) {
        getItemsByFilter(type, p->filterSortOrders.value(type));
    }
    else {
        getAllItems(type, p->filterSortOrders.value(type));
    }
}

void FilterManager::getAllItems(Filters::FilterType type, Library::SortOrder order)
{
    emit loadAllItems(type, order);
}

void FilterManager::getItemsByFilter(Filters::FilterType type, Library::SortOrder order)
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

void FilterManager::changeSelection(const IdSet& indexes, Filters::FilterType type, int index)
{
    if(p->activeFilters.isEmpty() && indexes.contains(-1)) {
        return;
    }

    for(const auto& [filterIndex, filter] : asRange(p->filterIndexes)) {
        if(index < filterIndex) {
            p->activeFilters.remove(filter);
        }
    }

    if(indexes.contains(-1)) {
        p->activeFilters.remove(type);
    }
    else {
        p->activeFilters.insert(type, indexes);
    }

    const IdSet& filterIds = p->activeFilters.value(type);

    if((!p->activeFilters.isEmpty() && !filterIds.contains(-1)) || !p->searchFilter.isEmpty()) {
        getFilteredTracks();
    }

    else {
        p->filteredTracks.clear();
        emit filteredTracks();
    }
}

void FilterManager::selectionChanged(const IdSet& indexes, Filters::FilterType type, int index)
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
        if(!p->activeFilters.isEmpty()) {
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

void FilterManager::itemsHaveLoaded(Filters::FilterType type, FilterList result)
{
    emit itemsLoaded(type, std::move(result));
}

void FilterManager::filteredTracksLoaded(TrackPtrList tracks)
{
    p->filteredTracks = std::move(tracks);
    emit filteredTracks();
}
