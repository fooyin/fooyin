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

#include <core/app/worker.h>
#include <core/library/sorting/sortorder.h>
#include <core/models/trackfwd.h>

namespace Core::DB {
class Database;
}

namespace Filters {
class FilterDatabase;
class FilterDatabaseManager : public Core::Worker
{
    Q_OBJECT

public:
    explicit FilterDatabaseManager(Core::DB::Database* database, QObject* parent = nullptr);
    ~FilterDatabaseManager() override;

    void getAllItems(Filters::FilterType type, Core::Library::SortOrder order);
    void getItemsByFilter(Filters::FilterType type, const ActiveFilters& filters, const QString& search,
                          Core::Library::SortOrder order);
    void filterTracks(const Core::TrackPtrList& tracks, const ActiveFilters& filters, const QString& search);

signals:
    void gotItems(Filters::FilterType type, const FilterEntries& result);
    void tracksFiltered(const Core::TrackPtrList& result);

private:
    Core::DB::Database* m_database;
    std::unique_ptr<FilterDatabase> m_filterDatabase;
};
} // namespace Filters
