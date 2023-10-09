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

LibraryFilter FilterStore::filterByIndex(int index) const
{
    const auto currentFilter = std::ranges::find_if(std::as_const(m_filters), [index](const LibraryFilter& filter) {
        return filter.index == index;
    });
    if(currentFilter != m_filters.cend()) {
        return *currentFilter;
    }
    return {};
}

LibraryFilter FilterStore::addFilter(const FilterField& field)
{
    LibraryFilter& filter = m_filters.emplace_back();
    filter.field          = field;
    filter.index          = static_cast<int>(m_filters.size() - 1);
    return filter;
}

void FilterStore::updateFilter(const LibraryFilter& filter)
{
    auto currentFilter = std::ranges::find_if(m_filters, [filter](LibraryFilter& libFilter) {
        return libFilter.index == filter.index;
    });
    if(currentFilter != m_filters.end()) {
        *currentFilter = filter;
    }
}

void FilterStore::removeFilter(int index)
{
    m_filters.erase(std::remove_if(m_filters.begin(), m_filters.end(),
                                   [index](const LibraryFilter& filter) {
                                       return filter.index == index;
                                   }),
                    m_filters.end());
}

[[nodiscard]] bool FilterStore::hasActiveFilters() const
{
    return std::ranges::any_of(std::as_const(m_filters), [](const LibraryFilter& filter) {
        return !filter.tracks.empty();
    });
};

FilterList FilterStore::activeFilters() const
{
    return Utils::filter(m_filters, [](const LibraryFilter& filter) {
        return !filter.tracks.empty();
    });
}

void FilterStore::clearActiveFilters(int index)
{
    for(LibraryFilter& filter : m_filters) {
        if(filter.index > index) {
            filter.tracks.clear();
        }
    }
}
} // namespace Fy::Filters
