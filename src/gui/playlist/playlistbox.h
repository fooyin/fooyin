/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <QPointer>

class QAction;
class QComboBox;
class QPoint;

namespace Fooyin {
class ActionManager;
class Command;
class Playlist;
class PlaylistController;
class PlaylistHandler;
class PopupLineEdit;
class WidgetContext;

class PlaylistBox : public FyWidget
{
    Q_OBJECT

public:
    explicit PlaylistBox(ActionManager* actionManager, PlaylistController* playlistController,
                         QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

    void setupBox();

    void addPlaylist(const Playlist* playlist);
    void removePlaylist(const Playlist* playlist);
    void playlistRenamed(const Playlist* playlist) const;
    void currentPlaylistChanged(const Playlist* playlist) const;
    void changePlaylist(int index);

    void showContextMenu(const QPoint& pos);
    void showRenameEditor();
    void closeRenameEditor();
    void cancelRenameEditor();

    [[nodiscard]] Playlist* currentPlaylist() const;

private:
    void setupActions();
    void updateActionState();
    void editCurrentAutoPlaylist();
    void removeCurrentPlaylist();

    ActionManager* m_actionManager;
    PlaylistController* m_playlistController;
    PlaylistHandler* m_playlistHandler;

    QComboBox* m_playlistBox;

    WidgetContext* m_context;
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

    QPointer<PopupLineEdit> m_lineEdit;
    bool m_renameCancelled;
};
} // namespace Fooyin
