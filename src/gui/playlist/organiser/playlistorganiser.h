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

#include <gui/fywidget.h>

class QTreeView;

namespace Fooyin {
class ActionManager;
class Command;
class PlaylistController;
class PlaylistOrganiserModel;
class SettingsManager;
class WidgetContext;

class PlaylistOrganiser : public FyWidget
{
    Q_OBJECT

public:
    explicit PlaylistOrganiser(ActionManager* actionManager, PlaylistController* playlistController,
                               SettingsManager* settings, QWidget* parent = nullptr);
    ~PlaylistOrganiser() override;

    QString name() const override;
    QString layoutName() const override;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void selectionChanged();
    void selectCurrentPlaylist();
    void createGroup(const QModelIndex& index) const;
    void createPlaylist(const QModelIndex& index);

    ActionManager* m_actionManager;
    SettingsManager* m_settings;
    PlaylistController* m_playlistController;

    QTreeView* m_organiserTree;
    PlaylistOrganiserModel* m_model;

    WidgetContext* m_context;

    QAction* m_removePlaylist;
    Command* m_removeCmd;
    QAction* m_renamePlaylist;
    Command* m_renameCmd;
    QAction* m_newGroup;
    Command* m_newGroupCmd;
    QAction* m_newPlaylist;
    Command* m_newPlaylistCmd;

    Id m_currentPlaylistId;
    bool m_creatingPlaylist{false};
};
} // namespace Fooyin
