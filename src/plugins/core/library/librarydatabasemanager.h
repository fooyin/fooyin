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

#include "app/worker.h"
#include "gui/filter/filterfwd.h"
#include "library/models/trackfwd.h"
#include "library/sorting/sortorder.h"

namespace DB {
class LibraryDatabase;
}

class LibraryDatabaseManager : public Worker
{
    Q_OBJECT

public:
    explicit LibraryDatabaseManager(QObject* parent = nullptr);
    ~LibraryDatabaseManager() override;

    void getAllTracks();
    void getAllItems(Filters::FilterType type, ::Library::SortOrder order);
    void getItemsByFilter(Filters::FilterType type, const ActiveFilters& filters, const QString& search,
                          ::Library::SortOrder order);
    void filterTracks(const TrackPtrList& tracks, const ActiveFilters& filters, const QString& search);

    void stopThread() override;

signals:
    void gotTracks(const TrackList& result);
    void gotItems(Filters::FilterType type, const FilterList& result);
    void tracksFiltered(const TrackPtrList& result);

private:
    DB::LibraryDatabase* m_libraryDatabase;
};
