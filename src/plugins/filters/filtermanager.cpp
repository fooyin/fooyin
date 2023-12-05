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

#include "filtercolumnregistry.h"
#include "filterstore.h"
#include "filterwidget.h"

#include <core/library/musiclibrary.h>
#include <core/library/tracksort.h>
#include <gui/trackselectioncontroller.h>
#include <utils/async.h>
#include <utils/widgets/autoheaderview.h>

#include <QActionGroup>
#include <QMenu>

#include <QCoroCore>

#include <set>

namespace {
bool containsSearch(const QString& text, const QString& search)
{
    return text.contains(search, Qt::CaseInsensitive);
}

// TODO: Support user-defined tags
bool matchSearch(const Fooyin::Track& track, const QString& search)
{
    if(search.isEmpty()) {
        return true;
    }

    return containsSearch(track.artist(), search) || containsSearch(track.title(), search)
        || containsSearch(track.album(), search) || containsSearch(track.albumArtist(), search);
}

Fooyin::TrackList filterTracks(const Fooyin::TrackList& tracks, const QString& search)
{
    return Fooyin::Utils::filter(tracks, [search](const Fooyin::Track& track) { return matchSearch(track, search); });
}
} // namespace

namespace Fooyin::Filters {
struct FilterManager::Private
{
    FilterManager* self;

    MusicLibrary* library;
    TrackSelectionController* trackSelection;
    SettingsManager* settings;

    std::map<int, FilterWidget*> filterWidgets;

    TrackAction doubleClickAction;
    TrackAction middleClickAction;

    FilterColumnRegistry columnRegistry;
    TrackList filteredTracks;
    FilterStore filterStore;
    QString searchFilter;

    Private(FilterManager* self, MusicLibrary* library, TrackSelectionController* trackSelection,
            SettingsManager* settings)
        : self{self}
        , library{library}
        , trackSelection{trackSelection}
        , settings{settings}
        , doubleClickAction{static_cast<TrackAction>(settings->value<Settings::Filters::FilterDoubleClick>())}
        , middleClickAction{static_cast<TrackAction>(settings->value<Settings::Filters::FilterMiddleClick>())}
        , columnRegistry{settings}
    {
        columnRegistry.loadItems();
    }

    void handleAction(const TrackAction& action, const QString& playlistName) const
    {
        const bool autoSwitch = settings->value<Settings::Filters::FilterAutoSwitch>();
        trackSelection->executeAction(action, autoSwitch ? PlaylistAction::Switch : PlaylistAction::None, playlistName);
    }

    void resetFiltersAfterIndex(int resetIndex)
    {
        for(const auto& [index, filterWidget] : filterWidgets) {
            if(index > resetIndex) {
                filterWidget->reset(tracks());
            }
        }
    }

    void deleteFilter(const LibraryFilter& filter)
    {
        filterWidgets.erase(filter.index);
        filterStore.removeFilter(filter.index);
        getFilteredTracks();
        resetFiltersAfterIndex(filter.index - 1);
    }

    void columnChanged(const Filters::FilterColumn& column)
    {
        const FilterList filters = filterStore.filters();
        std::set<int> filtersToReset;
        int resetIndex{-1};

        for(const LibraryFilter& filter : filters) {
            if(filter.hasColumn(column.id)) {
                LibraryFilter updatedFilter{filter};
                std::ranges::replace_if(
                    updatedFilter.columns,
                    [&column](const FilterColumn& filterCol) { return filterCol.id == column.id; }, column);
                filterStore.updateFilter(updatedFilter);

                filtersToReset.emplace(filter.index);
                resetIndex = resetIndex >= 0 ? std::min(resetIndex, filter.index) : filter.index;
            }
        }

        for(const auto& [index, filterWidget] : filterWidgets) {
            if(filtersToReset.contains(index)) {
                const LibraryFilter filter = filterStore.filterByIndex(index);
                filterWidget->changeFilter(filter);
            }
            if(index >= resetIndex) {
                filterWidget->reset(tracks());
            }
        }
    }

