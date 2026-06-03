/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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
#include "filtercontextmenu.h"
#include "filtermanager.h"
#include "filterpipeline.h"
#include "filterrows.h"
#include "filterwidget.h"

#include <core/coresettings.h>
#include <core/library/musiclibrary.h>
#include <core/plugins/coreplugincontext.h>
#include <gui/contextmenuutils.h>
#include <gui/coverprovider.h>
#include <gui/coverrepository.h>
#include <gui/editablelayout.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/guiutils.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgets/autoheaderview.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/async.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QPointer>

#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace Fooyin::Filters {
namespace {
std::vector<FilterWidget*> dedupeFilters(const std::vector<FilterWidget*>& filters)
{
    std::vector<FilterWidget*> uniqueFilters;
    uniqueFilters.reserve(filters.size());

    std::unordered_set<FilterWidget*> seen;
    seen.reserve(filters.size());

    for(FilterWidget* filter : filters) {
        if(!filter || seen.contains(filter)) {
            continue;
        }

        seen.emplace(filter);
        uniqueFilters.push_back(filter);
    }

    return uniqueFilters;
}

void filterHeaderContextMenu(FilterWidget* widget, AutoHeaderView* header, const QPoint& pos)
{
    if(!header) {
        return;
    }

    auto* menu = new QMenu(widget);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    widget->addFilterHeaderMenu(menu, pos);
    menu->popup(header->mapToGlobal(pos));
}

Id makeUngroupedGroupId(FilterWidget* widget)
{
    return Id{"Fooyin.Filters.Ungrouped."}.append(widget ? widget->id() : Id{});
}

bool isUngroupedGroupId(const Id& groupId)
{
    return groupId.name().startsWith(QStringLiteral("Fooyin.Filters.Ungrouped."));
}

bool containsSummaryKey(const std::vector<RowKey>& keys)
{
    return std::ranges::any_of(keys, [](const RowKey& key) { return key.isEmpty(); });
}
} // namespace

class FilterControllerPrivate
{
public:
    struct WidgetLocation
    {
        Id groupId;
        int stageIndex{-1};
    };

    struct FilterStageState
    {
        Id widgetId;
        QPointer<FilterWidget> widget;

        TrackList inputTracks;
        TrackList selectedTracks;

        FilterRowList rows;
        std::optional<FilterRowList> searchedRows;

        std::vector<RowKey> selectedKeys;
        QString searchText;

        bool isActive{false};
        uint64_t revision{0};
        uint64_t searchRevision{0};
    };

    struct FilterGroupState
    {
        Id id;
        std::vector<FilterWidget*> filters;
        std::vector<FilterStageState> stages;

        TrackList sourceTracks;
        TrackList finalFilteredTracks;
        bool hasActiveStages{false};
        uint64_t revision{0};
        bool recomputePending{false};
    };

    explicit FilterControllerPrivate(FilterController* self, ActionManager* actionManager,
                                     const CorePluginContext& core, TrackSelectionController* trackSelection,
                                     EditableLayout* editableLayout, CoverRepository* coverRepository,
                                     SettingsManager* settings);

    void handleAction(FilterWidget* filter, const TrackAction& action) const;
    void filterContextMenu(FilterWidget* widget, const QPoint& pos) const;

    [[nodiscard]] FilterRowBuildContext rowBuildContext() const;
    [[nodiscard]] std::optional<WidgetLocation> widgetLocation(FilterWidget* widget) const;
    [[nodiscard]] std::optional<Id> widgetGroupId(FilterWidget* widget) const;
    [[nodiscard]] FilterGroupState* groupState(FilterWidget* widget);
    [[nodiscard]] FilterStageState* stageState(FilterWidget* widget);

    void updateFilterPlaylistActions(FilterWidget* filterWidget) const;
    void updateAllPlaylistActions();
    void updateWidgetSelection(FilterStageState& stage) const;
    static const FilterRowList& rowsForSelection(const FilterStageState& stage);
    static void markStagesCurrent(FilterGroupState& group, int lastStageIndex);
    void handleSelectionChanged(FilterWidget* filter, const std::vector<RowKey>& keys);
    void handleSearchChanged(FilterWidget* filter, const QString& search);

    void syncStages(FilterGroupState& group);
    void rebuildWidgetLocations(const FilterGroupState& group);
    void sortGroupedFilters(FilterGroupState& group);
    void scheduleRecompute(const Id& groupId);
    void scheduleAllRecomputes();
    void handleLibraryTracksPatched(const TrackList& changedTracks);
    [[nodiscard]] bool patchGroup(const Id& groupId, const TrackList& sourceTracks, const TrackIds& changedTrackIds);
    void recomputeGroup(const Id& groupId);
    void recomputeStage(const Id& groupId, int stageIndex, uint64_t revision, TrackList currentTracks,
                        bool constrained);
    void refreshStageSearch(const Id& groupId, int stageIndex, uint64_t revision);
    void publishStage(const Id& groupId, int stageIndex);

