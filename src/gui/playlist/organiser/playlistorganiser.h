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

#include <core/track.h>
#include <gui/fywidget.h>

class QTreeView;

namespace Fooyin {
class ActionManager;
class Command;
class PlayerController;
class PlaylistHandler;
class PlaylistInteractor;
class PlaylistOrganiserModel;
class SettingsManager;
class WidgetContext;

class PlaylistOrganiser : public FyWidget
{
    Q_OBJECT

public:
    explicit PlaylistOrganiser(ActionManager* actionManager, PlaylistInteractor* playlistInteractor,
                               SettingsManager* settings, QWidget* parent = nullptr);
    ~PlaylistOrganiser() override;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void selectionChanged();
    void selectCurrentPlaylist();
    void createGroup(const QModelIndex& index) const;
    void createPlaylist(const QModelIndex& index, bool autoPlaylist);
    void editAutoPlaylist(const QModelIndex& index);

    void filesToPlaylist(const QList<QUrl>& urls, const UId& id);
    void filesToGroup(const QList<QUrl>& urls, const QString& group, int index);
    void tracksToPlaylist(const std::vector<int>& trackIds, const UId& id);
    void tracksToGroup(const std::vector<int>& trackIds, const QString& group, int index);

    ActionManager* m_actionManager;
    SettingsManager* m_settings;
    PlaylistInteractor* m_playlistInteractor;
    PlayerController* m_playerController;

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
    QAction* m_newAutoPlaylist;
    Command* m_newAutoPlaylistCmd;
    QAction* m_editAutoPlaylist;
    Command* m_editAutoPlaylistCmd;

    UId m_currentPlaylistId;
    bool m_creatingPlaylist{false};
};
} // namespace Fooyin
