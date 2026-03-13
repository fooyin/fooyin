/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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
#include "playlistcolumn.h"
#include "playlistcontroller.h"
#include "playlistmodel.h"
#include "playlistpreset.h"
#include "presetregistry.h"

#include <gui/fywidget.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgets/autoheaderview.h>

#include <core/library/sortingregistry.h>
#include <core/library/tracksort.h>
#include <core/player/playbackqueue.h>

#include <QByteArray>
#include <QModelIndexList>
#include <QString>

class QHBoxLayout;
class QAction;
class QMenu;

namespace Fooyin {
class ActionManager;
class Application;
class CoverProvider;
class MusicLibrary;
class PlaylistColumnRegistry;
class PlaylistDelegate;
class PlaylistInteractor;
class PlaylistView;
class SettingsManager;
class SettingsDialogController;
class SignalThrottler;
class SortingRegistry;
class StarDelegate;
class WidgetContext;
class PlaylistWidgetSession;

struct PlaylistWidgetLayoutState
{
    PlaylistPreset currentPreset;
    bool singleMode{false};
    PlaylistColumnList columns;
    QByteArray headerState;
};

class PlaylistWidget : public FyWidget
{
    Q_OBJECT

public:
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
        bool showClipboard{false};
        bool usePlaylistQueueCommands{false};
        bool disableSortMenu{false};
    };

    static PlaylistWidget* createMainPlaylist(ActionManager* actionManager, PlaylistInteractor* playlistInteractor,
                                              CoverProvider* coverProvider, Application* core,
                                              QWidget* parent = nullptr);
    static PlaylistWidget* createDetachedPlaylistSearch(ActionManager* actionManager,
                                                        PlaylistInteractor* playlistInteractor,
                                                        CoverProvider* coverProvider, Application* core,
                                                        QWidget* parent = nullptr);
    static PlaylistWidget* createDetachedLibrarySearch(ActionManager* actionManager,
                                                       PlaylistInteractor* playlistInteractor,
                                                       CoverProvider* coverProvider, Application* core,
                                                       QWidget* parent = nullptr);

    ~PlaylistWidget() override;

    [[nodiscard]] PlaylistView* view() const;
    [[nodiscard]] PlaylistModel* model() const;
    [[nodiscard]] int trackCount() const;

    void startPlayback();

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;
    void finalise() override;

    void searchEvent(const QString& search) override;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    PlaylistWidget(ActionManager* actionManager, PlaylistInteractor* playlistInteractor, CoverProvider* coverProvider,
                   Application* core, std::unique_ptr<PlaylistWidgetSession> session, QWidget* parent);
    void populateTrackContextMenu(QMenu* menu, const QModelIndexList& selected);
    void showHeaderMenu(const QPoint& pos);
    void addSortMenu(QMenu* parent, bool disabled);
    void addClipboardMenu(QMenu* parent, bool hasSelection) const;
    void addPresetMenu(QMenu* parent);
    void addColumnsMenu(QMenu* parent);
    void applyInitialViewSettings();
    void applySessionTexts();
    void refreshViewStyle();
    void handleColumnChanged(const PlaylistColumn& column);
    void handleColumnRemoved(int id);
    void changePreset(const PlaylistPreset& preset);
    void resetColumnsToDefault();
    void setColumnVisible(int columnId, bool visible);
    void setSingleMode(bool enabled);
    void updateSpans();

public:
    void setupConnections();
    void setupActions();
    void resetModel();
    void resetModelThrottled() const;
    void setReadOnly(bool readOnly, bool allowSorting);
    void doubleClicked(const QModelIndex& index);
    void middleClicked(const QModelIndex& index);
    void resetSort(bool force = false);
    void setHeaderVisible(bool visible);
    void setScrollbarVisible(bool visible);
    void selectAll();

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

    void handlePresetChanged(const PlaylistPreset& preset);
    void setMiddleClickAction(TrackAction action);
    void followCurrentTrack();
    void sessionHandleRestoredState();
    [[nodiscard]] bool hasDelayedStateLoad() const;
    void clearDelayedStateLoad();
    void setDelayedStateLoad(QMetaObject::Connection connection);

    [[nodiscard]] PlaylistWidgetSessionHost& sessionHost();
    [[nodiscard]] EditablePlaylistSessionHost& editableSessionHost();

private:
    ActionManager* m_actionManager;
    PlaylistInteractor* m_playlistInteractor;
    PlaylistController* m_playlistController;
    PlayerController* m_playerController;
    LibraryManager* m_libraryManager;
    TrackSelectionController* m_selectionController;
    MusicLibrary* m_library;
    SettingsManager* m_settings;
    SettingsDialogController* m_settingsDialog;

    std::unique_ptr<PlaylistWidgetSession> m_session;
    QMetaObject::Connection m_delayedStateLoad;
    SignalThrottler* m_resetThrottler;

    PlaylistColumnRegistry* m_columnRegistry;
    PresetRegistry* m_presetRegistry;
    SortingRegistry* m_sortRegistry;

    QHBoxLayout* m_layout;
    PlaylistModel* m_model;
    PlaylistDelegate* m_delgate;
    StarDelegate* m_starDelegate;
    PlaylistView* m_playlistView;
    AutoHeaderView* m_header;
    PlaylistWidgetLayoutState m_layoutState;

    WidgetContext* m_playlistContext;
    TrackAction m_middleClickAction;

    QAction* m_playAction;
    std::unique_ptr<EditablePlaylistSessionHost> m_host;
};
} // namespace Fooyin