    void handleFilterUpdated(FilterWidget* widget);
    void attachWidget(FilterWidget* widget, const Id& publicGroupId);
    [[nodiscard]] std::optional<Id> detachWidget(FilterWidget* widget);
    void recalculateIndexesOfGroup(const Id& groupId);
    [[nodiscard]] FilterGroups publicGroups() const;
    [[nodiscard]] std::optional<FilterGroup> publicGroup(const Id& id) const;

    FilterController* m_self;

    ActionManager* m_actionManager;
    MusicLibrary* m_library;
    LibraryManager* m_libraryManager;
    TrackSelectionController* m_trackSelection;
    std::shared_ptr<AudioLoader> m_audioLoader;
    CoverRepository* m_coverRepository;
    EditableLayout* m_editableLayout;
    SettingsManager* m_settings;

    FilterManager* m_manager;
    FilterColumnRegistry* m_columnRegistry;

    Id m_defaultId{"Default"};
    std::unordered_map<Id, FilterGroupState, Id::IdHash> m_groups;
    std::unordered_map<FilterWidget*, WidgetLocation> m_widgetLocations;
    std::unordered_set<FilterWidget*> m_ungrouped;
};

FilterControllerPrivate::FilterControllerPrivate(FilterController* self, ActionManager* actionManager,
                                                 const CorePluginContext& core,
                                                 TrackSelectionController* trackSelection,
                                                 EditableLayout* editableLayout, CoverRepository* coverRepository,
                                                 SettingsManager* settings)
    : m_self{self}
    , m_actionManager{actionManager}
    , m_library{core.library}
    , m_libraryManager{core.libraryManager}
    , m_trackSelection{trackSelection}
    , m_audioLoader{core.audioLoader}
    , m_coverRepository{coverRepository}
    , m_editableLayout{editableLayout}
    , m_settings{settings}
    , m_manager{new FilterManager(m_self, m_editableLayout, m_self)}
    , m_columnRegistry{new FilterColumnRegistry(settings, m_self)}
{
    m_settings->subscribe<Settings::Core::UseVariousForCompilations>(m_self, [this]() { scheduleAllRecomputes(); });
}

void FilterControllerPrivate::handleAction(FilterWidget* filter, const TrackAction& action) const
{
    if(!filter || action == TrackAction::None || !m_trackSelection) {
        return;
    }

    PlaylistAction::ActionOptions options;

    if(filter->autoSwitch()) {
        options |= PlaylistAction::Switch;
    }
    if(filter->sendPlayback()) {
        options |= PlaylistAction::StartPlayback;
    }

    m_trackSelection->executeAction(action, options);
}

