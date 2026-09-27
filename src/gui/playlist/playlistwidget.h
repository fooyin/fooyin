/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <luket@pm.me>
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

#include "editableplaylistsessionhost.h"
#include "internalguisettings.h"
#include "playlistcolumn.h"
#include "playlistcontroller.h"
#include "playlistmodel.h"
#include "playlistpreset.h"
#include "sortactionhandler.h"

#include <gui/fywidget.h>
#include <gui/trackdisplay.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgets/autoheaderview.h>

#include <core/library/sortingregistry.h>

#include <QByteArray>
#include <QModelIndexList>
#include <QString>

#include <memory>
#include <optional>

class QVBoxLayout;
class QAction;
class QMenu;

namespace Fooyin {
class ActionManager;
class Application;
class CoverProvider;
class GuiStyleProvider;
class MusicLibrary;
class PlaylistColumnRegistry;
class PlaylistDelegate;
class PlaylistSearchController;
class PlaylistInteractor;
class PlaylistView;
class SettingsManager;
class SettingsDialogController;
class SignalThrottler;
class SortingRegistry;
class WidgetContext;
class PlaylistWidgetSession;

struct PlaylistWidgetLayoutState
{
    PlaylistPreset currentPreset;
    bool singleMode{false};
    PlaylistColumnList columns;
    std::vector<Qt::Alignment> columnAlignments;
    QByteArray headerState;
};

class PlaylistWidget : public FyWidget
{
    Q_OBJECT

public:
    struct ConfigData
    {
        bool showHeader{true};
        bool showScrollBar{true};
        bool alternatingRows{true};
        int imagePadding{5};
        int imagePaddingTop{0};
        int artworkCornerRadius{0};
        int backgroundImageMode{0};
        QString backgroundCustomImage;
        int backgroundCoverType{0};
        int backgroundScaling{0};
        int backgroundPosition{4};
        int backgroundMaxSize{0};
        int backgroundBlur{0};
        int backgroundOpacity{40};
        int backgroundFadeDuration{0};
        TrackDisplayPreference backgroundTrackPreference{TrackDisplayPreference::PlayingTrack};
        TrackAction doubleClickAction{TrackAction::Play};
        TrackAction middleClickAction{TrackAction::None};
        bool startPlaybackOnSend{false};
    };

    struct ModeCapabilities
    {
        bool editablePlaylist{false};
        bool playlistBackedSelection{false};
    };

    struct ContextMenuState
    {
        bool hasSelection{false};
        bool showStopAfter{false};
        bool showEditablePlaylistActions{false};
        bool showSortMenu{false};
        bool showClipboard{false};
        bool usePlaylistQueueCommands{false};
        bool disableSortMenu{false};
    };

    struct ContextMenuRequest
    {
        qsizetype selectedCount{0};
    };

    static PlaylistWidget* createMainPlaylist(ActionManager* actionManager, PlaylistInteractor* playlistInteractor,
                                              TrackSelectionController* selectionController,
                                              CoverProvider* coverProvider, Application* core,
                                              GuiStyleProvider* styleProvider, QWidget* parent = nullptr);
    static PlaylistWidget* createDetachedPlaylistSearch(ActionManager* actionManager,
                                                        PlaylistInteractor* playlistInteractor,
                                                        TrackSelectionController* selectionController,
                                                        CoverProvider* coverProvider, Application* core,
                                                        GuiStyleProvider* styleProvider, QWidget* parent = nullptr);
    static PlaylistWidget* createDetachedLibrarySearch(ActionManager* actionManager,
                                                       PlaylistInteractor* playlistInteractor,
                                                       TrackSelectionController* selectionController,
                                                       CoverProvider* coverProvider, Application* core,
                                                       GuiStyleProvider* styleProvider, QWidget* parent = nullptr);
    static PlaylistWidget* createDetachedTracks(ActionManager* actionManager, PlaylistInteractor* playlistInteractor,
                                                TrackSelectionController* selectionController,
                                                CoverProvider* coverProvider, Application* core,
                                                GuiStyleProvider* styleProvider, const TrackList& tracks,
                                                QWidget* parent = nullptr);

