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

#include "filtercontroller.h"

#include "filtercolumnregistry.h"
#include "filtermanager.h"
#include "filterwidget.h"

#include <core/library/musiclibrary.h>
#include <core/library/trackfilter.h>
#include <gui/editablelayout.h>
#include <gui/fywidget.h>
#include <gui/trackselectioncontroller.h>
#include <utils/async.h>
#include <utils/crypto.h>

#include <QCoroCore>

#include <QMenu>

#include <set>

using namespace Qt::Literals::StringLiterals;

namespace Fooyin::Filters {
struct FilterController::Private
{
    FilterController* self;

    MusicLibrary* library;
    TrackSelectionController* trackSelection;
    EditableLayout* editableLayout;
    SettingsManager* settings;

    FilterManager* manager;
    FilterColumnRegistry columnRegistry;

    Id defaultId{"Default"};
    FilterGroups groups;
    std::unordered_map<Id, FilterWidget*, Id::IdHash> ungrouped;

    TrackAction doubleClickAction;
    TrackAction middleClickAction;

    explicit Private(FilterController* self, MusicLibrary* library, TrackSelectionController* trackSelection,
                     EditableLayout* editableLayout, SettingsManager* settings)
        : self{self}
        , library{library}
        , trackSelection{trackSelection}
        , editableLayout{editableLayout}
        , settings{settings}
        , manager{new FilterManager(self, editableLayout, self)}
        , columnRegistry{settings}
        , doubleClickAction{static_cast<TrackAction>(settings->value<Settings::Filters::FilterDoubleClick>())}
        , middleClickAction{static_cast<TrackAction>(settings->value<Settings::Filters::FilterMiddleClick>())}
    {
        columnRegistry.loadItems();
    }

    void handleAction(const TrackAction& action, const QString& playlistName) const
    {
        const bool autoSwitch = settings->value<Settings::Filters::FilterAutoSwitch>();
        trackSelection->executeAction(action, autoSwitch ? PlaylistAction::Switch : PlaylistAction::None, playlistName);
    }

    Id findContainingGroup(FilterWidget* widget)
    {
        for(const auto& [id, group] : groups) {
            for(const auto& filterWidget : group.filters) {
                if(filterWidget == widget) {
                    return id;
                }
            }
        }
        return {};
    }

    void handleFilterUpdated(FilterWidget* widget)
    {
        const Id groupId  = widget->group();
        const Id oldGroup = findContainingGroup(widget);

        if(groupId == oldGroup) {
            getFilteredTracks(groupId);
            widget->reset(tracks(groupId));
            return;
        }

        // Remove from old group
        if(!oldGroup.isValid()) {
            ungrouped.erase(widget->id());
        }
        else if(groups.contains(oldGroup)) {
            if(std::erase(groups.at(oldGroup).filters, widget) > 0) {
                if(groups.at(oldGroup).filters.empty()) {
                    groups.erase(oldGroup);
                }
                recalculateIndexesOfGroup(oldGroup);
            }
        }

        // Add to new group
        if(!groupId.isValid()) {
            ungrouped.emplace(widget->id(), widget);
        }
        else {
            FilterGroup& group = groups[groupId];
            const int index    = widget->index();
            if(index < 0 || index > static_cast<int>(group.filters.size())) {
                group.filters.push_back(widget);
            }
            else {
                group.filters.insert(group.filters.begin() + widget->index(), widget);
            }
            group.id = groupId;
            recalculateIndexesOfGroup(groupId);
        }

        getFilteredTracks(groupId);
        widget->reset(tracks(groupId));
    }

    void filterContextMenu(const QPoint& pos) const
    {
        auto* menu = new QMenu();
        menu->setAttribute(Qt::WA_DeleteOnClose);

        trackSelection->addTrackPlaylistContextMenu(menu);
        trackSelection->addTrackContextMenu(menu);

        menu->popup(pos);
    }

    [[nodiscard]] TrackList tracks(const Id& groupId) const
    {
        if(!groups.contains(groupId)) {
            return library->tracks();
        }

        const FilterGroup& group = groups.at(groupId);

        if(!group.filteredTracks.empty() || !group.searchFilter.isEmpty() /* || filterStore.hasActiveFilters()*/) {
            return group.filteredTracks;
        }

        return library->tracks();
    }

    void recalculateIndexesOfGroup(const Id& group)
    {
        if(!groups.contains(group)) {
            return;
        }

        for(auto index{0}; const auto& filter : groups.at(group).filters) {
            filter->setIndex(index++);
        }
    }

    void resetAll()
    {
        for(const auto& group : groups | std::views::keys) {
            getFilteredTracks(group);
        }

        for(const auto& [id, group] : groups) {
            for(const auto& filterWidget : group.filters) {
                filterWidget->reset(tracks(id));
            }
        }

        for(auto* filterWidget : ungrouped | std::views::values) {
            filterWidget->reset(library->tracks());
        }
    }

