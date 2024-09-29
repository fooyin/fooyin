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

#include "filtercolumnregistry.h"
#include "filtermanager.h"
#include "filterwidget.h"
#include "settings/filtersettings.h"

#include <core/coresettings.h>
#include <core/library/musiclibrary.h>
#include <core/library/tracksort.h>
#include <core/plugins/coreplugincontext.h>
#include <core/scripting/scriptparser.h>
#include <gui/coverprovider.h>
#include <gui/editablelayout.h>
#include <gui/trackselectioncontroller.h>
#include <utils/async.h>
#include <utils/crypto.h>
#include <utils/helpers.h>
#include <utils/settings/settingsmanager.h>

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
class FilterControllerPrivate
{
public:
    explicit FilterControllerPrivate(FilterController* self, const CorePluginContext& core,
                                     TrackSelectionController* trackSelection, EditableLayout* editableLayout,
                                     SettingsManager* settings);

    void handleAction(const TrackAction& action) const;
    Id findContainingGroup(FilterWidget* widget);
    void handleFilterUpdated(FilterWidget* widget);
    void filterContextMenu(const QPoint& pos) const;

    [[nodiscard]] TrackList tracks(const Id& groupId) const;

    void recalculateIndexesOfGroup(const Id& group);

    void resetAll();
    void resetGroup(const Id& group);
    void resetFiltersAfterFilter(FilterWidget* filter);

    void getFilteredTracks(const Id& groupId);
    void clearActiveFilters(const Id& group, int index);

    void updateFilterPlaylistActions(FilterWidget* filterWidget) const;
    void updateAllPlaylistActions();

    void selectionChanged(FilterWidget* filter);

    void removeLibraryTracks(int libraryId);
    void handleTracksAddedUpdated(const TrackList& tracks, bool updated = false);
    void refreshFilters(const Id& groupId);
    void searchChanged(FilterWidget* filter, const QString& search);

    FilterController* m_self;

    MusicLibrary* m_library;
    LibraryManager* m_libraryManager;
    TrackSelectionController* m_trackSelection;
    EditableLayout* m_editableLayout;
    CoverProvider m_coverProvider;
    SettingsManager* m_settings;

    FilterManager* m_manager;
    FilterColumnRegistry* m_columnRegistry;
    TrackSorter m_sorter;

    Id m_defaultId{"Default"};
    FilterGroups m_groups;
    std::unordered_map<Id, FilterWidget*, Id::IdHash> m_ungrouped;

    TrackAction m_doubleClickAction;
    TrackAction m_middleClickAction;
};

FilterControllerPrivate::FilterControllerPrivate(FilterController* self, const CorePluginContext& core,
                                                 TrackSelectionController* trackSelection,
                                                 EditableLayout* editableLayout, SettingsManager* settings)
    : m_self{self}
    , m_library{core.library}
    , m_libraryManager{core.libraryManager}
    , m_trackSelection{trackSelection}
    , m_editableLayout{editableLayout}
    , m_coverProvider{core.audioLoader, settings}
    , m_settings{settings}
    , m_manager{new FilterManager(m_self, m_editableLayout, m_self)}
    , m_columnRegistry{new FilterColumnRegistry(settings, m_self)}
    , m_sorter{core.libraryManager}
    , m_doubleClickAction{static_cast<TrackAction>(m_settings->value<Settings::Filters::FilterDoubleClick>())}
    , m_middleClickAction{static_cast<TrackAction>(m_settings->value<Settings::Filters::FilterMiddleClick>())}
{
    m_settings->subscribe<Settings::Filters::FilterDoubleClick>(
        m_self, [this](int action) { m_doubleClickAction = static_cast<TrackAction>(action); });
    m_settings->subscribe<Settings::Filters::FilterMiddleClick>(
        m_self, [this](int action) { m_middleClickAction = static_cast<TrackAction>(action); });
    m_settings->subscribe<Settings::Filters::FilterSendPlayback>(m_self, [this]() { updateAllPlaylistActions(); });
    m_settings->subscribe<Settings::Core::UseVariousForCompilations>(m_self, [this]() { resetAll(); });
}

void FilterControllerPrivate::handleAction(const TrackAction& action) const
{
    PlaylistAction::ActionOptions options;

    if(m_settings->value<Settings::Filters::FilterAutoSwitch>()) {
        options |= PlaylistAction::Switch;
    }
    if(m_settings->value<Settings::Filters::FilterSendPlayback>()) {
        options |= PlaylistAction::StartPlayback;
    }

    m_trackSelection->executeAction(action, options);
}