    QCoro::Task<void> selectionChanged(LibraryFilter filter, QString playlistName)
    {
        LibraryFilter updatedFilter{filter};

        TrackList sortedTracks
            = co_await Utils::asyncExec([&updatedFilter]() { return Sorting::sortTracks(updatedFilter.tracks); });

        trackSelection->changeSelectedTracks(sortedTracks, playlistName);
        updatedFilter.tracks = sortedTracks;

        if(settings->value<Settings::Filters::FilterPlaylistEnabled>()) {
            const QString autoPlaylist = settings->value<Settings::Filters::FilterAutoPlaylist>();
            const bool autoSwitch      = settings->value<Settings::Filters::FilterAutoSwitch>();

            PlaylistAction::ActionOptions options = PlaylistAction::KeepActive;
            if(autoSwitch) {
                options |= PlaylistAction::Switch;
            }
            trackSelection->executeAction(TrackAction::SendNewPlaylist, options, autoPlaylist);
        }

        filterStore.updateFilter(updatedFilter);

        const int resetIndex = filter.index;
        filterStore.clearActiveFilters(resetIndex);
        getFilteredTracks();

        for(const auto& [index, filterWidget] : filterWidgets) {
            if(index > resetIndex) {
                filterWidget->reset(tracks());
            }
        }
    }

    void updateFilter(const LibraryFilter& filter)
    {
        filterStore.updateFilter(filter);

        const int resetIndex = filter.index - 1;

        filterStore.clearActiveFilters(resetIndex);
        getFilteredTracks();

        for(const auto& [index, filterWidget] : filterWidgets) {
            if(index == filter.index) {
                filterWidget->changeFilter(filter);
            }
            if(index > resetIndex) {
                filterWidget->reset(tracks());
            }
        }
    }

    void enableMultiColumns(const LibraryFilter& filter, bool enabled)
    {
        LibraryFilter updatedFilter{filter};
        updatedFilter.multipleColumns = enabled;

        for(const auto& [index, filterWidget] : filterWidgets) {
            if(index == updatedFilter.index) {
                filterWidget->changeFilter(updatedFilter);
            }
        }
    }

    void addFilterColumn(const LibraryFilter& filter, int column)
    {
        const FilterColumn filterColumn = columnRegistry.itemById(column);

        LibraryFilter updatedFilter{filter};
        updatedFilter.columns.push_back(filterColumn);

        updateFilter(updatedFilter);
    }

    void changeFilterColumn(const LibraryFilter& filter, int column)
    {
        const FilterColumn filterColumn = columnRegistry.itemById(column);

        LibraryFilter updatedFilter{filter};
        updatedFilter.columns = {filterColumn};

        updateFilter(updatedFilter);
    }

    void removeFilterColumn(const LibraryFilter& filter, int column)
    {
        LibraryFilter updatedFilter{filter};
        std::erase_if(updatedFilter.columns,
                      [column](const FilterColumn& filterCol) { return filterCol.id == column; });

        updateFilter(updatedFilter);
    }

    void changeFilterColumns(const LibraryFilter& filter, const ColumnIds& columns)
    {
        FilterColumnList filterColumns;
        std::ranges::transform(columns, std::back_inserter(filterColumns),
                               [this](int column) { return columnRegistry.itemById(column); });

        LibraryFilter updatedFilter{filter};
        updatedFilter.columns         = filterColumns;
        updatedFilter.multipleColumns = filterColumns.size() > 1;

        updateFilter(updatedFilter);
    }

