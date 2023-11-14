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
struct PlayerController::Private
{
    PlayerController* self;

    Utils::SettingsManager* settings;

    Track currentTrack;
    uint64_t totalDuration{0};
    PlayState playStatus{PlayState::Stopped};
    Playlist::PlayModes playMode;
    uint64_t position{0};
    bool counted{false};

    Private(PlayerController* self, Utils::SettingsManager* settings)
        : self{self}
        , settings{settings}
        , playMode{static_cast<Playlist::PlayModes>(settings->value<Settings::PlayMode>())}
    { }
};

PlayerController::PlayerController(Utils::SettingsManager* settings, QObject* parent)
    : PlayerManager{parent}
    , p{std::make_unique<Private>(this, settings)}
{
    settings->subscribe<Settings::PlayMode>(this, [this]() {
        const auto mode = static_cast<Playlist::PlayModes>(p->settings->value<Settings::PlayMode>());
        if(std::exchange(p->playMode, mode) != mode) {
            emit playModeChanged(mode);
        }
    });
}

PlayerController::~PlayerController() = default;

void PlayerController::reset()
{
    p->playStatus   = PlayState::Stopped;
    p->position     = 0;
    p->currentTrack = {};
}

void PlayerController::play()
{
    p->playStatus = PlayState::Playing;
    emit playStateChanged(p->playStatus);
}

void PlayerController::wakeUp()
{
    emit wakeup();
}

void PlayerController::playPause()
{
    switch(p->playStatus) {
        case(PlayState::Playing): {
            pause();
            break;
        }
        case(PlayState::Stopped): {
            wakeUp();
            break;
        }
        default: {
            play();
            break;
        }
    }
}

void PlayerController::pause()
{
    p->playStatus = PlayState::Paused;
    emit playStateChanged(p->playStatus);
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
    emit playStateChanged(p->playStatus);
}

void PlayerController::setCurrentPosition(uint64_t ms)
{
    p->position = ms;
    // TODO: Only increment playCount based on total time listened excluding seeking.
    if(!p->counted && ms >= p->totalDuration / 2) {
        // TODO: Save playCounts to db.
        int playCount = p->currentTrack.playCount();
        p->currentTrack.setPlayCount(++playCount);
        p->counted = true;
    }
    emit positionChanged(ms);
}

void PlayerController::changePosition(uint64_t ms)
{
    if(ms >= p->totalDuration - 100) {
        next();
        return;
    }
    p->position = ms;
    emit positionMoved(ms);
}

void PlayerController::changeCurrentTrack(const Track& track)
{
    p->currentTrack  = track;
    p->totalDuration = track.duration();
    p->position      = 0;
    p->counted       = false;

    emit currentTrackChanged(p->currentTrack);
}

void PlayerController::setPlayMode(Playlist::PlayModes mode)
{
    p->settings->set<Settings::PlayMode>(static_cast<int>(mode));
}

PlayState PlayerController::playState() const
{
    return p->playStatus;
}

Playlist::PlayModes PlayerController::playMode() const
{
    return p->playMode;
}

uint64_t PlayerController::currentPosition() const
{
    return p->position;
}

Track PlayerController::currentTrack() const
{
    return p->currentTrack;
}
} // namespace Fy::Core::Player

#include "moc_playercontroller.cpp"