Id FilterControllerPrivate::findContainingGroup(FilterWidget* widget)
{
    for(const auto& [id, group] : m_groups) {
        for(const auto& filterWidget : group.filters) {
            if(filterWidget == widget) {
                return id;
            }
        }
    }
    return {};
}

void FilterControllerPrivate::handleFilterUpdated(FilterWidget* widget)
{
    const Id groupId  = widget->group();
    const Id oldGroup = findContainingGroup(widget);

    if(groupId == oldGroup) {
        if(!groupId.isValid()) {
            // Ungrouped
            widget->reset(m_library->tracks());
            return;
        }
        resetGroup(widget->group());
        return;
    }

    // Remove from old group
    if(!oldGroup.isValid()) {
        m_ungrouped.erase(widget->id());
    }
    else if(m_groups.contains(oldGroup)) {
        if(std::erase(m_groups.at(oldGroup).filters, widget) > 0) {
            if(m_groups.at(oldGroup).filters.empty()) {
                m_groups.erase(oldGroup);
            }
            recalculateIndexesOfGroup(oldGroup);
        }
    }

    // Add to new group
    if(!groupId.isValid()) {
        m_ungrouped.emplace(widget->id(), widget);
    }
    else {
        FilterGroup& group = m_groups[groupId];
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

void FilterControllerPrivate::filterContextMenu(const QPoint& pos) const
{
    auto* menu = new QMenu();
    menu->setAttribute(Qt::WA_DeleteOnClose);

    m_trackSelection->addTrackPlaylistContextMenu(menu);
    m_trackSelection->addTrackQueueContextMenu(menu);
    menu->addSeparator();
    m_trackSelection->addTrackContextMenu(menu);

    menu->popup(pos);
}

TrackList FilterControllerPrivate::tracks(const Id& groupId) const
{
    if(!m_groups.contains(groupId)) {
        return m_library->tracks();
    }

    const FilterGroup& group = m_groups.at(groupId);

    return group.filteredTracks.empty() ? m_library->tracks() : group.filteredTracks;
}

void FilterControllerPrivate::recalculateIndexesOfGroup(const Id& group)
{
    if(!m_groups.contains(group)) {
        return;
    }

    for(auto index{0}; const auto& filter : m_groups.at(group).filters) {
        filter->setIndex(index++);
    }
}

void FilterControllerPrivate::resetAll()
{
    for(auto& [id, group] : m_groups) {
        group.filteredTracks.clear();
        for(const auto& filterWidget : group.filters) {
            filterWidget->reset(tracks(id));
        }
    }

    for(auto* filterWidget : m_ungrouped | std::views::values) {
        filterWidget->reset(m_library->tracks());
    }
}

void FilterControllerPrivate::resetGroup(const Id& group)
{
    if(!m_groups.contains(group)) {
        return;
    }

    auto& filterGroup = m_groups.at(group);
    filterGroup.filteredTracks.clear();
    for(const auto& filterWidget : filterGroup.filters) {
        filterWidget->reset(tracks(group));
    }
}

void FilterControllerPrivate::resetFiltersAfterFilter(FilterWidget* filter)
{
    const Id group = filter->group();

    if(!m_groups.contains(group)) {
        return;
    }

    const int resetIndex = filter->index() - 1;

    for(const auto& filterWidget : m_groups.at(group).filters) {
        if(filterWidget->index() > resetIndex) {
            filterWidget->reset(tracks(group));
        }
    }
}

void FilterControllerPrivate::getFilteredTracks(const Id& groupId)
{
    if(!m_groups.contains(groupId)) {
        return;
    }

    FilterGroup& group = m_groups.at(groupId);
    group.filteredTracks.clear();

    auto activeFilters = group.filters | std::views::filter([](FilterWidget* widget) { return widget->isActive(); });

    for(auto& filter : activeFilters) {
        if(group.filteredTracks.empty()) {
            std::ranges::copy(filter->filteredTracks(), std::back_inserter(group.filteredTracks));
        }
        else {
            group.filteredTracks = trackIntersection(filter->filteredTracks(), group.filteredTracks);
        }
    }
}

void FilterControllerPrivate::clearActiveFilters(const Id& group, int index)
{
    if(!m_groups.contains(group)) {
        return;
    }

    for(const auto& filter : m_groups.at(group).filters) {
        if(filter->index() > index) {
            filter->clearFilteredTracks();
        }
    }
}

void FilterControllerPrivate::updateFilterPlaylistActions(FilterWidget* filterWidget) const
{
    const bool startPlayback = m_settings->value<Settings::Filters::FilterSendPlayback>();

    m_trackSelection->changePlaybackOnSend(filterWidget->widgetContext(), startPlayback);
}

void FilterControllerPrivate::updateAllPlaylistActions()
{
    for(const auto& [_, group] : m_groups) {
        for(FilterWidget* filterWidget : group.filters) {
            updateFilterPlaylistActions(filterWidget);
        }
    }
}

void FilterControllerPrivate::selectionChanged(FilterWidget* filter)
{
    m_trackSelection->changeSelectedTracks(filter->widgetContext(), filter->filteredTracks());

    if(m_settings->value<Settings::Filters::FilterPlaylistEnabled>()) {
        PlaylistAction::ActionOptions options{PlaylistAction::None};

        if(m_settings->value<Settings::Filters::FilterKeepAlive>()) {
            options |= PlaylistAction::KeepActive;
        }
        if(m_settings->value<Settings::Filters::FilterAutoSwitch>()) {
            options |= PlaylistAction::Switch;
        }

        const QString autoPlaylist = m_settings->value<Settings::Filters::FilterAutoPlaylist>();
        m_trackSelection->executeAction(TrackAction::SendNewPlaylist, options, autoPlaylist);
    }

    const Id group       = filter->group();
    const int resetIndex = filter->index();
    clearActiveFilters(group, resetIndex);
    getFilteredTracks(group);

    if(!m_groups.contains(group)) {
        return;
    }

    for(const auto& filterWidget : m_groups.at(group).filters) {
        if(filterWidget->index() > resetIndex) {
            filterWidget->reset(tracks(group));
        }
    }
}

void FilterControllerPrivate::removeLibraryTracks(int libraryId)
{
    for(const auto& [_, group] : m_groups) {
        for(const auto& filterWidget : group.filters) {
            TrackList cleanedTracks;
            std::ranges::copy_if(filterWidget->filteredTracks(), std::back_inserter(cleanedTracks),
                                 [libraryId](const Track& track) { return track.libraryId() != libraryId; });
            filterWidget->setFilteredTracks(cleanedTracks);
        }
    }
}

void FilterControllerPrivate::handleTracksAddedUpdated(const TrackList& tracks, bool updated)
{
    for(const auto& [_, group] : m_groups) {
        const int count = static_cast<int>(group.filters.size());
        TrackList activeFilterTracks;

        for(const auto& filterWidget : group.filters) {
            if(updated) {
                QObject::connect(
                    filterWidget, &FilterWidget::finishedUpdating, filterWidget,
                    [this, count, filterWidget]() {
                        const auto groupId = filterWidget->group();
                        if(m_groups.contains(groupId)) {
                            int& updateCount = m_groups.at(groupId).updateCount;
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
                Utils::asyncExec([search = filterWidget->searchFilter(), tracks]() {
                    ScriptParser parser;
                    return parser.filter(search, tracks);
                }).then(m_self, [filterWidget, updated](const TrackList& filteredTracks) {
                    if(updated) {
                        filterWidget->tracksChanged(filteredTracks);
                    }
                    else {
                        filterWidget->tracksAdded(filteredTracks);
                    }
                });
            }
            else if(activeFilterTracks.empty()) {
                if(updated) {
                    filterWidget->tracksChanged(tracks);
                }
                else {
                    filterWidget->tracksAdded(tracks);
                }
            }
            else {
                const auto filtered = trackIntersection(activeFilterTracks, tracks);
                if(updated) {
                    filterWidget->tracksChanged(filtered);
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

void FilterControllerPrivate::refreshFilters(const Id& groupId)
{
    if(!m_groups.contains(groupId)) {
        return;
    }

    FilterGroup& group = m_groups.at(groupId);
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

void FilterControllerPrivate::searchChanged(FilterWidget* filter, const QString& search)
{
    const Id groupId = filter->group();

    if(!m_groups.contains(groupId)) {
        return;
    }

    if(filter->searchFilter().length() >= 2 && search.length() < 2) {
        filter->reset(m_library->tracks());
        return;
    }

    if(search.length() < 2) {
        return;
    }

    const TrackList tracksToFilter = m_library->tracks();
    Utils::asyncExec([search, tracksToFilter]() {
        ScriptParser parser;
        return parser.filter(search, tracksToFilter);
    }).then(m_self, [filter](const TrackList& filteredTracks) { filter->reset(filteredTracks); });
}

FilterController::FilterController(const CorePluginContext& core, TrackSelectionController* trackSelection,
                                   EditableLayout* editableLayout, SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<FilterControllerPrivate>(this, core, trackSelection, editableLayout, settings)}
{
    QObject::connect(p->m_library, &MusicLibrary::tracksAdded, this,
                     [this](const TrackList& tracks) { p->handleTracksAddedUpdated(tracks); });
    QObject::connect(p->m_library, &MusicLibrary::tracksScanned, this,
                     [this](int /*id*/, const TrackList& tracks) { p->handleTracksAddedUpdated(tracks); });
    QObject::connect(p->m_library, &MusicLibrary::tracksMetadataChanged, this,
                     [this](const TrackList& tracks) { p->handleTracksAddedUpdated(tracks, true); });
    QObject::connect(p->m_library, &MusicLibrary::tracksUpdated, this, &FilterController::tracksUpdated);
    QObject::connect(p->m_library, &MusicLibrary::tracksDeleted, this, &FilterController::tracksRemoved);
    QObject::connect(p->m_library, &MusicLibrary::tracksLoaded, this, [this]() { p->resetAll(); });
    QObject::connect(p->m_library, &MusicLibrary::tracksSorted, this, [this]() { p->resetAll(); });
}

FilterController::~FilterController() = default;

FilterColumnRegistry* FilterController::columnRegistry() const
{
    return p->m_columnRegistry;
}

FilterWidget* FilterController::createFilter()
{
    auto* widget = new FilterWidget(p->m_columnRegistry, p->m_libraryManager, &p->m_coverProvider, p->m_settings);

    auto& group = p->m_groups[p->m_defaultId];
    group.id    = p->m_defaultId;
    widget->setGroup(p->m_defaultId);
    widget->setIndex(static_cast<int>(group.filters.size()));
    group.filters.push_back(widget);

    QObject::connect(widget, &FilterWidget::doubleClicked, this, [this]() { p->handleAction(p->m_doubleClickAction); });
    QObject::connect(widget, &FilterWidget::middleClicked, this, [this]() { p->handleAction(p->m_middleClickAction); });
    QObject::connect(widget, &FilterWidget::requestSearch, this,
                     [this, widget](const QString& search) { p->searchChanged(widget, search); });
    QObject::connect(widget, &FilterWidget::requestContextMenu, this,
                     [this](const QPoint& pos) { p->filterContextMenu(pos); });
    QObject::connect(widget, &FilterWidget::selectionChanged, this, [this, widget]() { p->selectionChanged(widget); });
    QObject::connect(widget, &FilterWidget::filterUpdated, this, [this, widget]() { p->handleFilterUpdated(widget); });
    QObject::connect(widget, &FilterWidget::filterDeleted, this, [this, widget]() { removeFilter(widget); });
    QObject::connect(widget, &FilterWidget::requestEditConnections, this,
                     [this]() { p->m_manager->setupWidgetConnections(); });
    QObject::connect(this, &FilterController::tracksChanged, widget, &FilterWidget::tracksChanged);
    QObject::connect(this, &FilterController::tracksUpdated, widget, &FilterWidget::tracksUpdated);
    QObject::connect(this, &FilterController::tracksRemoved, widget, &FilterWidget::tracksRemoved);

    widget->reset(p->tracks(p->m_defaultId));
    p->updateFilterPlaylistActions(widget);

    return widget;
}

bool FilterController::haveUngroupedFilters() const
{
    return !p->m_ungrouped.empty();
}

bool FilterController::filterIsUngrouped(const Id& id) const
{
    return p->m_ungrouped.contains(id);
}

FilterGroups FilterController::filterGroups() const
{
    return p->m_groups;
}

std::optional<FilterGroup> FilterController::groupById(const Id& id) const
{
    if(!p->m_groups.contains(id)) {
        return {};
    }
    return p->m_groups.at(id);
}

UngroupedFilters FilterController::ungroupedFilters() const
{
    return p->m_ungrouped;
}

void FilterController::addFilterToGroup(FilterWidget* widget, const Id& groupId)
{
    if(!groupId.isValid()) {
        widget->setGroup("");
        widget->setIndex(-1);
        p->m_ungrouped.emplace(widget->id(), widget);
    }
    else {
        auto& group = p->m_groups[groupId];
        group.id    = groupId;
        widget->setGroup(groupId);
        widget->setIndex(static_cast<int>(group.filters.size()));
        group.filters.push_back(widget);
    }
}

bool FilterController::removeFilter(FilterWidget* widget)
{
    const Id groupId = widget->group();

    if(!groupId.isValid() && p->m_ungrouped.contains(widget->id())) {
        p->m_ungrouped.erase(widget->id());
        return true;
    }

    if(!p->m_groups.contains(groupId)) {
        return false;
    }

    auto& group = p->m_groups.at(groupId);

    if(std::erase(group.filters, widget) > 0) {
        p->recalculateIndexesOfGroup(groupId);

        if(group.filters.empty()) {
            p->m_groups.erase(groupId);
        }

        return true;
    }

    return false;
}
} // namespace Fooyin::Filters

#include "moc_filtercontroller.cpp"