    void filterHeaderMenu(const LibraryFilter& filter, AutoHeaderView* header, const QPoint& pos)
    {
        auto* menu = new QMenu();
        menu->setAttribute(Qt::WA_DeleteOnClose);

        auto* filterList = new QActionGroup{menu};
        filterList->setExclusionPolicy(QActionGroup::ExclusionPolicy::None);

        for(const auto& [filterIndex, column] : columnRegistry.items()) {
            auto* columnAction = new QAction(column.name, menu);
            columnAction->setData(column.id);
            columnAction->setCheckable(true);
            columnAction->setChecked(filter.hasColumn(column.id));
            columnAction->setEnabled(!filter.hasColumn(column.id) || filter.columns.size() > 1);
            menu->addAction(columnAction);
            filterList->addAction(columnAction);
        }

        menu->setDefaultAction(filterList->checkedAction());
        QObject::connect(filterList, &QActionGroup::triggered, self, [this, &filter](QAction* action) {
            if(action->isChecked()) {
                if(filter.multipleColumns) {
                    addFilterColumn(filter, action->data().toInt());
                }
                else {
                    changeFilterColumn(filter, action->data().toInt());
                }
            }
            else {
                removeFilterColumn(filter, action->data().toInt());
            }
        });

        menu->addSeparator();
        auto* multiColAction = new QAction(tr("Multiple Columns"), menu);
        multiColAction->setCheckable(true);
        multiColAction->setChecked(filter.multipleColumns);
        multiColAction->setEnabled(!(filter.columns.size() > 1));
        QObject::connect(multiColAction, &QAction::triggered, self,
                         [this, &filter](bool checked) { enableMultiColumns(filter, checked); });
        menu->addAction(multiColAction);

        menu->addSeparator();
        header->addHeaderContextMenu(menu, pos);

        menu->popup(pos);
    }

    void filterContextMenu(const LibraryFilter& /*filter*/, const QPoint& pos) const
    {
        auto* menu = new QMenu();
        menu->setAttribute(Qt::WA_DeleteOnClose);

        trackSelection->addTrackPlaylistContextMenu(menu);
        trackSelection->addTrackContextMenu(menu);

        menu->popup(pos);
    }

    [[nodiscard]] bool hasTracks() const
    {
        return !filteredTracks.empty() || !searchFilter.isEmpty() || filterStore.hasActiveFilters();
    }

    [[nodiscard]] TrackList tracks() const
    {
        return hasTracks() ? filteredTracks : library->tracks();
    }

    void getFilteredTracks()
    {
        filteredTracks.clear();

        for(auto& filter : filterStore.activeFilters()) {
            if(filteredTracks.empty()) {
                std::ranges::copy(filter.tracks, std::back_inserter(filteredTracks));
            }
            else {
                filteredTracks = Utils::intersection<Track, Track::TrackHash>(filter.tracks, filteredTracks);
            }
        }
    }

    void removeLibraryTracks(int libraryId)
    {
        std::vector<LibraryFilter> filtersToUpdate;

        for(const auto& filter : filterStore.activeFilters()) {
            LibraryFilter updatedFilter{filter};
            TrackList cleanedTracks;
            std::ranges::copy_if(std::as_const(filter.tracks), std::back_inserter(cleanedTracks),
                                 [libraryId](const Track& track) { return track.libraryId() != libraryId; });
            updatedFilter.tracks = cleanedTracks;
            filtersToUpdate.push_back(updatedFilter);
        }

        std::ranges::for_each(filtersToUpdate, [this](const auto& filter) { filterStore.updateFilter(filter); });
    }
};

FilterManager::FilterManager(MusicLibrary* library, TrackSelectionController* trackSelection, SettingsManager* settings,
                             QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, library, trackSelection, settings)}
{
    QObject::connect(p->library, &MusicLibrary::tracksAdded, this, &FilterManager::tracksAdded);
    QObject::connect(p->library, &MusicLibrary::tracksScanned, this, &FilterManager::tracksAdded);
    QObject::connect(p->library, &MusicLibrary::tracksUpdated, this, &FilterManager::tracksUpdated);
    QObject::connect(p->library, &MusicLibrary::tracksDeleted, this, &FilterManager::tracksRemoved);

    const auto tracksChanged = [this]() {
        p->getFilteredTracks();
        p->resetFiltersAfterIndex(-1);
    };

    QObject::connect(p->library, &MusicLibrary::tracksLoaded, this, tracksChanged);
    QObject::connect(p->library, &MusicLibrary::tracksSorted, this, tracksChanged);
    QObject::connect(p->library, &MusicLibrary::libraryChanged, this, tracksChanged);
    QObject::connect(p->library, &MusicLibrary::libraryRemoved, this, [tracksChanged, this](int libraryId) {
        p->removeLibraryTracks(libraryId);
        tracksChanged();
    });

    QObject::connect(&p->columnRegistry, &FilterColumnRegistry::columnChanged, this,
                     [this](const Filters::FilterColumn& column) { p->columnChanged(column); });

    settings->subscribe<Settings::Filters::FilterDoubleClick>(
        this, [this](int action) { p->doubleClickAction = static_cast<TrackAction>(action); });
    settings->subscribe<Settings::Filters::FilterMiddleClick>(
        this, [this](int action) { p->middleClickAction = static_cast<TrackAction>(action); });
}

