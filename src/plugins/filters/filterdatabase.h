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

#pragma once

#include "filterfwd.h"

#include <core/database/module.h>
#include <core/library/models/trackfwd.h>
#include <core/library/sorting/sortorder.h>

namespace DB {
class FilterDatabase : public DB::Module
{
public:
    FilterDatabase(const QString& connectionName);
    ~FilterDatabase() override;

    bool getAllItems(Filters::FilterType type, ::Library::SortOrder order, FilterList& result) const;

    bool getItemsByFilter(Filters::FilterType type, const ActiveFilters& filters, const QString& search,
                          ::Library::SortOrder order, FilterList& result) const;

    [[nodiscard]] static QString fetchQueryItems(Filters::FilterType type, const QString& where, const QString& join,
                                                 ::Library::SortOrder order);

    static bool dbFetchItems(Query& q, FilterList& result);

protected:
    [[nodiscard]] const Module* module() const;
};
} // namespace DB