    void resetFiltersAfterFilter(FilterWidget* filter)
    {
        const Id group = filter->group();

        if(!groups.contains(group)) {
            return;
        }

        const int resetIndex = filter->index() - 1;

        for(const auto& filterWidget : groups.at(group).filters) {
            if(filterWidget->index() > resetIndex) {
                filterWidget->reset(tracks(group));
            }
        }
    }

    void getFilteredTracks(const Id& groupId)
    {
        if(!groups.contains(groupId)) {
            return;
        }

        FilterGroup& group = groups.at(groupId);
        group.filteredTracks.clear();

        auto activeFilters
            = group.filters | std::views::filter([](FilterWidget* widget) { return !widget->tracks().empty(); });

        for(auto& filter : activeFilters) {
            if(group.filteredTracks.empty()) {
                std::ranges::copy(filter->tracks(), std::back_inserter(group.filteredTracks));
            }
            else {
                group.filteredTracks
                    = Utils::intersection<Track, Track::TrackHash>(filter->tracks(), group.filteredTracks);
            }
        }
    }

    void clearActiveFilters(const Id& group, int index)
    {
        if(!groups.contains(group)) {
            return;
        }

        for(const auto& filter : groups.at(group).filters) {
            if(filter->index() > index) {
                filter->clearTracks();
            }
        }
    }

    void selectionChanged(FilterWidget* filter, const QString& playlistName)
    {
        trackSelection->changeSelectedTracks(filter->tracks(), playlistName);

        if(settings->value<Settings::Filters::FilterPlaylistEnabled>()) {
            const QString autoPlaylist = settings->value<Settings::Filters::FilterAutoPlaylist>();
            const bool autoSwitch      = settings->value<Settings::Filters::FilterAutoSwitch>();

            PlaylistAction::ActionOptions options = PlaylistAction::KeepActive;
            if(autoSwitch) {
                options |= PlaylistAction::Switch;
            }
            trackSelection->executeAction(TrackAction::SendNewPlaylist, options, autoPlaylist);
        }

        const Id group       = filter->group();
        const int resetIndex = filter->index();
        clearActiveFilters(group, resetIndex);
        getFilteredTracks(group);

        if(!groups.contains(group)) {
            return;
        }

        for(const auto& filterWidget : groups.at(group).filters) {
            if(filterWidget->index() > resetIndex) {
                filterWidget->reset(tracks(group));
            }
        }
    }

    void removeLibraryTracks(int libraryId)
    {
        for(const auto& [_, group] : groups) {
            for(const auto& filterWidget : group.filters) {
                TrackList cleanedTracks;
                std::ranges::copy_if(filterWidget->tracks(), std::back_inserter(cleanedTracks),
                                     [libraryId](const Track& track) { return track.libraryId() != libraryId; });
                filterWidget->setTracks(cleanedTracks);
            }
        }
    }

    void handleTracksAdded(const TrackList& tracks)
    {
        bool firstActive{false};
        for(const auto& [_, group] : groups) {
            for(const auto& filterWidget : group.filters) {
                if(firstActive) {
                    break;
                }

                if(!filterWidget->tracks().empty()) {
                    firstActive = true;
                }

                filterWidget->tracksAdded(tracks);
            }
        }
    }

    QCoro::Task<void> searchChanged(FilterWidget* filter, QString search)
    {
        const Id groupId = filter->group();

        if(!groups.contains(groupId)) {
            co_return;
        }

        FilterGroup& group = groups.at(groupId);

        const bool reset   = group.searchFilter.length() > search.length();
        group.searchFilter = search;

        TrackList tracksToFilter = !reset && !filter->tracks().empty() ? filter->tracks() : library->tracks();

        group.filteredTracks = co_await Utils::asyncExec(
            [&search, &tracksToFilter]() { return Filter::filterTracks(tracksToFilter, search); });

        filter->reset(tracks(filter->group()));
    }
};

FilterController::FilterController(MusicLibrary* library, TrackSelectionController* trackSelection,
                                   EditableLayout* editableLayout, SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, library, trackSelection, editableLayout, settings)}
{
    QObject::connect(p->library, &MusicLibrary::tracksAdded, this,
                     [this](const TrackList& tracks) { p->handleTracksAdded(tracks); });
    QObject::connect(p->library, &MusicLibrary::tracksScanned, this,
                     [this](const TrackList& tracks) { p->handleTracksAdded(tracks); });
    QObject::connect(p->library, &MusicLibrary::tracksUpdated, this, &FilterController::tracksUpdated);
    QObject::connect(p->library, &MusicLibrary::tracksDeleted, this, &FilterController::tracksRemoved);

    QObject::connect(p->library, &MusicLibrary::tracksLoaded, this, [this]() { p->resetAll(); });
    QObject::connect(p->library, &MusicLibrary::tracksSorted, this, [this]() { p->resetAll(); });
    QObject::connect(p->library, &MusicLibrary::libraryChanged, this, [this]() { p->resetAll(); });
    QObject::connect(p->library, &MusicLibrary::libraryRemoved, this, [this](int libraryId) {
        p->removeLibraryTracks(libraryId);
        p->resetAll();
    });

    settings->subscribe<Settings::Filters::FilterDoubleClick>(
        this, [this](int action) { p->doubleClickAction = static_cast<TrackAction>(action); });
    settings->subscribe<Settings::Filters::FilterMiddleClick>(
        this, [this](int action) { p->middleClickAction = static_cast<TrackAction>(action); });
}

