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

#include "playercontroller.h"

#include "core/coresettings.h"
#include "core/library/models/track.h"

#include <pluginsystem/pluginmanager.h>
#include <utils/enumhelper.h>

namespace Core::Player {
PlayerController::PlayerController(QObject* parent)
    : PlayerManager(parent)
    , m_currentTrack(nullptr)
    , m_totalDuration(0)
    , m_playStatus(PlayState::Stopped)
    , m_playMode(PlayMode::Default)
    , m_position(0)
    , m_volume(1.0F)
    , m_counted(false)
{
    m_playMode = static_cast<PlayMode>(PluginSystem::object<SettingsManager>()->value(Settings::PlayMode).toInt());
}

PlayerController::~PlayerController() = default;

void PlayerController::reset()
{
    m_playStatus = Player::PlayState::Stopped;
    m_position   = 0;
}

void PlayerController::play()
{
    m_playStatus = Player::PlayState::Playing;
    emit playStateChanged(m_playStatus);
}

void PlayerController::wakeUp()
{
    emit wakeup();
}

void PlayerController::playPause()
{
    switch(m_playStatus) {
        case(Player::PlayState::Playing):
            return pause();
        case(Player::PlayState::Stopped):
            return wakeUp();
        default:
            return play();
    }
}

void PlayerController::pause()
{
    m_playStatus = Player::PlayState::Paused;
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

void PlayerController::setCurrentPosition(quint64 ms)
{
    m_position = ms;
    // TODO: Only increment playCount based on total time listened excluding seeking.
    if(!m_counted && ms >= m_totalDuration / 2) {
        // TODO: Save playCounts to db.
        quint16 playCount = m_currentTrack->playCount();
        m_currentTrack->setPlayCount(++playCount);
        m_counted = true;
    }
    emit positionChanged(ms);
}

void PlayerController::changePosition(quint64 ms)
{
    if(ms >= m_totalDuration - 100) {
        return next();
    }
    m_position = ms;
    emit positionMoved(ms);
}

void PlayerController::changeCurrentTrack(Track* track)
{
    m_currentTrack  = track;
    m_totalDuration = track->duration();
    m_position      = 0;
    m_counted       = false;

    emit currentTrackChanged(m_currentTrack);
    play();
}

void PlayerController::setRepeat()
{
    switch(m_playMode) {
        case Player::PlayMode::Default:
            m_playMode = Player::PlayMode::RepeatAll;
            break;
        case Player::PlayMode::RepeatAll:
            m_playMode = Player::PlayMode::Repeat;
            break;
        case Player::PlayMode::Repeat:
            m_playMode = Player::PlayMode::Default;
            break;
        case Player::PlayMode::Shuffle:
            m_playMode = Player::PlayMode::RepeatAll;
            break;
    }
    emit playModeChanged(m_playMode);
}

void PlayerController::setShuffle()
{
    if(m_playMode == Player::PlayMode::Shuffle) {
        m_playMode = Player::PlayMode::Default;
    }
    else {
        m_playMode = Player::PlayMode::Shuffle;
    }
    emit playModeChanged(m_playMode);
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

Player::PlayState PlayerController::playState() const
{
    return m_playStatus;
}

Player::PlayMode PlayerController::playMode() const
{
    return m_playMode;
}

quint64 PlayerController::currentPosition() const
{
    return m_position;
}

Track* PlayerController::currentTrack() const
{
    return m_currentTrack;
}

double PlayerController::volume() const
{
    return m_volume;
}
}; // namespace Core::Player
