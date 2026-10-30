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

#pragma once

#include "fygui_export.h"

#include "filtercontroller.h"
#include "filterexpansionpopulator.h"
#include "filterfwd.h"
#include "filterrows.h"

#include <core/track.h>
#include <gui/coverartworktypes.h>
#include <gui/fywidget.h>
#include <gui/widgets/backgroundconfig.h>
#include <gui/widgets/expandedtreeview.h>

#include <QBasicTimer>
#include <QMargins>
#include <QPointer>

#include <optional>

class QTimerEvent;

namespace Fooyin {
class AudioLoader;
class AutoHeaderView;
class ActionManager;
class CoverProvider;
class CoverRepository;
class BackgroundImageSource;
class LibraryManager;
class PlayerController;
class TrackSelectionController;
class SettingsManager;
class WidgetContext;
enum class TrackAction;
struct CorePluginContext;

namespace Filters {
class FilterColumnRegistry;
class FilterDelegate;
class FilterExpansionWidget;
class FilterModel;
class FilterSortModel;
class FilterView;

enum class FilterSource : uint8_t
{
    Library = 0,
    CurrentPlaylist,
};

enum class FilterScrollPolicy : uint8_t
{
    Preserve = 0,
    ResetIfChanged,
};

class FYGUI_EXPORT FilterWidget : public FyWidget
{
    Q_OBJECT

public:
    struct FilterViewState
    {
        FilterRowList rows;
        std::vector<RowKey> selectedKeys;
        QString searchText;
        bool selectionRestorePending{false};
        FilterScrollPolicy scrollPolicy{FilterScrollPolicy::Preserve};
    };

    explicit FilterWidget(ActionManager* actionManager, FilterColumnRegistry* columnRegistry,
                          const CorePluginContext& core, CoverRepository* coverRepository,
                          TrackSelectionController* selection, QWidget* parent = nullptr);
    ~FilterWidget() override;

    [[nodiscard]] Id group() const;
    [[nodiscard]] int index() const;
    [[nodiscard]] const FilterColumnList& columns() const;
    [[nodiscard]] bool multipleColumns() const;
    [[nodiscard]] bool isActive() const;
    [[nodiscard]] std::vector<RowKey> selectedKeys() const;
    [[nodiscard]] bool selectionRestorePending() const;
    [[nodiscard]] QString searchText() const;
    [[nodiscard]] WidgetContext* widgetContext() const;
    [[nodiscard]] TrackAction doubleClickAction() const;
    [[nodiscard]] TrackAction middleClickAction() const;
    [[nodiscard]] bool sendPlayback() const;
    [[nodiscard]] FilterSource source() const;
    [[nodiscard]] bool playlistEnabled() const;
    [[nodiscard]] bool autoSwitch() const;
    [[nodiscard]] bool preservePlaybackPlaylist() const;
    [[nodiscard]] QString playlistName() const;
    [[nodiscard]] bool hasSelection() const;

    void setGroup(const Id& group);
    void setIndex(int index);
    void setViewState(const FilterViewState& state);

    [[nodiscard]] ExpansionRequest expansionPreviewRequest(const ExpansionScripts& scripts,
                                                           std::shared_ptr<const TrackList> tracks) const;
    [[nodiscard]] ScriptContextService* expansionScriptContext() const;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void saveCopyLayoutData(QJsonObject& layout, LayoutCopyContext& context, bool isRoot) override;
    void loadLayoutData(const QJsonObject& layout) override;
    void finalise() override;

    void searchEvent(const SearchRequest& request) override;

    struct ConfigData
    {
        int doubleClickAction{1};
        int middleClickAction{0};
        // -1 inherits the filter action
        int expansionDoubleClickAction{-1};
        int expansionMiddleClickAction{-1};
        bool sendPlayback{true};
        FilterSource source{FilterSource::Library};
        bool playlistEnabled{true};
        bool autoSwitch{true};
        bool preservePlaybackPlaylist{true};
        QString playlistName{FilterController::defaultPlaylistName()};
        bool restoreState{true};
        ExpandedTreeView::ViewMode viewMode{ExpandedTreeView::ViewMode::Tree};
        ExpandedTreeView::CaptionDisplay captionDisplay{ExpandedTreeView::CaptionDisplay::Bottom};
        Track::Cover coverType{Track::Cover::Front};
        std::optional<ArtworkSourcePreference> coverSource;
        bool showSummary{true};
        bool showHeader{true};
        bool showScrollbar{true};
        bool alternatingRows{false};
        int rowHeight{0};
        QSize iconSize{100, 100};
        bool autoFitArtwork{false};
        int iconHorizontalGap{-1};
        int iconVerticalGap{10};
        int artworkCornerRadius{0};
        QMargins artworkMargins;
        BackgroundConfig background;
        bool expandGroups{false};
        bool expansionAnimated{true};
        bool expansionAlternatingRows{false};
        bool expandedArtwork{true};
        int expansionMinimumRows{4};
        int expansionMaximumColumns{0};
        bool expansionStretchColumns{false};
        int expansionMinimumColumnWidth{100};
        bool expansionArtworkOnLeft{true};
        int expansionArtworkSize{100};
        bool expansionShowCloseButton{true};
        bool expansionArtworkReflection{false};
        int expansionArtworkReflectionHeight{33};
        int expansionArtworkReflectionOpacity{35};
        int expansionArtworkReflectionGap{3};
        bool expansionArtworkAutoMargins{false};
        QMargins expansionArtworkMargins{10, 10, 10, 10};
        ExpansionScripts expansionScripts;
        BackgroundConfig expansionBackground;
        QString expansionBackgroundColour;
    };