FilterController::~FilterController() = default;

void FilterController::shutdown()
{
    p->columnRegistry.saveItems();
}

FilterWidget* FilterController::createFilter()
{
    auto* widget = new FilterWidget(&p->columnRegistry, p->settings);

    auto& group = p->groups[p->defaultId];
    group.id    = p->defaultId;
    widget->setGroup(p->defaultId);
    widget->setIndex(static_cast<int>(group.filters.size()));
    group.filters.push_back(widget);

    QObject::connect(widget, &FilterWidget::doubleClicked, this,
                     [this](const QString& playlistName) { p->handleAction(p->doubleClickAction, playlistName); });
    QObject::connect(widget, &FilterWidget::middleClicked, this,
                     [this](const QString& playlistName) { p->handleAction(p->middleClickAction, playlistName); });
    QObject::connect(widget, &FilterWidget::requestSearch, this,
                     [this, widget](const QString& search) { p->searchChanged(widget, search); });
    QObject::connect(widget, &FilterWidget::requestContextMenu, this,
                     [this](const QPoint& pos) { p->filterContextMenu(pos); });
    QObject::connect(widget, &FilterWidget::selectionChanged, this,
                     [this, widget](const QString& playlistName) { p->selectionChanged(widget, playlistName); });
    QObject::connect(widget, &FilterWidget::filterUpdated, this, [this, widget]() { p->handleFilterUpdated(widget); });
    QObject::connect(widget, &FilterWidget::filterDeleted, this, [this, widget]() { removeFilter(widget); });
    QObject::connect(widget, &FilterWidget::requestEditConnections, this,
                     [this]() { p->manager->setupWidgetConnections(); });
    QObject::connect(this, &FilterController::tracksUpdated, widget, &FilterWidget::tracksUpdated);
    QObject::connect(this, &FilterController::tracksRemoved, widget, &FilterWidget::tracksRemoved);

    widget->reset(p->tracks(p->defaultId));

    return widget;
}

FilterColumnRegistry* FilterController::columnRegistry() const
{
    return &p->columnRegistry;
}

bool FilterController::haveUngroupedFilters() const
{
    return !p->ungrouped.empty();
}

bool FilterController::filterIsUngrouped(const Id& id) const
{
    return p->ungrouped.contains(id);
}

FilterGroups FilterController::filterGroups() const
{
    return p->groups;
}

std::optional<FilterGroup> FilterController::groupById(const Id& id) const
{
    if(!p->groups.contains(id)) {
        return {};
    }
    return p->groups.at(id);
}

UngroupedFilters FilterController::ungroupedFilters() const
{
    return p->ungrouped;
}

void FilterController::addFilterToGroup(FilterWidget* widget, const Id& groupId)
{
    if(!groupId.isValid()) {
        widget->setGroup("");
        widget->setIndex(-1);
        p->ungrouped.emplace(widget->id(), widget);
    }
    else {
        auto& group = p->groups[groupId];
        group.id    = groupId;
        widget->setGroup(groupId);
        widget->setIndex(static_cast<int>(group.filters.size()));
        group.filters.push_back(widget);
    }
}

bool FilterController::removeFilter(FilterWidget* widget)
{
    const Id groupId = widget->group();

    if(!groupId.isValid() && p->ungrouped.contains(widget->id())) {
        p->ungrouped.erase(widget->id());
        return true;
    }

    if(!p->groups.contains(groupId)) {
        return false;
    }

    auto& group = p->groups.at(groupId);

    if(std::erase(group.filters, widget) > 0) {
        p->recalculateIndexesOfGroup(groupId);

        if(group.filters.empty()) {
            p->groups.erase(groupId);
        }

        return true;
    }

    return false;
}
} // namespace Fooyin::Filters

#include "moc_filtercontroller.cpp"