FilterManager::~FilterManager() = default;

FilterWidget* FilterManager::createFilter()
{
    auto* filter = new FilterWidget(p->settings);

    const LibraryFilter libFilter = p->filterStore.addFilter({p->columnRegistry.itemByName(QStringLiteral(""))});

    filter->changeFilter(libFilter);

    p->filterWidgets.emplace(libFilter.index, filter);

    QObject::connect(filter, &FilterWidget::doubleClicked, this,
                     [this](const QString& playlistName) { p->handleAction(p->doubleClickAction, playlistName); });
    QObject::connect(filter, &FilterWidget::middleClicked, this,
                     [this](const QString& playlistName) { p->handleAction(p->middleClickAction, playlistName); });
    QObject::connect(
        filter, &FilterWidget::requestColumnsChange, this,
        [this](const LibraryFilter& filter, const ColumnIds& columns) { p->changeFilterColumns(filter, columns); });
    QObject::connect(filter, &FilterWidget::requestHeaderMenu, this,
                     [this](const LibraryFilter& filter, AutoHeaderView* header, const QPoint& pos) {
                         p->filterHeaderMenu(filter, header, pos);
                     });
    QObject::connect(filter, &FilterWidget::requestContextMenu, this,
                     [this](const LibraryFilter& filter, const QPoint& pos) { p->filterContextMenu(filter, pos); });
    QObject::connect(filter, &FilterWidget::selectionChanged, this,
                     [this](const LibraryFilter& filter, const QString& playlistName) {
                         p->selectionChanged(filter, playlistName);
                     });
    QObject::connect(filter, &FilterWidget::filterDeleted, this,
                     [this](const LibraryFilter& filter) { p->deleteFilter(filter); });

    QObject::connect(this, &FilterManager::tracksAdded, filter, &FilterWidget::tracksAdded);
    QObject::connect(this, &FilterManager::tracksUpdated, filter, &FilterWidget::tracksUpdated);
    QObject::connect(this, &FilterManager::tracksRemoved, filter, &FilterWidget::tracksRemoved);

    filter->reset(p->tracks());

    return filter;
}

void FilterManager::shutdown()
{
    p->columnRegistry.saveItems();
};

FilterColumnRegistry* FilterManager::columnRegistry() const
{
    return &p->columnRegistry;
}

QCoro::Task<void> FilterManager::searchChanged(QString search)
{
    const bool reset = p->searchFilter.length() > search.length();
    p->searchFilter  = search;

    TrackList tracksToFilter{!reset && !p->filteredTracks.empty() ? p->filteredTracks : p->library->tracks()};

    p->filteredTracks
        = co_await Utils::asyncExec([&search, &tracksToFilter]() { return filterTracks(tracksToFilter, search); });

    p->resetFiltersAfterIndex(-1);
}
} // namespace Fooyin::Filters

#include "moc_filtermanager.cpp"
