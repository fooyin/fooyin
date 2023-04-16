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
namespace Utils {
class ActionManager;
}

namespace Core::Playlist {
class PlaylistHandler;
class Playlist;
} // namespace Core::Playlist

namespace Gui::Widgets {
class WidgetFactory;
class PlaylistTabs : public FyWidget
{
    Q_OBJECT

public:
    explicit PlaylistTabs(Utils::ActionManager* actionManager, WidgetFactory* widgetFactory,
                          Core::Playlist::PlaylistHandler* playlistHandler, QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

    void setupTabs();

    int addPlaylist(Core::Playlist::Playlist* playlist);
    void removePlaylist(int id);
    int addNewTab(const QString& name, const QIcon& icon = {});

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    //    void layoutEditingMenu(Utils::ActionContainer* menu) override;

private:
    void tabChanged(int index);
    void playlistChanged(Core::Playlist::Playlist* playlist);
    void playlistRenamed(Core::Playlist::Playlist* playlist);

    Utils::ActionManager* m_actionManager;
    WidgetFactory* m_widgetFactory;
    Core::Playlist::PlaylistHandler* m_playlistHandler;

    QVBoxLayout* m_layout;
    QTabBar* m_tabs;
};
} // namespace Gui::Widgets
} // namespace Fy