void FilterControllerPrivate::filterContextMenu(FilterWidget* widget, const QPoint& pos) const
{
    auto* menu = new QMenu(widget);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const bool hasSelection = widget->hasSelection();

    ContextMenuUtils::renderStaticContextMenu(
        menu, FilterContextMenu::DefaultItems,
        m_settings->fileValue(FilterContextMenu::LayoutKey, QStringList{}).toStringList(),
        m_settings->fileValue(FilterContextMenu::DisabledSectionsKey, FilterContextMenu::defaultDisabledSections())
            .toStringList(),
        [&](const auto& id, QMenu* targetMenu, const auto& sectionEnabled) {
            if(id == QLatin1StringView{Constants::Actions::AddToCurrent}) {
                if(!hasSelection || !m_trackSelection || !sectionEnabled(Constants::Actions::AddToCurrent)) {
                    return;
                }

                if(auto* addCurrentCmd = m_actionManager->command(Constants::Actions::AddToCurrent)) {
                    targetMenu->addAction(addCurrentCmd->action());
                }
                return;
            }
            if(id == QLatin1StringView{Constants::Actions::AddToActive}) {
                if(!hasSelection || !m_trackSelection || !sectionEnabled(Constants::Actions::AddToActive)) {
                    return;
                }

                if(auto* addActiveCmd = m_actionManager->command(Constants::Actions::AddToActive)) {
                    targetMenu->addAction(addActiveCmd->action());
                }
                return;
            }
            if(id == QLatin1StringView{Constants::Actions::SendToCurrent}) {
                if(!hasSelection || !m_trackSelection || !sectionEnabled(Constants::Actions::SendToCurrent)) {
                    return;
                }

                if(auto* sendCurrentCmd = m_actionManager->command(Constants::Actions::SendToCurrent)) {
                    targetMenu->addAction(sendCurrentCmd->action());
                }
                return;
            }
            if(id == QLatin1StringView{Constants::Actions::SendToNew}) {
                if(!hasSelection || !m_trackSelection || !sectionEnabled(Constants::Actions::SendToNew)) {
                    return;
                }

                if(auto* sendNewCmd = m_actionManager->command(Constants::Actions::SendToNew)) {
                    targetMenu->addAction(sendNewCmd->action());
                }
                return;
            }
            if(id == QLatin1StringView{Constants::Actions::AddToPlaylist}) {
                if(hasSelection && m_trackSelection && sectionEnabled(Constants::Actions::AddToPlaylist)) {
                    auto* playlistMenu = new QMenu(FilterWidget::tr("Add to playlist"), targetMenu);
                    m_trackSelection->addTrackAddToPlaylistContextMenu(playlistMenu);
                    targetMenu->addMenu(playlistMenu);
                }
                return;
            }
            if(id == QLatin1StringView{Constants::Actions::AddToQueue}) {
                if(!hasSelection || !m_trackSelection || !sectionEnabled(Constants::Actions::AddToQueue)) {
                    return;
                }

                if(auto* addQueueCmd = m_actionManager->command(Constants::Actions::AddToQueue)) {
                    targetMenu->addAction(addQueueCmd->action());
                }
                return;
            }
            if(id == QLatin1StringView{Constants::Actions::QueueNext}) {
                if(!hasSelection || !m_trackSelection || !sectionEnabled(Constants::Actions::QueueNext)) {
                    return;
                }

                if(auto* queueNextCmd = m_actionManager->command(Constants::Actions::QueueNext)) {
                    targetMenu->addAction(queueNextCmd->action());
                }
                return;
            }
            if(id == QLatin1StringView{Constants::Actions::RemoveFromQueue}) {
                if(!hasSelection || !m_trackSelection || !sectionEnabled(Constants::Actions::RemoveFromQueue)) {
                    return;
                }

                if(auto* removeQueueCmd = m_actionManager->command(Constants::Actions::RemoveFromQueue)) {
                    targetMenu->addAction(removeQueueCmd->action());
                }
                return;
            }
            if(id == QLatin1StringView{FilterContextMenu::FilterOptions}) {
                if(sectionEnabled(FilterContextMenu::FilterOptions)) {
                    auto* headerMenu = new QMenu(FilterWidget::tr("Filter options"), targetMenu);
                    targetMenu->addMenu(headerMenu);
                    widget->addFilterHeaderMenu(headerMenu, pos, false);
                }
                return;
            }
            if(id == QLatin1StringView{FilterContextMenu::Configure}) {
                if(sectionEnabled(FilterContextMenu::Configure)) {
                    auto* configure = new QAction(FilterWidget::tr("Configure…"), targetMenu);
                    QObject::connect(configure, &QAction::triggered, widget,
                                     [widget]() { widget->openConfigDialog(); });
                    targetMenu->addAction(configure);
                }
                return;
            }
            if(id == QLatin1StringView{FilterContextMenu::TrackActions}) {
                if(hasSelection && m_trackSelection && sectionEnabled(FilterContextMenu::TrackActions)) {
                    m_trackSelection->addTrackContextMenu(targetMenu);
                }
            }
        });

    menu->popup(pos);
}

FilterRowBuildContext FilterControllerPrivate::rowBuildContext() const
{
    return {
        .font          = QApplication::font("Fooyin::Filters::FilterView"),
        .ratingSymbols = Gui::ratingStarSymbols(*m_settings),
        .useVarious    = m_settings->value<Settings::Core::UseVariousForCompilations>(),
    };
}

std::optional<FilterControllerPrivate::WidgetLocation>
FilterControllerPrivate::widgetLocation(FilterWidget* widget) const
{
    if(!widget || !m_widgetLocations.contains(widget)) {
        return {};
    }

    return m_widgetLocations.at(widget);
}

std::optional<Id> FilterControllerPrivate::widgetGroupId(FilterWidget* widget) const
{
    const auto location = widgetLocation(widget);
    if(!location) {
        return {};
    }

    return location->groupId;
}

FilterControllerPrivate::FilterGroupState* FilterControllerPrivate::groupState(FilterWidget* widget)
{
    const auto location = widgetLocation(widget);
    if(!location || !m_groups.contains(location->groupId)) {
        return nullptr;
    }

    return &m_groups.at(location->groupId);
}

FilterControllerPrivate::FilterStageState* FilterControllerPrivate::stageState(FilterWidget* widget)
{
    const auto location = widgetLocation(widget);
    if(!location || !m_groups.contains(location->groupId)) {
        return nullptr;
    }

    auto& group = m_groups.at(location->groupId);
    if(location->stageIndex < 0 || std::cmp_greater_equal(location->stageIndex, group.stages.size())) {
        return nullptr;
    }

    auto& stage = group.stages.at(location->stageIndex);
    if(stage.widget != widget) {
        return nullptr;
    }

    return &stage;
}

