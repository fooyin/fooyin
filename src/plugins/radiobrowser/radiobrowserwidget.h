/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "radiodiscovery.h"
#include "radiostation.h"
#include "radiostationview.h"

#include <gui/fywidget.h>
#include <gui/trackselectioncontroller.h>

#include <QPointer>

#include <optional>
#include <unordered_map>

class QPoint;
class QJsonObject;
class QAction;
class QModelIndex;
class QMenu;

namespace Fooyin {
class ActionManager;
class Command;
class SignalThrottler;
class SettingsManager;
class TrackSelectionController;
class WidgetContext;

namespace RadioBrowser {
class RadioSearch;
class RadioBrowserController;
class RadioBrowserModel;
class RadioStationDelegate;

class RadioBrowserWidget : public FyWidget
{
    Q_OBJECT

public:
    static constexpr auto ToggleSavedStation = 1000;

    explicit RadioBrowserWidget(RadioBrowserController* controller, ActionManager* actionManager,
                                TrackSelectionController* trackSelection, SettingsManager* settings,
                                bool applyInitialSearch = true, QWidget* parent = nullptr);
    ~RadioBrowserWidget() override;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;
    void finalise() override;
    void searchEvent(const SearchRequest& request) override;
    void browseSavedStations();
    void showFilterBar();
    void hideFilterBar();
    void setFilterBarAllowed(bool allowed);
    void setFilterBarToggleAllowed(bool allowed);
    void setApplySearchOnLoad(bool enabled);
    void connectFilterBar(RadioSearch* filterBar);

    struct ConfigData
    {
        struct ViewConfig
        {
            ExpandedTreeView::ViewMode viewMode{ExpandedTreeView::ViewMode::Tree};
            ExpandedTreeView::CaptionDisplay captions{ExpandedTreeView::CaptionDisplay::Right};
            bool showHeader{true};
            bool showScrollbar{true};
            bool alternatingRows{true};
            bool showIcons{true};
            bool showSavedStationIcons{true};
            bool showToolTips{true};
            bool separateSavedStationsViewState{false};
            int rowHeight{0};
            QSize iconSize{36, 36};
            int iconHorizontalGap{-1};
            int iconVerticalGap{8};
            int iconItemBorderWidth{2};
            bool uniformStationIcons{true};
        };

        ViewConfig view;
        int doubleClickAction{5};
        int middleClickAction{0};
        bool playbackOnSend{true};
        bool hideBroken{true};
    };

    [[nodiscard]] ConfigData factoryConfig() const;
    [[nodiscard]] ConfigData defaultConfig() const;
    [[nodiscard]] const ConfigData& currentConfig() const;
    void saveDefaults(const ConfigData& config) const;
    void clearSavedDefaults() const;
    void applyConfig(const ConfigData& config);
    [[nodiscard]] bool sendClicks() const;
    void setSendClicks(bool enabled);
    [[nodiscard]] bool separateSavedStationsViewStateAllowed() const;
    void setSeparateSavedStationsViewStateAllowed(bool allowed);

protected:
    void showEvent(QShowEvent* event) override;
    void openConfigDialog() override;

private:
    void handleIconSizeChanged(const QSize& size);
    void handleStationsReordered(const RadioStationList& stations);
    void handleStationsChanged(const RadioStationList& stations, bool reset);
    void handleSavedStationsBrowsingChanged(bool browsing);
    void handleCategoriesChanged(RadioCategoryType type, const RadioCategoryList& categories);
    void handleSearchRequestActivated(const RadioSearchRequest& request);
    void handleSortRequested(int column, Qt::SortOrder order);
    void handleSearchFailed();
    void handleStationActionFailed(QObject* context, const RadioStation& station, const QString& error);
    void handleFilterChanged();
    void handleSaveSearchTriggered();
    void updateActionState();

    void scheduleFilterSearch();
    void startInitialSearch();
    bool syncControllerBrowseState();
    void browseInitialSelection();

    void applyFilterSearch();
    void applyFilterSearch(bool updateLatestSearch);

    [[nodiscard]] RadioSearchRequest currentFilterRequest() const;
    void setFilterRequest(const RadioSearchRequest& request);

