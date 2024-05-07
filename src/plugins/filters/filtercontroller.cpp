/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "filtermanager.h"
#include "filterwidget.h"
#include "settings/filtersettings.h"

#include <core/library/musiclibrary.h>
#include <core/library/trackfilter.h>
#include <gui/editablelayout.h>
#include <gui/trackselectioncontroller.h>
#include <utils/async.h>
#include <utils/crypto.h>
#include <utils/helpers.h>
#include <utils/settings/settingsmanager.h>

#include <QCoro/QCoroCore>

#include <QMenu>

#include <ranges>

namespace {
Fooyin::TrackList trackIntersection(const Fooyin::TrackList& v1, const Fooyin::TrackList& v2)
{
    Fooyin::TrackList result;
    std::unordered_set<int> ids;

    for(const auto& track : v1) {
        ids.emplace(track.id());
    }

    for(const auto& track : v2) {
        if(ids.contains(track.id())) {
            result.push_back(track);
        }
    }

    return result;
}
} // namespace

namespace Fooyin::Filters {
struct FilterController::Private
{
    FilterController* self;

    MusicLibrary* library;
    TrackSelectionController* trackSelection;
    EditableLayout* editableLayout;
    SettingsManager* settings;

    FilterManager* manager;

    Id defaultId{"Default"};
    FilterGroups groups;
    std::unordered_map<Id, FilterWidget*, Id::IdHash> ungrouped;

    TrackAction doubleClickAction;
    TrackAction middleClickAction;

    explicit Private(FilterController* self_, MusicLibrary* library_, TrackSelectionController* trackSelection_,
                     EditableLayout* editableLayout_, SettingsManager* settings_)
        : self{self_}
        , library{library_}
        , trackSelection{trackSelection_}
        , editableLayout{editableLayout_}
        , settings{settings_}
        , manager{new FilterManager(self, editableLayout, self)}
        , doubleClickAction{static_cast<TrackAction>(settings->value<Settings::Filters::FilterDoubleClick>())}
        , middleClickAction{static_cast<TrackAction>(settings->value<Settings::Filters::FilterMiddleClick>())}
    { }

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
            if(!groupId.isValid()) {
                // Ungrouped
                widget->reset(library->tracks());
                return;
            }
            resetGroup(widget->group());
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

        resetGroup(oldGroup);
        resetGroup(groupId);
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

        return group.filteredTracks.empty() ? library->tracks() : group.filteredTracks;
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
        for(auto& [id, group] : groups) {
            group.filteredTracks.clear();
            for(const auto& filterWidget : group.filters) {
                filterWidget->reset(tracks(id));
            }
        }

        for(auto* filterWidget : ungrouped | std::views::values) {
            filterWidget->reset(library->tracks());
        }
    }