void FilterControllerPrivate::updateFilterPlaylistActions(FilterWidget* filterWidget) const
{
    if(!filterWidget || !m_trackSelection) {
        return;
    }

    m_trackSelection->changePlaybackOnSend(filterWidget->widgetContext(), filterWidget->sendPlayback());
}

void FilterControllerPrivate::updateAllPlaylistActions()
{
    for(auto& [_, group] : m_groups) {
        for(FilterWidget* filterWidget : group.filters) {
            updateFilterPlaylistActions(filterWidget);
        }
    }
}

void FilterControllerPrivate::updateWidgetSelection(FilterStageState& stage) const
{
    if(!stage.widget || !m_trackSelection) {
        return;
    }

    TrackSelection selection;
    selection.tracks = Gui::sortTracksForLibraryViewerPlaylist(m_settings, stage.selectedTracks);
    m_trackSelection->changeSelectedTracks(stage.widget->widgetContext(), selection);
}

const FilterRowList& FilterControllerPrivate::rowsForSelection(const FilterStageState& stage)
{
    return stage.searchedRows ? *stage.searchedRows : stage.rows;
}

void FilterControllerPrivate::markStagesCurrent(FilterGroupState& group, int lastStageIndex)
{
    if(lastStageIndex < 0) {
        return;
    }

    const int stageCount = std::min(lastStageIndex + 1, static_cast<int>(group.stages.size()));
    for(auto& state : group.stages | std::views::take(stageCount)) {
        state.revision = group.revision;
    }
}

void FilterControllerPrivate::handleSelectionChanged(FilterWidget* filter, const std::vector<RowKey>& keys)
{
    const auto location = widgetLocation(filter);
    auto* group         = groupState(filter);
    auto* stage         = stageState(filter);
    if(!location || !group || !stage) {
        return;
    }

    const FilterSelectionResolution selection
        = resolveFilterSelection(rowsForSelection(*stage), stage->inputTracks, keys);
    stage->selectedKeys   = selection.selectedKeys;
    stage->selectedTracks = selection.selectedTracks;
    stage->isActive       = selection.isActive;
    updateWidgetSelection(*stage);

    if(filter->playlistEnabled() && m_trackSelection) {
        PlaylistAction::ActionOptions options{PlaylistAction::None};

        if(filter->keepAlive()) {
            options |= PlaylistAction::KeepActive;
        }
        if(filter->autoSwitch()) {
            options |= PlaylistAction::Switch;
        }

        m_trackSelection->executeAction(TrackAction::SendNewPlaylist, options, filter->playlistName());
    }

    const int stageIndex      = location->stageIndex;
    const bool hasPriorActive = std::ranges::any_of(group->stages | std::views::take(stageIndex),
                                                    [](const FilterStageState& state) { return state.isActive; });

    if(stage->searchText.isEmpty()) {
        stage->searchedRows.reset();
    }
    publishStage(group->id, stageIndex);

    ++group->revision;
    markStagesCurrent(*group, stageIndex);

    const TrackList nextTracks = stage->isActive ? stage->selectedTracks : stage->inputTracks;
    const bool nextConstrained = hasPriorActive || stage->isActive;

    recomputeStage(group->id, stageIndex + 1, group->revision, nextTracks, nextConstrained);
}

void FilterControllerPrivate::handleSearchChanged(FilterWidget* filter, const QString& search)
{
    const auto location = widgetLocation(filter);
    auto* group         = groupState(filter);
    auto* stage         = stageState(filter);
    if(!location || !group || !stage) {
        return;
    }

    if(std::exchange(stage->searchText, search) == search) {
        return;
    }

    if(!stage->selectedKeys.empty()) {
        stage->selectedKeys.clear();
        stage->selectedTracks.clear();
        stage->isActive = false;
        updateWidgetSelection(*stage);

        ++group->revision;
        markStagesCurrent(*group, location->stageIndex);

        publishStage(group->id, location->stageIndex);
        scheduleRecompute(group->id);
        return;
    }

    if(group->recomputePending) {
        return;
    }

    refreshStageSearch(group->id, location->stageIndex, group->revision);
}

void FilterControllerPrivate::syncStages(FilterGroupState& group)
{
    group.filters = dedupeFilters(group.filters);

    if(!isUngroupedGroupId(group.id)) {
        sortGroupedFilters(group);
    }

    std::unordered_map<Id, FilterStageState, Id::IdHash> existing;
    for(auto& stage : group.stages) {
        existing.emplace(stage.widgetId, std::move(stage));
    }

    std::vector<FilterStageState> stages;
    stages.reserve(group.filters.size());

    for(FilterWidget* filter : group.filters) {
        FilterStageState stage;
        if(existing.contains(filter->id())) {
            stage = std::move(existing.at(filter->id()));
        }

        stage.widgetId = filter->id();
        stage.widget   = filter;

        if(stage.searchText.isNull()) {
            stage.searchText = filter->searchText();
        }
        if(stage.selectedKeys.empty()) {
            stage.selectedKeys = filter->selectedKeys();
        }

        stages.push_back(std::move(stage));
    }

    group.stages = std::move(stages);
    rebuildWidgetLocations(group);
}