    void updateSavedSearchState();
    void saveCurrentSearch();
    void showSavedSearchMenu();
    void renameSavedSearch(const RadioSavedSearch& search);
    void removeSavedSearch(const RadioSavedSearch& search);
    [[nodiscard]] QString defaultSearchName(const RadioSearchRequest& request) const;

    void resetFilters();
    void setFilterBarVisible(bool visible);

    void refreshThemeIcons();
    void scheduleVisibleIconRequest();
    void requestVisibleIcons();

    void maybeLoadMoreStations();
    [[nodiscard]] RadioStationList selectedStations() const;
    void updateSelectedTracks();
    void addSelectedStations(bool play);
    void queueSelectedStations();
    void executeResolvedAction(Fooyin::TrackAction action);
    void handleResolvedTracks(int resolveId, QObject* context, const Fooyin::TrackList& tracks);

    void saveSelectedStation();
    void removeSelectedStations();
    void addCustomStation();
    void editSelectedStation();
    void importSavedStations();
    void exportSavedStations();

    void handleDoubleClick(const QModelIndex& index);
    void handleMiddleClick(const QModelIndex& index);
    void executeClickAction(const QModelIndex& index, int actionValue);
    void addTrackAction(QMenu* menu, const char* actionId);
    void showContextMenu(const QPoint& pos);
    void showHeaderContextMenu(const QPoint& pos);
    void addColumnsMenu(QMenu* menu);
    void addDisplayMenu(QMenu* menu);

    void saveCurrentViewState();
    void applyActiveViewState();
    void setViewConfig(const ConfigData::ViewConfig& config);
    void updateIconColumnOrder();
    void disconnectFilterBar();
    void updateFilterBarCategories();
    void updateFilterBarActionState();
    void updateApiSortingState(const RadioSearchRequest& request);

    struct PendingTrackAction
    {
        Fooyin::TrackAction action{Fooyin::TrackAction::None};
        PlaylistAction::ActionOptions options;
    };

    enum class InitialSearchState : uint8_t
    {
        Disabled = 0,
        Pending,
        Complete
    };

    enum class FilterBarMode : uint8_t
    {
        Hidden = 0,
        Fixed,
        Toggleable
    };

    struct ViewState
    {
        ExpandedTreeView::ViewMode viewMode{ExpandedTreeView::ViewMode::Tree};
        ExpandedTreeView::CaptionDisplay captions{ExpandedTreeView::CaptionDisplay::Right};
        QByteArray headerState;
    };

    RadioBrowserController* m_controller;
    ActionManager* m_actionManager;
    TrackSelectionController* m_trackSelection;
    SettingsManager* m_settings;

    WidgetContext* m_context;
    QPointer<RadioSearch> m_filterBar;

    QAction* m_toggleFilterBarAction;
    QAction* m_saveStationsAction;
    QAction* m_removeStationsAction;
    Command* m_saveStationsCmd;
    Command* m_removeStationsCmd;
    SignalThrottler* m_filterThrottler;
    RadioStationView* m_resultsView;
    RadioStationDelegate* m_delegate;
    RadioBrowserModel* m_model;

    ConfigData m_config;
    ConfigData::ViewConfig m_viewConfig;
    ViewState m_browseViewState;
    ViewState m_savedStationsViewState;
    int m_doubleClickAction;
    int m_middleClickAction;
    bool m_playbackOnSend;
    bool m_hideBroken;
    bool m_sendClicks;

    bool m_loadingLayout;
    InitialSearchState m_initialSearchState;
    FilterBarMode m_filterBarMode;
    bool m_separateSavedStationsViewStateAllowed;
    bool m_browsingSavedStations;
    bool m_visibleIconRequestPending;
    RadioSearchRequest m_filterRequest;
    RadioCategoryList m_countryCategories;
    RadioCategoryList m_tagCategories;
    RadioCategoryList m_codecCategories;
    std::unordered_map<int, PendingTrackAction> m_pendingTrackActions;
    std::optional<RadioSavedSearch> m_currentSavedSearch;
};
} // namespace RadioBrowser
} // namespace Fooyin