    ~PlaylistWidget() override;

    [[nodiscard]] PlaylistView* view() const;
    [[nodiscard]] PlaylistModel* model() const;
    [[nodiscard]] int trackCount() const;
    void setHeaderText(QString text);

    //! Replaces the source tracks when this widget uses a detached tracklist session.
    void setTracks(const TrackList& tracks);
    void startPlayback();

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;
    void finalise() override;
    void layoutEditingMenu(QMenu* menu) override;
    void setConfigDialogTitle(QString title);
    void openConfigDialog(const QString& title);

    [[nodiscard]] ConfigData factoryConfig() const;
    [[nodiscard]] ConfigData defaultConfig() const;
    [[nodiscard]] const ConfigData& currentConfig() const;
    void saveDefaults(const ConfigData& config) const;
    void clearSavedDefaults() const;
    void applyConfig(const ConfigData& config);

    void searchEvent(const SearchRequest& request) override;
    bool openIntegratedSearch();

    void resetModel();
    void resetModelThrottled() const;
    void changePreset(const PlaylistPreset& preset);
    void setReadOnly(bool readOnly, bool allowSorting);
    void doubleClicked(const QModelIndex& index);
    void middleClicked(const QModelIndex& index);
    void resetSort(bool force = false);
    void setHeaderVisible(bool visible);
    void setScrollbarVisible(bool visible);
    void setAlternatingRowColors(bool enabled);
    void selectAll();

    void handlePresetChanged(const PlaylistPreset& preset);
    void changePlaylistLayout(Playlist* previousPlaylist, const Playlist* playlist);
    bool followCurrentTrack();
    void sessionHandleRestoredState();
    [[nodiscard]] bool hasDelayedStateLoad() const;
    void clearDelayedStateLoad();
    void setDelayedStateLoad(QMetaObject::Connection connection);

    [[nodiscard]] const PlaylistWidgetLayoutState& layoutState() const;
    [[nodiscard]] ActionManager* actionManager() const;
    [[nodiscard]] PresetRegistry* presetRegistry() const;
    [[nodiscard]] PlaylistController* playlistController() const;
    [[nodiscard]] PlayerController* playerController() const;
    [[nodiscard]] MusicLibrary* musicLibrary() const;
    [[nodiscard]] PlaylistInteractor* playlistInteractor() const;
    [[nodiscard]] SettingsManager* settingsManager() const;
    [[nodiscard]] SignalThrottler* resetThrottler() const;
    [[nodiscard]] LibraryManager* libraryManager() const;
    [[nodiscard]] TrackSelectionController* selectionController() const;
    [[nodiscard]] WidgetContext* playlistContext() const;
    [[nodiscard]] PlaylistModel* playlistModel() const;
    [[nodiscard]] PlaylistView* playlistView() const;

    [[nodiscard]] PlaylistWidgetSessionHost& sessionHost();
    [[nodiscard]] EditablePlaylistSessionHost& editableSessionHost();

Q_SIGNALS:
    void configChanged();
    void headerMenuAboutToShow(QMenu* menu);

protected:
    void openConfigDialog() override;

    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    PlaylistWidget(ActionManager* actionManager, PlaylistInteractor* playlistInteractor, CoverProvider* coverProvider,
                   Application* core, GuiStyleProvider* styleProvider, TrackSelectionController* selectionController,
                   std::unique_ptr<PlaylistWidgetSession> session, QWidget* parent);