    [[nodiscard]] ConfigData factoryConfig() const;
    [[nodiscard]] ConfigData defaultConfig() const;
    [[nodiscard]] const ConfigData& currentConfig() const;
    void saveDefaults(const ConfigData& config) const;
    void clearSavedDefaults() const;
    void applyConfig(const ConfigData& config);
    void openConfigDialog() override;

    void addFilterHeaderMenu(QMenu* menu, const QPoint& pos, bool includeWidgetActions = true);

Q_SIGNALS:
    void doubleClicked();
    void middleClicked();
    void actionRequested(Fooyin::TrackAction action);

    void filterDeleted();
    void filterUpdated();
    void selectionKeysChanged(const std::vector<Fooyin::Filters::RowKey>& keys);
    void configChanged();
    void expansionLayoutChanged();
    void expansionScriptErrorsChanged(const QStringList& errors);
    void requestHeaderMenu(Fooyin::AutoHeaderView* header, const QPoint& pos);
    void requestContextMenu(const QPoint& pos, Fooyin::WidgetContext* context, bool hasSelection);
    void requestEditConnections();
    void searchTextChanged(const QString& search);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void timerEvent(QTimerEvent* event) override;

private:
    void setupConnections();
    void traceSortState(const char* event, bool includeRows = false) const;

    void handleSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    void updateViewMode(ExpandedTreeView::ViewMode mode);
    void updateCaptions(ExpandedTreeView::CaptionDisplay captions);
    void updateAppearance();
    void scheduleVisibleCoverPinUpdate(int delay = 0);
    void updateVisibleCoverPins();

    void expandSelection();
    void scheduleExpansionUpdate();
    void updateExpansion();
    void submitExpansion(ExpansionRequest request);
    [[nodiscard]] ExpansionRequest expansionRequest(std::shared_ptr<const TrackList> tracks,
                                                    const ExpansionScripts& scripts) const;
    void applyExpansionContent(const ExpansionResult& result);
    void refreshExpansionScripts(const ScriptChanges& changes);
    void closeExpansion(bool restoreFocus = false);
    void restoreScrollPosition();

    void addDisplayMenu(QMenu* menu);
    void filterHeaderMenu(const QPoint& pos);

    [[nodiscard]] bool hasColumn(int id) const;
    void columnChanged(const FilterColumn& changedColumn);
    void columnRemoved(int id);

    [[nodiscard]] ConfigData configFromLayout(const QJsonObject& layout) const;
    static void saveConfigToLayout(const ConfigData& config, QJsonObject& layout);

    ActionManager* m_actionManager;
    FilterColumnRegistry* m_columnRegistry;
    LibraryManager* m_libraryManager;
    SettingsManager* m_settings;

    FilterView* m_view;
    FilterDelegate* m_delegate;
    AutoHeaderView* m_header;
    FilterModel* m_model;
    FilterSortModel* m_sortProxy;
    BackgroundImageSource* m_backgroundSource;

    Id m_group;
    int m_index;
    FilterColumnList m_columns;
    bool m_multipleColumns;

    WidgetContext* m_widgetContext;

    QString m_searchStr;
    std::vector<RowKey> m_restoredSelectedKeys;
    int m_pendingScrollPosition;
    bool m_applyingViewState;
    bool m_selectionRestorePending;

    QByteArray m_headerState;

    ConfigData m_config;
    QBasicTimer m_visibleCoverPinUpdateTimer;

    bool m_expansionUpdateQueued;
    QPointer<FilterExpansionWidget> m_expansion;
    TrackSelectionController* m_trackSelection;
    PlayerController* m_player;
    RowKey m_expandedKey;
    RowKey m_displayedKey;
    FilterColumnList m_expandedColumns;
    uint64_t m_expansionRequest;
    ScriptContextService* m_scriptContext;
    ScriptSubscription* m_expansionSubscription;
    std::shared_ptr<const TrackList> m_expansionTracks;
    ExpansionDependencies m_expansionDependencies;
    ScriptContextData m_expansionContext;
    std::vector<QString> m_expansionColumnValues;
    uint64_t m_displayedGeneration;
};
} // namespace Filters
} // namespace Fooyin
