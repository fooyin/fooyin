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

#include <gui/fywidget.h>

class QCloseEvent;
class QPoint;
class QAbstractItemModel;
class QJsonObject;
class QSortFilterProxyModel;
class QShowEvent;
class QAction;

namespace Fooyin {
class ActionManager;
class Command;
class ExpandedTreeView;
class Playlist;
class PlaylistController;
class PlaylistInteractor;
class PlaylistManagerModel;
class SettingsManager;
class TrackSelectionController;
class WidgetContext;

class PlaylistManagerWidget : public FyWidget
{
    Q_OBJECT

public:
    explicit PlaylistManagerWidget(ActionManager* actionManager, PlaylistController* playlistController,
                                   PlaylistInteractor* playlistInteractor,
                                   TrackSelectionController* selectionController, SettingsManager* settings,
                                   QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;
    void finalise() override;

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    [[nodiscard]] QByteArray saveHeaderState() const;
    void resetToPlaylistIndexOrder() const;

    void addPlaylistContentsMenu(QMenu* menu, const Playlist* playlist) const;
    void setupActions();
    void updateActionState() const;

    void activatePlaylist(const QModelIndex& proxyIndex) const;
    void editAutoPlaylist(Playlist* playlist) const;
    void removePlaylist(const Playlist* playlist);
    void activateCurrentPlaylist() const;
    void editCurrentAutoPlaylist();
    void removeCurrentPlaylist();

    void showPlaylistContextMenu(const QPoint& pos);
    void showHeaderContextMenu(const QPoint& pos);
    void showRenameEditor(const QModelIndex& proxyIndex) const;
    void showRenameEditor() const;

    void selectCurrentPlaylist();

    void saveTopLevelState();
    void loadTopLevelState();

    [[nodiscard]] bool isWindowWidget() const;

    ActionManager* m_actionManager;
    PlaylistController* m_playlistController;
    TrackSelectionController* m_selectionController;
    SettingsManager* m_settings;

    PlaylistManagerModel* m_model;
    QSortFilterProxyModel* m_proxyModel;
    ExpandedTreeView* m_view;
    WidgetContext* m_context;

    QAction* m_activateAction;
    QAction* m_editAutoPlaylistAction;
    Command* m_editAutoPlaylistCmd;
    QAction* m_renameAction;
    Command* m_renameCmd;
    QAction* m_removeAction;
    Command* m_removeCmd;
    QAction* m_newPlaylistAction;
    Command* m_newPlaylistCmd;
    QAction* m_newAutoPlaylistAction;
    Command* m_newAutoPlaylistCmd;

    bool m_activateOnSelection;
    bool m_selectingPlaylist;
    bool m_topLevelStateLoaded;
};
} // namespace Fooyin
