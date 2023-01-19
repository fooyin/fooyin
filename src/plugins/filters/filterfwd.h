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

#ifndef FILTER_FWD_H
#define FILTER_FWD_H

#include <core/library/sorting/sortorder.h>
#include <core/typedefs.h>

namespace Filters {
Q_NAMESPACE
enum class FilterType
{
    Genre = 0,
    Year,
    AlbumArtist,
    Artist,
    Album,
};
Q_ENUM_NS(FilterType)
} // namespace Filters

struct LibraryFilter
{
    int index;
    Filters::FilterType type;
    Core::Library::SortOrder sortOrder;
};
using LibraryFilters = std::vector<LibraryFilter>;

struct FilterEntry
{
    int id;
    QString name;
};

using FilterEntries = std::vector<FilterEntry>;

using ActiveFilters = std::unordered_map<Filters::FilterType, Core::IdSet>;

#endif // FILTER_FWD_H
