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

#pragma once

#include "gui/fywidget.h"
#include "librarytreegroup.h"

#include <core/track.h>

#include <QByteArray>
#include <QModelIndex>
#include <QSize>

#include <map>
#include <vector>

class QAction;
class QItemSelection;
class QJsonObject;
class QMenu;
class QModelIndex;
class QPoint;
class QVBoxLayout;

namespace Fooyin {
class ActionManager;
class Application;
class LibraryTreeController;
class LibraryTreeGroupRegistry;
class LibraryTreeModel;
class LibraryTreeSortModel;
class LibraryTreeView;
class MusicLibrary;
class PlayerController;
class Playlist;
class PlaylistController;
class PlaylistHandler;
struct PlaylistTrack;
class SettingsManager;
class SignalThrottler;
class TrackSelectionController;
class WidgetContext;
enum class TrackAction;

class LibraryTreeWidget : public FyWidget
{
    Q_OBJECT

public:
    LibraryTreeWidget(ActionManager* actionManager, PlaylistController* playlistController,
                      LibraryTreeController* controller, Application* core, QWidget* parent = nullptr);
    ~LibraryTreeWidget() override;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

    void searchEvent(const QString& search) override;
    struct ConfigData
    {
        int doubleClickAction{0};
        int middleClickAction{0};
        bool sendPlayback{true};
        bool playlistEnabled{false};
        bool autoSwitch{true};
        bool keepAlive{false};
        QString playlistName;
        bool restoreState{true};
        bool animated{true};
        bool showHeader{true};
        bool showScrollbar{true};
        bool alternatingRows{false};
        int rowHeight{0};
        QSize iconSize{36, 36};
    };

    [[nodiscard]] ConfigData defaultConfig() const;
    [[nodiscard]] const ConfigData& currentConfig() const;
    void saveDefaults(const ConfigData& config) const;
    void applyConfig(const ConfigData& config);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void setupConnections();
    void reset() const;

    void changeGrouping(const LibraryTreeGrouping& newGrouping);

    void activePlaylistChanged(Playlist* playlist) const;
    void playlistTrackChanged(const PlaylistTrack& track);

    void addGroupMenu(QMenu* parent);
    void addOpenMenu(QMenu* menu);

    void setScrollbarEnabled(bool enabled) const;
    void setupHeaderContextMenu(const QPoint& pos);
    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) const;
    void queueSelectedTracks(bool next) const;
    void dequeueSelectedTracks() const;

    void searchChanged(const QString& search);

    void handlePlayback(const QModelIndexList& indexes, int row = 0);
    void handlePlayTrack();
    void handleDoubleClick(const QModelIndex& index);
    void handleMiddleClick(const QModelIndex& index) const;

    void handleTracksAdded(const TrackList& tracks);
    void handleTracksUpdated(const TrackList& tracks);

    void restoreSelection(const QStringList& expandedTitles, const QStringList& selectedTitles);
    [[nodiscard]] QByteArray saveState() const;
    void restoreIndexState(const QByteArray& topKey, const std::vector<QByteArray>& keys, int currentIndex = 0);
    void restoreState(const QByteArray& state);

    [[nodiscard]] ConfigData configFromLayout(const QJsonObject& layout) const;
    static void saveConfigToLayout(const ConfigData& config, QJsonObject& layout);
    void openConfigDialog() override;

    ActionManager* m_actionManager;
    MusicLibrary* m_library;
    PlaylistHandler* m_playlistHandler;
    PlayerController* m_playerController;
    LibraryTreeGroupRegistry* m_groupsRegistry;
    TrackSelectionController* m_trackSelection;
    SettingsManager* m_settings;

    SignalThrottler* m_resetThrottler;
    LibraryTreeGrouping m_grouping;

    QVBoxLayout* m_layout;
    LibraryTreeView* m_libraryTree;
    LibraryTreeModel* m_model;
    LibraryTreeSortModel* m_sortProxy;

    WidgetContext* m_widgetContext;

    QAction* m_addToQueueAction;
    QAction* m_queueNextAction;
    QAction* m_removeFromQueueAction;
    QAction* m_playAction;

    TrackAction m_doubleClickAction;
    TrackAction m_middleClickAction;

    QString m_currentSearch;
    TrackList m_filteredTracks;

    bool m_updating;
    QByteArray m_pendingState;
    ConfigData m_config;

    Playlist* m_playlist;
    std::map<int, QString> m_playlistGroups;
};
} // namespace Fooyin
