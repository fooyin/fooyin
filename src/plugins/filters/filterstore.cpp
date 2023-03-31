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

#include "filterstore.h"

namespace Fy::Filters {

FilterList FilterStore::filters() const
{
    return m_filters;
}

LibraryFilter& FilterStore::addFilter(FilterType type)
{
    LibraryFilter& filter = m_filters.emplace_back();
    filter.type           = type;
    filter.index          = static_cast<int>(m_filters.size() - 1);
    return filter;
}

void FilterStore::removeFilter(FilterType type)
{
    m_filters.erase(std::remove_if(m_filters.begin(), m_filters.end(),
                                   [type](const LibraryFilter& filter) {
                                       return filter.type == type;
                                   }),
                    m_filters.end());
}

LibraryFilter* FilterStore::find(FilterType type)
{
    for(auto& filter : m_filters) {
        if(filter.type == type) {
            return &filter;
        }
    }
    return nullptr;
}

bool FilterStore::hasFilter(FilterType type) const
{
    return std::find_if(m_filters.cbegin(), m_filters.cend(),
                        [type](const LibraryFilter& filter) {
                            return (filter.type == type);
                        })
        != m_filters.cend();
}

bool FilterStore::filterIsActive(FilterType type) const
{
    return std::any_of(m_filters.cbegin(), m_filters.cend(), [type](const LibraryFilter& filter) {
        return (filter.type == type);
    });
}

[[nodiscard]] bool FilterStore::hasActiveFilters() const
{
    return std::any_of(m_filters.cbegin(), m_filters.cend(), [](const LibraryFilter& filter) {
        return !filter.tracks.empty();
    });
};

FilterList FilterStore::activeFilters() const
{
    return Utils::filter(m_filters, [](const LibraryFilter& filter) {
        return !filter.tracks.empty();
    });
}

void FilterStore::deactivateFilter(FilterType type)
{
    if(auto* filter = find(type)) {
        filter->tracks.clear();
    }
}

} // namespace Fy::Filters
