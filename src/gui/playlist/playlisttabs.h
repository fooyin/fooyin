/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

class QVBoxLayout;
class QTabBar;

namespace Fy {

namespace Core::Playlist {
class PlaylistManager;
class Playlist;
} // namespace Core::Playlist

namespace Gui::Widgets::Playlist {
class PlaylistController;

class PlaylistTabs : public FyWidget
{
    Q_OBJECT

public:
    explicit PlaylistTabs(Core::Playlist::PlaylistManager* playlistHandler, PlaylistController* controller,
                          QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

    void setupTabs();

    int addPlaylist(const Core::Playlist::Playlist& playlist, bool switchTo);
    void removePlaylist(const Core::Playlist::Playlist& playlist);
    int addNewTab(const QString& name, const QIcon& icon = {});

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void tabChanged(int index);
    void playlistChanged(const Core::Playlist::Playlist& playlist);
    void playlistRenamed(const Core::Playlist::Playlist& playlist);

    Core::Playlist::PlaylistManager* m_playlistHandler;
    PlaylistController* m_controller;

    QVBoxLayout* m_layout;
    QTabBar* m_tabs;
};
} // namespace Gui::Widgets::Playlist
} // namespace Fy