    void populateTrackContextMenu(QMenu* menu, const ContextMenuRequest& request);
    void showHeaderMenu(const QPoint& pos);
    void addSortMenu(QMenu* parent, bool disabled);
    void refreshSortActions();
    void updateSortActionState();
    void addClipboardMenu(QMenu* parent, bool hasSelection) const;
    void addSingleModeAction(QMenu* parent);
    void addCustomLayoutAction(QMenu* parent);
    void addPresetMenu(QMenu* parent);
    void addColumnsMenu(QMenu* parent);
    void addSettingsAction(QMenu* menu);
    void applySessionTexts();
    void refreshViewStyle();
    void updateMetadataEditTriggers(bool readOnly);
    void handleColumnChanged(const PlaylistColumn& column);
    void handleColumnRemoved(int id);
    void resetColumnsToDefault();
    void setColumnVisible(int columnId, bool visible);
    void setSingleMode(bool enabled);
    void ensureDefaultColumns(PlaylistWidgetLayoutState& state) const;
    void applyDefaultHeaderConfiguration();
    [[nodiscard]] PlaylistWidgetLayoutState captureLayoutState() const;
    [[nodiscard]] QString serialiseLayoutState(const PlaylistWidgetLayoutState& state) const;
    [[nodiscard]] std::optional<PlaylistWidgetLayoutState> deserialiseLayoutState(const QString& encoded) const;
    void applyLayoutState(const PlaylistWidgetLayoutState& state);
    [[nodiscard]] bool columnAvailable(const PlaylistColumn& column) const;
    void saveRememberedLayout(Playlist* playlist);
    [[nodiscard]] bool remembersLayout(const Playlist* playlist) const;
    void updateSpans();
    void applyBackgroundSettings();
    [[nodiscard]] ConfigData configFromLayout(const QJsonObject& layout) const;
    void saveConfigToLayout(const ConfigData& config, QJsonObject& layout) const;
    void reloadBackgroundCover();
    void updateVisibleCoverPins();
    void executeClickAction(TrackAction action);

    void handleMetadataWriteRequested(const TrackList& tracks);
    void handleBulkWriteRequested(const TrackList& tracks);

    void setupConnections();
    void setupActions();

    ActionManager* m_actionManager;
    PlaylistInteractor* m_playlistInteractor;
    PlaylistController* m_playlistController;
    CoverProvider* m_coverProvider;
    PlayerController* m_playerController;
    LibraryManager* m_libraryManager;
    TrackSelectionController* m_selectionController;
    MusicLibrary* m_library;
    SettingsManager* m_settings;
    GuiStyleProvider* m_styleProvider;
    SettingsDialogController* m_settingsDialog;

    std::unique_ptr<PlaylistWidgetSession> m_session;
    QMetaObject::Connection m_delayedStateLoad;
    SignalThrottler* m_resetThrottler;

    PlaylistColumnRegistry* m_columnRegistry;
    PresetRegistry* m_presetRegistry;
    SortingRegistry* m_sortRegistry;

    QVBoxLayout* m_layout;
    PlaylistModel* m_model;
    PlaylistDelegate* m_delgate;
    PlaylistView* m_playlistView;
    AutoHeaderView* m_header;
    PlaylistWidgetLayoutState m_layoutState;
    PlaylistWidgetLayoutState m_defaultLayoutState;
    ConfigData m_config;
    QString m_configDialogTitle;
    QString m_loadedPlaylistLayout;
    // Until playlist settings are per-playlist
    bool m_useGlobalPresetState;

    WidgetContext* m_playlistContext;
    TrackAction m_doubleClickAction;
    TrackAction m_middleClickAction;
    bool m_startPlaybackOnSend;
    QAction* m_playAction;
    std::unique_ptr<SortActionHandler> m_sortActions;

    int m_bgCoverRequestId;
    PlaylistBgImage m_bgImageMode;
    Track::Cover m_bgCoverType;
    TrackDisplayPreference m_bgTrackPreference;
    QString m_bgCustomImage;
    Track m_bgCoverTrack;

    std::unique_ptr<EditablePlaylistSessionHost> m_host;
    PlaylistSearchController* m_searchController;
};
} // namespace Fooyin