void FilterControllerPrivate::rebuildWidgetLocations(const FilterGroupState& group)
{
    for(int stageIndex{0}; std::cmp_less(stageIndex, group.stages.size()); ++stageIndex) {
        if(FilterWidget* widget = group.stages.at(stageIndex).widget) {
            m_widgetLocations[widget] = WidgetLocation{
                .groupId    = group.id,
                .stageIndex = stageIndex,
            };
        }
    }
}

void FilterControllerPrivate::sortGroupedFilters(FilterGroupState& group)
{
    std::ranges::stable_sort(group.filters, [](const FilterWidget* left, const FilterWidget* right) {
        return left->index() < right->index();
    });
}

void FilterControllerPrivate::scheduleRecompute(const Id& groupId)
{
    if(!m_groups.contains(groupId)) {
        return;
    }

    auto& group = m_groups.at(groupId);
    if(std::exchange(group.recomputePending, true)) {
        return;
    }

    QMetaObject::invokeMethod(
        m_self,
        [this, groupId]() {
            if(!m_groups.contains(groupId)) {
                return;
            }

            auto& currentGroup            = m_groups.at(groupId);
            currentGroup.recomputePending = false;
            recomputeGroup(groupId);
        },
        Qt::QueuedConnection);
}

void FilterControllerPrivate::scheduleAllRecomputes()
{
    for(const auto& groupId : m_groups | std::views::keys) {
        scheduleRecompute(groupId);
    }
}

void FilterControllerPrivate::handleLibraryTracksPatched(const TrackList& changedTracks)
{
    if(changedTracks.empty()) {
        return;
    }

    const TrackList sourceTracks = m_library->libraryTracks();

    TrackIds changedTrackIds;
    changedTrackIds.reserve(changedTracks.size());
    for(const Track& track : changedTracks) {
        changedTrackIds.push_back(track.id());
    }

    for(const auto& groupId : m_groups | std::views::keys) {
        if(!patchGroup(groupId, sourceTracks, changedTrackIds)) {
            scheduleRecompute(groupId);
        }
    }
}

bool FilterControllerPrivate::patchGroup(const Id& groupId, const TrackList& sourceTracks,
                                         const TrackIds& changedTrackIds)
{
    if(!m_groups.contains(groupId)) {
        return false;
    }

    auto& group = m_groups.at(groupId);
    ++group.revision;
    group.sourceTracks = sourceTracks;

    const auto context      = rowBuildContext();
    TrackList currentTracks = group.sourceTracks;
    bool constrained{false};

    for(int stageIndex{0}; std::cmp_less(stageIndex, group.stages.size()); ++stageIndex) {
        auto& stage                         = group.stages.at(stageIndex);
        const TrackList previousInputTracks = stage.inputTracks;
        const FilterColumnList columns      = stage.widget ? stage.widget->columns() : FilterColumnList{};

        stage.inputTracks = currentTracks;
        stage.rows     = patchFilterRows(m_libraryManager, columns, stage.rows, previousInputTracks, stage.inputTracks,
                                         changedTrackIds, context);
        stage.revision = group.revision;

        const FilterSelectionResolution selection
            = resolveFilterSelection(stage.rows, stage.inputTracks, stage.selectedKeys);
        stage.selectedKeys   = selection.selectedKeys;
        stage.selectedTracks = selection.selectedTracks;
        stage.isActive       = selection.isActive;

        refreshStageSearch(groupId, stageIndex, group.revision);

        if(stage.isActive) {
            currentTracks = stage.selectedTracks;
            constrained   = true;
        }
        else {
            currentTracks = stage.inputTracks;
        }
    }

    group.finalFilteredTracks = constrained ? currentTracks : TrackList{};
    group.hasActiveStages     = constrained;
    return true;
}

void FilterControllerPrivate::recomputeGroup(const Id& groupId)
{
    if(!m_groups.contains(groupId)) {
        return;
    }

    auto& group        = m_groups.at(groupId);
    group.sourceTracks = m_library->libraryTracks();
    group.finalFilteredTracks.clear();
    group.hasActiveStages = false;
    ++group.revision;

    syncStages(group);
    recomputeStage(groupId, 0, group.revision, group.sourceTracks, false);
}