    void resetGroup(const Id& group)
    {
        if(!groups.contains(group)) {
            return;
        }

        auto& filterGroup = groups.at(group);
        filterGroup.filteredTracks.clear();
        for(const auto& filterWidget : filterGroup.filters) {
            filterWidget->reset(tracks(group));
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
            = group.filters | std::views::filter([](FilterWidget* widget) { return widget->isActive(); });

        for(auto& filter : activeFilters) {
            if(group.filteredTracks.empty()) {
                std::ranges::copy(filter->filteredTracks(), std::back_inserter(group.filteredTracks));
            }
            else {
                group.filteredTracks = trackIntersection(filter->filteredTracks(), group.filteredTracks);
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
                filter->clearFilteredTracks();
            }
        }
    }

    void selectionChanged(FilterWidget* filter, const QString& playlistName)
    {
        trackSelection->changeSelectedTracks(filter->widgetContext(), filter->filteredTracks(), playlistName);

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
                std::ranges::copy_if(filterWidget->filteredTracks(), std::back_inserter(cleanedTracks),
                                     [libraryId](const Track& track) { return track.libraryId() != libraryId; });
                filterWidget->setFilteredTracks(cleanedTracks);
            }
        }
    }

    void handleTracksAddedUpdated(const TrackList& tracks, bool updated = false)
    {
        for(const auto& [_, group] : groups) {
            const int count = static_cast<int>(group.filters.size());
            TrackList activeFilterTracks;

            for(const auto& filterWidget : group.filters) {
                if(updated) {
                    QObject::connect(
                        filterWidget, &FilterWidget::finishedUpdating, filterWidget,
                        [this, count, filterWidget]() {
                            const auto groupId = filterWidget->group();
                            if(groups.contains(groupId)) {
                                int& updateCount = groups.at(groupId).updateCount;
                                ++updateCount;

                                if(updateCount == count) {
                                    updateCount = 0;
                                    refreshFilters(groupId);
                                }
                            }
                        },
                        static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::SingleShotConnection));
                }

                if(!filterWidget->searchFilter().isEmpty()) {
                    const TrackList filteredTracks = Filter::filterTracks(tracks, filterWidget->searchFilter());
                    if(updated) {
                        filterWidget->tracksUpdated(filteredTracks);
                    }
                    else {
                        filterWidget->tracksAdded(filteredTracks);
                    }
                }
                else if(activeFilterTracks.empty()) {
                    if(updated) {
                        filterWidget->tracksUpdated(tracks);
                    }
                    else {
                        filterWidget->tracksAdded(tracks);
                    }
                }
                else {
                    const auto filtered = trackIntersection(activeFilterTracks, tracks);
                    if(updated) {
                        filterWidget->tracksUpdated(filtered);
                    }
                    else {
                        filterWidget->tracksAdded(filtered);
                    }
                }

                if(filterWidget->isActive()) {
                    activeFilterTracks = filterWidget->filteredTracks();
                }
            }
        }
    }

    void refreshFilters(const Id& groupId)
    {
        if(!groups.contains(groupId)) {
            return;
        }

        FilterGroup& group = groups.at(groupId);
        const auto count   = static_cast<int>(group.filters.size());

        for(int i{0}; i < count - 1; i += 2) {
            auto* filter = group.filters.at(i);

            if(filter->isActive()) {
                filter->refetchFilteredTracks();
                group.filters.at(i + 1)->softReset(filter->filteredTracks());
            }
        }

        if(count > 1 && count % 2 == 1) {
            auto* filter = group.filters.at(count - 2);
            if(filter->isActive()) {
                filter->refetchFilteredTracks();
                group.filters.at(count - 1)->softReset(filter->filteredTracks());
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

        const bool reset         = !group.filteredTracks.empty() || filter->searchFilter().length() > search.length();
        TrackList tracksToFilter = reset ? library->tracks() : filter->tracks();
        TrackList filteredTracks = co_await Utils::asyncExec(
            [&search, &tracksToFilter]() { return Filter::filterTracks(tracksToFilter, search); });

        filter->reset(filteredTracks);
    }
};

FilterController::FilterController(MusicLibrary* library, TrackSelectionController* trackSelection,
                                   EditableLayout* editableLayout, SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, library, trackSelection, editableLayout, settings)}
{
    QObject::connect(p->library, &MusicLibrary::tracksAdded, this,
                     [this](const TrackList& tracks) { p->handleTracksAddedUpdated(tracks); });
    QObject::connect(p->library, &MusicLibrary::tracksScanned, this,
                     [this](int /*id*/, const TrackList& tracks) { p->handleTracksAddedUpdated(tracks); });
    QObject::connect(p->library, &MusicLibrary::tracksUpdated, this,
                     [this](const TrackList& tracks) { p->handleTracksAddedUpdated(tracks, true); });
    QObject::connect(p->library, &MusicLibrary::tracksDeleted, this, &FilterController::tracksRemoved);
    QObject::connect(p->library, &MusicLibrary::tracksLoaded, this, [this]() { p->resetAll(); });
    QObject::connect(p->library, &MusicLibrary::tracksSorted, this, [this]() { p->resetAll(); });

    settings->subscribe<Settings::Filters::FilterDoubleClick>(
        this, [this](int action) { p->doubleClickAction = static_cast<TrackAction>(action); });
    settings->subscribe<Settings::Filters::FilterMiddleClick>(
        this, [this](int action) { p->middleClickAction = static_cast<TrackAction>(action); });
}

FilterController::~FilterController() = default;

FilterWidget* FilterController::createFilter()
{
    auto* widget = new FilterWidget(p->settings);

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
