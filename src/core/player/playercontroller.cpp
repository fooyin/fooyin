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

#include "playercontroller.h"

#include <core/coresettings.h>
#include <core/track.h>
#include <utils/settings/settingsmanager.h>

namespace Fy::Core::Player {
PlayerController::PlayerController(Utils::SettingsManager* settings, QObject* parent)
    : PlayerManager{parent}
    , m_settings{settings}
    , m_totalDuration{0}
    , m_playStatus{PlayState::Stopped}
    , m_playMode{PlayMode::Default}
    , m_position{0}
    , m_volume{1.0F}
    , m_counted{false}
{
    m_playMode = static_cast<PlayMode>(m_settings->value<Settings::PlayMode>());
    m_settings->subscribe<Settings::PlayMode>(this, &PlayerController::changePlayMode);
}

void PlayerController::reset()
{
    m_playStatus   = PlayState::Stopped;
    m_position     = 0;
    m_currentTrack = {};
}

void PlayerController::play()
{
    m_playStatus = PlayState::Playing;
    emit playStateChanged(m_playStatus);
}

void PlayerController::wakeUp()
{
    emit wakeup();
}

void PlayerController::playPause()
{
    switch(m_playStatus) {
        case(PlayState::Playing):
            return pause();
        case(PlayState::Stopped):
            return wakeUp();
        default:
            return play();
    }
}

void PlayerController::pause()
{
    m_playStatus = PlayState::Paused;
    emit playStateChanged(m_playStatus);
}

void PlayerController::previous()
{
    emit previousTrack();
}

void PlayerController::next()
{
    emit nextTrack();
}

void PlayerController::stop()
{
    reset();
    emit playStateChanged(m_playStatus);
}

void PlayerController::setCurrentPosition(uint64_t ms)
{
    m_position = ms;
    // TODO: Only increment playCount based on total time listened excluding seeking.
    if(!m_counted && ms >= m_totalDuration / 2) {
        // TODO: Save playCounts to db.
        int playCount = m_currentTrack.playCount();
        m_currentTrack.setPlayCount(++playCount);
        m_counted = true;
    }
    emit positionChanged(ms);
}

void PlayerController::changePosition(uint64_t ms)
{
    if(ms >= m_totalDuration - 100) {
        return next();
    }
    m_position = ms;
    emit positionMoved(ms);
}

void PlayerController::changeCurrentTrack(const Track& track)
{
    m_currentTrack  = track;
    m_totalDuration = track.duration();
    m_position      = 0;
    m_counted       = false;

    emit currentTrackChanged(m_currentTrack);
    play();
}

void PlayerController::setPlayMode(PlayMode mode)
{
    m_settings->set<Settings::PlayMode>(static_cast<int>(mode));
}

void PlayerController::volumeUp()
{
    setVolume(volume() + 5);
}

void PlayerController::volumeDown()
{
    setVolume(volume() - 0.05F);
}

void PlayerController::setVolume(double value)
{
    m_volume = value;
    emit volumeChanged(value);
}

PlayState PlayerController::playState() const
{
    return m_playStatus;
}

PlayMode PlayerController::playMode() const
{
    return m_playMode;
}

uint64_t PlayerController::currentPosition() const
{
    return m_position;
}

Track PlayerController::currentTrack() const
{
    return m_currentTrack;
}

double PlayerController::volume() const
{
    return m_volume;
}

void PlayerController::changePlayMode()
{
    const auto mode = static_cast<PlayMode>(m_settings->value<Settings::PlayMode>());
    if(std::exchange(m_playMode, mode) != mode) {
        emit playModeChanged(m_playMode);
    }
}
} // namespace Fy::Core::Player