void FilterControllerPrivate::recomputeStage(const Id& groupId, int stageIndex, uint64_t revision,
                                             TrackList currentTracks, bool constrained)
{
    if(!m_groups.contains(groupId)) {
        return;
    }

    auto& group = m_groups.at(groupId);
    if(group.revision != revision) {
        return;
    }

    if(std::cmp_greater_equal(stageIndex, group.stages.size())) {
        group.finalFilteredTracks = constrained ? std::move(currentTracks) : TrackList{};
        group.hasActiveStages     = constrained;
        return;
    }

    auto& stage = group.stages.at(stageIndex);

    stage.inputTracks = currentTracks;
    stage.selectedTracks.clear();
    stage.searchedRows.reset();
    stage.isActive = false;
    stage.revision = revision;

    const FilterColumnList columns = stage.widget ? stage.widget->columns() : FilterColumnList{};
    const auto context             = rowBuildContext();

    Utils::asyncExec([libraryManager = m_libraryManager, columns, tracks = stage.inputTracks, context]() {
        return buildFilterRows(libraryManager, columns, tracks, context);
    })
        .then(m_self, [this, groupId, stageIndex, revision, currentTracks = std::move(currentTracks),
                       constrained](const FilterRowList& rows) mutable {
            if(!m_groups.contains(groupId)) {
                return;
            }

            auto& currentGroup = m_groups.at(groupId);
            if(currentGroup.revision != revision || std::cmp_greater_equal(stageIndex, currentGroup.stages.size())) {
                return;
            }

            auto& currentStage = currentGroup.stages.at(stageIndex);
            if(currentStage.revision != revision) {
                return;
            }

            currentStage.rows = rows;

            const FilterSelectionResolution selection
                = resolveFilterSelection(currentStage.rows, currentStage.inputTracks, currentStage.selectedKeys);
            currentStage.selectedKeys   = selection.selectedKeys;
            currentStage.selectedTracks = selection.selectedTracks;
            currentStage.isActive       = selection.isActive;

            refreshStageSearch(groupId, stageIndex, revision);

            TrackList nextTracks = currentTracks;
            bool nextConstrained = constrained;

            if(currentStage.isActive) {
                nextTracks      = currentStage.selectedTracks;
                nextConstrained = true;
            }

            recomputeStage(groupId, stageIndex + 1, revision, std::move(nextTracks), nextConstrained);
        });
}

void FilterControllerPrivate::refreshStageSearch(const Id& groupId, int stageIndex, uint64_t revision)
{
    if(!m_groups.contains(groupId)) {
        return;
    }

    auto& group = m_groups.at(groupId);
    if(group.revision != revision || stageIndex < 0 || std::cmp_greater_equal(stageIndex, group.stages.size())) {
        return;
    }

    auto& stage = group.stages.at(stageIndex);
    stage.searchedRows.reset();
    const uint64_t searchRevision = ++stage.searchRevision;

    if(stage.searchText.isEmpty()) {
        publishStage(groupId, stageIndex);
        return;
    }

    const QString searchText = stage.searchText;

    Utils::asyncExec([rows = stage.rows, tracks = stage.inputTracks, searchText]() {
        return filterRowsBySearch(searchText, rows, tracks);
    })
        .then(m_self,
              [this, groupId, stageIndex, revision, searchRevision, searchText](const FilterRowList& searchedRows) {
                  if(!m_groups.contains(groupId)) {
                      return;
                  }

                  auto& currentGroup = m_groups.at(groupId);
                  if(currentGroup.revision != revision || stageIndex < 0
                     || std::cmp_greater_equal(stageIndex, currentGroup.stages.size())) {
                      return;
                  }

                  auto& currentStage = currentGroup.stages.at(stageIndex);
                  if(currentStage.revision != revision || currentStage.searchRevision != searchRevision
                     || currentStage.searchText != searchText) {
                      return;
                  }

                  currentStage.searchedRows = searchedRows;

                  if(containsSummaryKey(currentStage.selectedKeys)) {
                      const FilterSelectionResolution selection = resolveFilterSelection(
                          *currentStage.searchedRows, currentStage.inputTracks, currentStage.selectedKeys);
                      currentStage.selectedKeys   = selection.selectedKeys;
                      currentStage.selectedTracks = selection.selectedTracks;
                      currentStage.isActive       = selection.isActive;

                      const bool hasPriorActive = std::ranges::any_of(
                          currentGroup.stages | std::views::take(stageIndex), &FilterStageState::isActive);
                      const TrackList nextTracks
                          = currentStage.isActive ? currentStage.selectedTracks : currentStage.inputTracks;
                      const bool nextConstrained = hasPriorActive || currentStage.isActive;

                      recomputeStage(groupId, stageIndex + 1, revision, nextTracks, nextConstrained);
                  }

                  publishStage(groupId, stageIndex);
              });
}

