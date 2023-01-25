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

#include "core/typedefs.h"
#include "filterfwd.h"

#include <core/library/musiclibraryinteractor.h>
#include <core/library/sorting/sortorder.h>
#include <core/models/trackfwd.h>

namespace Core {
class ThreadManager;

namespace DB {
class Database;
}

namespace Library {
class MusicLibrary;
} // namespace Library
} // namespace Core

namespace Filters {
class FilterDatabaseManager;
class FilterLibraryController;

class FilterManager : public Core::Library::MusicLibraryInteractor
{
    Q_OBJECT

public:
    explicit FilterManager(Core::ThreadManager* threadManager, Core::DB::Database* database,
                           Core::Library::MusicLibrary* library, QObject* parent = nullptr);
    ~FilterManager() override;

    [[nodiscard]] Core::TrackPtrList tracks() const override;
    [[nodiscard]] bool hasTracks() const override;

    [[nodiscard]] LibraryFilters filters() const;
    [[nodiscard]] bool hasFilter(FilterType type) const;
    [[nodiscard]] bool filterIsActive(FilterType type) const;
    [[nodiscard]] LibraryFilter findFilter(FilterType type) const;
    int registerFilter(FilterType type);
    void unregisterFilter(FilterType type);
    void changeFilter(FilterType oldType, FilterType type);
    void resetFilter(FilterType type);
    [[nodiscard]] Core::Library::SortOrder filterOrder(FilterType type) const;
    void changeFilterOrder(FilterType type, Core::Library::SortOrder order);

    void items(FilterType type);
    void getAllItems(FilterType type, Core::Library::SortOrder order);
    void getItemsByFilter(FilterType type, Core::Library::SortOrder order);

    void getFilteredTracks();
    bool tracksHaveFiltered();

    void changeSelection(const Core::IdSet& indexes, FilterType type, int index);
    void selectionChanged(const Core::IdSet& indexes, FilterType type, int index);
    void changeSearch(const QString& search);
    void searchChanged(const QString& search);

signals:
    void loadAllItems(Filters::FilterType type, Core::Library::SortOrder order);
    void loadItemsByFilter(Filters ::FilterType type, ActiveFilters filters, QString search,
                           Core::Library::SortOrder order);
    void itemsLoaded(Filters::FilterType type, FilterEntries result);
    void filteredItems(int index = -1);

    void loadFilteredTracks(Core::TrackPtrList tracks, ActiveFilters filters, QString search);
    void filteredTracks();

    void orderedFilter(Filters ::FilterType type);
    void filterReset(Filters ::FilterType type, const Core::IdSet& selection);

protected:
    void itemsHaveLoaded(FilterType type, FilterEntries result);
    void filteredTracksLoaded(Core::TrackPtrList tracks);

private:
    struct Private;
    std::unique_ptr<FilterManager::Private> p;
};
} // namespace Filters
