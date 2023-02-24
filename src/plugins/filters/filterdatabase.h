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

#pragma once

#include "filterfwd.h"

#include <core/database/module.h>
#include <core/library/sorting/sortorder.h>
#include <core/models/trackfwd.h>

namespace Filters {
class FilterDatabase : public Core::DB::Module
{
public:
    explicit FilterDatabase(const QString& connectionName);

    bool getAllItems(Filters::FilterType type, Core::Library::SortOrder order, FilterEntries& result) const;

    bool getItemsByFilter(Filters::FilterType type, const ActiveFilters& filters, const QString& search,
                          Core::Library::SortOrder order, FilterEntries& result) const;

    [[nodiscard]] static QString fetchQueryItems(Filters::FilterType type, const QString& where, const QString& join,
                                                 Core::Library::SortOrder order);

    static bool dbFetchItems(Core::DB::Query& q, FilterEntries& result);

protected:
    [[nodiscard]] const Module* module() const;
};
} // namespace Filters