void FilterControllerPrivate::publishStage(const Id& groupId, int stageIndex)
{
    if(!m_groups.contains(groupId)) {
        return;
    }

    auto& group = m_groups.at(groupId);
    if(stageIndex < 0 || std::cmp_greater_equal(stageIndex, group.stages.size())) {
        return;
    }

    auto& stage = group.stages.at(stageIndex);
    if(!stage.widget) {
        return;
    }

    FilterWidget::FilterViewState viewState;
    viewState.rows         = stage.searchedRows.value_or(stage.rows);
    viewState.selectedKeys = stage.selectedKeys;
    viewState.searchText   = stage.searchText;

    stage.widget->setViewState(viewState);
    updateWidgetSelection(stage);
}

void FilterControllerPrivate::handleFilterUpdated(FilterWidget* widget)
{
    const Id newPublicGroup = widget->group();
    const Id newGroupId     = newPublicGroup.isValid() ? newPublicGroup : makeUngroupedGroupId(widget);
    const auto oldGroupId   = widgetGroupId(widget);

    if(oldGroupId && oldGroupId.value() == newGroupId) {
        if(m_groups.contains(newGroupId)) {
            auto& group = m_groups.at(newGroupId);
            if(newPublicGroup.isValid() && widget->index() < 0) {
                widget->setIndex(static_cast<int>(group.filters.size()) - 1);
            }
            syncStages(group);
            scheduleRecompute(newGroupId);
        }
        return;
    }

    const auto removedGroupId = detachWidget(widget);
    attachWidget(widget, newPublicGroup);

    if(removedGroupId) {
        scheduleRecompute(removedGroupId.value());
    }
    scheduleRecompute(newGroupId);
}

void FilterControllerPrivate::attachWidget(FilterWidget* widget, const Id& publicGroupId)
{
    if(!widget) {
        return;
    }

    const Id groupId = publicGroupId.isValid() ? publicGroupId : makeUngroupedGroupId(widget);
    auto& group      = m_groups[groupId];
    group.id         = groupId;

    if(publicGroupId.isValid()) {
        m_ungrouped.erase(widget);
    }
    else {
        m_ungrouped.emplace(widget);
    }

    if(publicGroupId.isValid() && widget->index() < 0) {
        widget->setIndex(static_cast<int>(group.filters.size()));
    }

    group.filters.push_back(widget);

    if(publicGroupId.isValid()) {
        widget->setGroup(publicGroupId);
        sortGroupedFilters(group);
    }
    else {
        widget->setGroup({});
        widget->setIndex(-1);
    }

    syncStages(group);
}

std::optional<Id> FilterControllerPrivate::detachWidget(FilterWidget* widget)
{
    auto groupId = widgetGroupId(widget);
    if(!groupId || !m_groups.contains(groupId.value())) {
        return {};
    }

    auto& group = m_groups.at(groupId.value());
    std::erase(group.filters, widget);
    m_widgetLocations.erase(widget);
    m_ungrouped.erase(widget);

    if(group.filters.empty()) {
        m_groups.erase(groupId.value());
        return groupId;
    }

    syncStages(group);
    if(!isUngroupedGroupId(groupId.value())) {
        recalculateIndexesOfGroup(groupId.value());
    }

    return groupId;
}

void FilterControllerPrivate::recalculateIndexesOfGroup(const Id& groupId)
{
    if(!m_groups.contains(groupId)) {
        return;
    }

    if(isUngroupedGroupId(groupId)) {
        return;
    }

    int index = 0;
    for(FilterWidget* filter : m_groups.at(groupId).filters) {
        filter->setIndex(index++);
    }
}

FilterGroups FilterControllerPrivate::publicGroups() const
{
    FilterGroups groups;

    for(const auto& [groupId, group] : m_groups) {
        if(isUngroupedGroupId(groupId)) {
            continue;
        }

        FilterGroup publicGroup;
        publicGroup.id               = groupId;
        publicGroup.filters          = group.filters;
        publicGroup.filteredTracks   = group.finalFilteredTracks;
        publicGroup.hasActiveFilters = group.hasActiveStages;

        groups.emplace(groupId, std::move(publicGroup));
    }

    return groups;
}

std::optional<FilterGroup> FilterControllerPrivate::publicGroup(const Id& id) const
{
    if(!m_groups.contains(id) || isUngroupedGroupId(id)) {
        return {};
    }

    FilterGroup group;
    group.id               = id;
    group.filters          = m_groups.at(id).filters;
    group.filteredTracks   = m_groups.at(id).finalFilteredTracks;
    group.hasActiveFilters = m_groups.at(id).hasActiveStages;
    return group;
}

