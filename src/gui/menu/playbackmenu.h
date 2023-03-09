/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include <core/player/playermanager.h>

#include <QIcon>
#include <QObject>

class QAction;
class QActionGroup;

namespace Fy {

namespace Utils {
class ActionManager;
class ActionContainer;
class SettingsManager;
} // namespace Utils

namespace Core {
namespace Player {
class PlayerManager;
}
} // namespace Core

namespace Gui {
class PlaybackMenu : public QObject
{
    Q_OBJECT

public:
    PlaybackMenu(Utils::ActionManager* actionManager, Core::Player::PlayerManager* playerManager,
                 QObject* parent = nullptr);

private:
    void updatePlayPause(Core::Player::PlayState state);
    void updatePlayMode(Core::Player::PlayMode mode);

    Utils::ActionManager* m_actionManager;
    Core::Player::PlayerManager* m_playerManager;

    QAction* m_stop;
    QAction* m_playPause;
    QAction* m_previous;
    QAction* m_next;

    QActionGroup* m_playbackGroup;

    QAction* m_default;
    QAction* m_repeat;
    QAction* m_repeatAll;
    QAction* m_shuffle;

    QIcon m_playIcon;
    QIcon m_pauseIcon;
};
} // namespace Gui
} // namespace Fy
