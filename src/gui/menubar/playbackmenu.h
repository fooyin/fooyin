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

#include <core/player/playercontroller.h>

#include <QIcon>
#include <QObject>

class QAction;

namespace Fooyin {
class ActionManager;
class PlayerController;
class SettingsManager;

class PlaybackMenu : public QObject
{
    Q_OBJECT

public:
    PlaybackMenu(ActionManager* actionManager, PlayerController* playerController, SettingsManager* settings,
                 QObject* parent = nullptr);

private:
    void updatePlayPause(Player::PlayState state) const;
    void updatePlayMode(Playlist::PlayModes mode) const;
    void setPlayMode(Playlist::PlayMode mode) const;

    ActionManager* m_actionManager;
    PlayerController* m_playerController;
    SettingsManager* m_settings;

    QIcon m_playIcon;
    QIcon m_pauseIcon;

    QAction* m_stop;
    QAction* m_playPause;
    QAction* m_previous;
    QAction* m_next;

    QAction* m_defaultPlayback;
    QAction* m_repeatTrack;
    QAction* m_repeatAlbum;
    QAction* m_repeatPlaylist;
    QAction* m_shuffleTracks;
    QAction* m_shuffleAlbums;
    QAction* m_random;

    QAction* m_stopAfterCurrent;
};
} // namespace Fooyin