FilterController::FilterController(ActionManager* actionManager, const CorePluginContext& core,
                                   TrackSelectionController* trackSelection, EditableLayout* editableLayout,
                                   CoverRepository* coverRepository, SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<FilterControllerPrivate>(this, actionManager, core, trackSelection, editableLayout,
                                                  coverRepository, settings)}
{
    QObject::connect(p->m_library, &MusicLibrary::tracksAdded, this,
                     [this](const TrackList& tracks) { p->handleLibraryTracksPatched(tracks); });
    QObject::connect(p->m_library, &MusicLibrary::tracksScanned, this, [this]() { p->scheduleAllRecomputes(); });
    QObject::connect(p->m_library, &MusicLibrary::tracksMetadataChanged, this,
                     [this](const TrackList& tracks) { p->handleLibraryTracksPatched(tracks); });
    QObject::connect(p->m_library, &MusicLibrary::tracksUpdated, this,
                     [this](const TrackList& tracks) { p->handleLibraryTracksPatched(tracks); });
    QObject::connect(p->m_library, &MusicLibrary::tracksDeleted, this,
                     [this](const TrackList& tracks) { p->handleLibraryTracksPatched(tracks); });
    QObject::connect(p->m_library, &MusicLibrary::tracksLoaded, this, [this]() { p->scheduleAllRecomputes(); });
    QObject::connect(p->m_library, &MusicLibrary::tracksSorted, this, [this]() { p->scheduleAllRecomputes(); });
}

FilterController::~FilterController() = default;

FilterColumnRegistry* FilterController::columnRegistry() const
{
    return p->m_columnRegistry;
}

QString FilterController::defaultPlaylistName()
{
    return tr("Filter Results");
}

FilterWidget* FilterController::createFilter()
{
    auto* widget
        = new FilterWidget(p->m_actionManager, p->m_columnRegistry, p->m_library, p->m_coverRepository, p->m_settings);

    widget->setGroup(p->m_defaultId);
    p->attachWidget(widget, p->m_defaultId);

    QObject::connect(widget, &FilterWidget::doubleClicked, this,
                     [this, widget]() { p->handleAction(widget, widget->doubleClickAction()); });
    QObject::connect(widget, &FilterWidget::middleClicked, this,
                     [this, widget]() { p->handleAction(widget, widget->middleClickAction()); });
    QObject::connect(widget, &FilterWidget::searchTextChanged, this,
                     [this, widget](const QString& search) { p->handleSearchChanged(widget, search); });
    QObject::connect(
        widget, &FilterWidget::requestHeaderMenu, this,
        [widget](AutoHeaderView* header, const QPoint& pos) { filterHeaderContextMenu(widget, header, pos); });
    QObject::connect(widget, &FilterWidget::requestContextMenu, this,
                     [this, widget](const QPoint& pos) { p->filterContextMenu(widget, pos); });
    QObject::connect(widget, &FilterWidget::selectionKeysChanged, this,
                     [this, widget](const std::vector<RowKey>& keys) { p->handleSelectionChanged(widget, keys); });
    QObject::connect(widget, &FilterWidget::filterUpdated, this, [this, widget]() { p->handleFilterUpdated(widget); });
    QObject::connect(widget, &FilterWidget::filterDeleted, this, [this, widget]() { removeFilter(widget); });
    QObject::connect(widget, &FilterWidget::configChanged, this,
                     [this, widget]() { p->updateFilterPlaylistActions(widget); });
    QObject::connect(widget, &FilterWidget::requestEditConnections, this,
                     [this]() { p->m_manager->setupWidgetConnections(); });

    p->updateFilterPlaylistActions(widget);

    if(const auto group = p->widgetGroupId(widget)) {
        p->scheduleRecompute(group.value());
    }

    return widget;
}

bool FilterController::haveUngroupedFilters() const
{
    return !p->m_ungrouped.empty();
}

bool FilterController::filterIsUngrouped(const Id& id) const
{
    return std::ranges::any_of(p->m_ungrouped,
                               [&id](const FilterWidget* widget) { return widget && widget->id() == id; });
}

FilterGroups FilterController::filterGroups() const
{
    return p->publicGroups();
}

std::optional<FilterGroup> FilterController::groupById(const Id& id) const
{
    return p->publicGroup(id);
}

UngroupedFilters FilterController::ungroupedFilters() const
{
    UngroupedFilters ungrouped;

    for(FilterWidget* widget : p->m_ungrouped) {
        if(widget) {
            ungrouped.emplace(widget->id(), widget);
        }
    }

    return ungrouped;
}

void FilterController::addFilterToGroup(FilterWidget* widget, const Id& groupId)
{
    p->attachWidget(widget, groupId);

    if(groupId.isValid()) {
        widget->setGroup(groupId);
    }
    if(const auto group = p->widgetGroupId(widget)) {
        p->scheduleRecompute(group.value());
    }
}

bool FilterController::removeFilter(FilterWidget* widget)
{
    const auto groupId = p->detachWidget(widget);
    if(!groupId) {
        return false;
    }

    if(p->m_groups.contains(groupId.value())) {
        p->scheduleRecompute(groupId.value());
    }
    return true;
}
} // namespace Fooyin::Filters

#include "moc_filtercontroller.cpp"
