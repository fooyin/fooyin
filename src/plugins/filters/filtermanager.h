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

#include "core/typedefs.h"
#include "filterfwd.h"

#include <core/library/models/trackfwd.h>
#include <core/library/musiclibraryinteractor.h>
#include <core/library/sorting/sortorder.h>

namespace Library {
class MusicLibrary;
}; // namespace Library

class FilterDatabaseManager;
class ThreadManager;
class FilterLibraryController;

namespace Filters {
class FilterManager : public MusicLibraryInteractor
{
    Q_OBJECT

public:
    explicit FilterManager(QObject* parent = nullptr);
    ~FilterManager() override;

    TrackPtrList tracks() override;
    bool hasTracks() override;

    LibraryFilters filters();
    bool hasFilter(Filters::FilterType type) const;
    LibraryFilter findFilter(Filters::FilterType type);
    int registerFilter(Filters::FilterType type);
    void unregisterFilter(Filters::FilterType type);
    void changeFilter(int index);
    void resetFilter(Filters::FilterType type);
    Library::SortOrder filterOrder(Filters::FilterType type);
    void changeFilterOrder(Filters::FilterType type, Library::SortOrder order);

    void items(Filters::FilterType type);
    void getAllItems(Filters::FilterType type, Library::SortOrder order);
    void getItemsByFilter(Filters::FilterType type, Library::SortOrder order);

    void getFilteredTracks();
    bool tracksHaveFiltered();

    void changeSelection(const IdSet& indexes, Filters::FilterType type, int index);
    void selectionChanged(const IdSet& indexes, Filters::FilterType type, int index);
    void changeSearch(const QString& search);
    void searchChanged(const QString& search);

signals:
    void loadAllItems(Filters::FilterType type, Library::SortOrder order);
    void loadItemsByFilter(Filters::FilterType type, ActiveFilters filters, QString search, Library::SortOrder order);

    void itemsLoaded(Filters::FilterType type, FilterEntries result);

    void filteredTracks();
    void loadFilteredTracks(TrackPtrList tracks, ActiveFilters filters, QString search);
    void filteredItems(int index = -1);
    void orderedFilter(Filters::FilterType type);
    void filterReset(Filters::FilterType type, const IdSet& selection);

protected:
    void itemsHaveLoaded(Filters::FilterType type, FilterEntries result);
    void filteredTracksLoaded(TrackPtrList tracks);

private:
    struct Private;
    std::unique_ptr<FilterManager::Private> p;
};
}; // namespace Filters
