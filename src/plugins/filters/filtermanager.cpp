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

#include <core/library/musiclibrary.h>

#include <utils/helpers.h>
#include <utils/threadmanager.h>

namespace Fy::Filters {
FilterManager::FilterManager(Utils::ThreadManager* threadManager, Core::Library::MusicLibrary* library, QObject* parent)
    : LibraryInteractor{parent}
    , m_threadManager{threadManager}
    , m_library{library}
{
    m_threadManager->moveToNewThread(&m_searchManager);

    connect(this, &FilterManager::filteredTracks, m_library, &Core::Library::MusicLibrary::tracksChanged);

    connect(this, &FilterManager::filterTracks, &m_searchManager, &TrackFilterer::filterTracks);
    connect(&m_searchManager, &TrackFilterer::tracksFiltered, this, &FilterManager::tracksFiltered);

    connect(m_library, &Core::Library::MusicLibrary::tracksLoaded, this, &FilterManager::tracksChanged);
    connect(m_library, &Core::Library::MusicLibrary::tracksUpdated, this, &FilterManager::tracksChanged);
    connect(m_library, &Core::Library::MusicLibrary::tracksAdded, this, &FilterManager::tracksChanged);
    connect(m_library, &Core::Library::MusicLibrary::tracksDeleted, this, &FilterManager::tracksChanged);
    connect(m_library, &Core::Library::MusicLibrary::libraryChanged, this, &FilterManager::tracksChanged);
    connect(m_library, &Core::Library::MusicLibrary::libraryRemoved, this, &FilterManager::tracksChanged);

    m_library->addInteractor(this);
}

Core::TrackPtrList FilterManager::tracks() const
{
    return hasTracks() ? m_filteredTracks : m_library->allTracks();
}

bool FilterManager::hasTracks() const
{
    return !m_filteredTracks.empty() || !m_searchFilter.isEmpty() || m_filterStore.hasActiveFilters();
}

bool FilterManager::hasFilter(Filters::FilterType type) const
{
    return m_filterStore.hasFilter(type);
}

LibraryFilter* FilterManager::registerFilter(Filters::FilterType type)
{
    if(auto* filter = m_filterStore.find(type)) {
        return filter;
    }
    return &m_filterStore.addFilter(type);
}

void FilterManager::unregisterFilter(Filters::FilterType type)
{
    m_filterStore.removeFilter(type);
    emit filteredItems(-1);
    getFilteredTracks();
}

void FilterManager::changeFilter(int index)
{
    for(auto& filter : m_filterStore.activeFilters()) {
        if(filter.index > index) {
            m_filterStore.removeFilter(filter.type);
        }
    };
    emit filteredItems(index);
}

void FilterManager::getFilteredTracks()
{
    m_filteredTracks.clear();

    for(auto& filter : m_filterStore.activeFilters()) {
        if(m_filteredTracks.empty()) {
            m_filteredTracks.insert(m_filteredTracks.cend(), filter.tracks.cbegin(), filter.tracks.cend());
        }
        else {
            m_filteredTracks = Utils::intersection<Core::Track*>(filter.tracks, m_filteredTracks);
        }
    }

    emit filteredTracks();
    emit filteredItems(m_lastFilterIndex);
}

void FilterManager::selectionChanged(LibraryFilter& filter, const Core::TrackPtrList& tracks)
{
    for(const auto& activeFilter : m_filterStore.activeFilters()) {
        if(filter.index < activeFilter.index) {
            m_filterStore.deactivateFilter(activeFilter.type);
        }
    }

    filter.tracks = tracks;

    m_lastFilterIndex = filter.index;

    getFilteredTracks();
}

void FilterManager::searchChanged(const QString& search)
{
    m_searchFilter = search;
    m_filteredTracks.clear();

    emit filterTracks(!m_filteredTracks.empty() ? m_filteredTracks : m_library->allTracks(), m_searchFilter);
}

void FilterManager::tracksFiltered(const Core::TrackPtrList& tracks)
{
    m_filteredTracks = tracks;
    emit filteredTracks();
    emit filteredItems(-1);
}

void FilterManager::tracksChanged()
{
    emit filteredItems(-1);
}
} // namespace Fy::Filters
