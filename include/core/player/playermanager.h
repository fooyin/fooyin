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

#include "fycore_export.h"

#include <QObject>

namespace Fy::Core {
class Track;

namespace Player {
enum class PlayMode : int
{
    Default = 0,
    RepeatAll,
    Repeat,
    Shuffle,
};

enum class PlayState
{
    Playing = 0,
    Paused,
    Stopped,
};

class FYCORE_EXPORT PlayerManager : public QObject
{
    Q_OBJECT

public:
    explicit PlayerManager(QObject* parent = nullptr)
        : QObject{parent} {};

    [[nodiscard]] virtual PlayState playState() const      = 0;
    [[nodiscard]] virtual PlayMode playMode() const        = 0;
    [[nodiscard]] virtual uint64_t currentPosition() const = 0;
    [[nodiscard]] virtual Track currentTrack() const       = 0;

    virtual void play()                                       = 0;
    virtual void wakeUp()                                     = 0;
    virtual void playPause()                                  = 0;
    virtual void pause()                                      = 0;
    virtual void previous()                                   = 0;
    virtual void next()                                       = 0;
    virtual void stop()                                       = 0;
    virtual void reset()                                      = 0;
    virtual void setPlayMode(PlayMode mode)                   = 0;
    virtual void setCurrentPosition(uint64_t ms)              = 0;
    virtual void changePosition(uint64_t ms)                  = 0;
    virtual void changeCurrentTrack(const Core::Track& track) = 0;

signals:
    void playStateChanged(Core::Player::PlayState);
    void playModeChanged(Core::Player::PlayMode);
    void nextTrack();
    void wakeup();
    void previousTrack();
    void positionChanged(uint64_t ms);
    void positionMoved(uint64_t ms);
    void currentTrackChanged(const Core::Track& track);
};
} // namespace Player
} // namespace Fy::Core
