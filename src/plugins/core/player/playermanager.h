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

#include "core/typedefs.h"

#include <QObject>

namespace Core {
class Track;

namespace Player {
class PlayerManager : public QObject
{
    Q_OBJECT

public:
    explicit PlayerManager(QObject* parent)
        : QObject(parent){};
    ~PlayerManager() override = default;

    [[nodiscard]] virtual Player::PlayState playState() const = 0;
    [[nodiscard]] virtual Player::PlayMode playMode() const = 0;
    [[nodiscard]] virtual quint64 currentPosition() const = 0;
    [[nodiscard]] virtual Track* currentTrack() const = 0;
    [[nodiscard]] virtual double volume() const = 0;

signals:
    void playStateChanged(Player::PlayState);
    void playModeChanged(Player::PlayMode);
    void nextTrack();
    void wakeup();
    void previousTrack();
    void positionChanged(quint64 ms);
    void positionMoved(quint64 ms);
    void currentTrackChanged(Core::Track* track);
    void volumeChanged(double value);
    void muteChanged(bool b);

public:
    virtual void play() = 0;
    virtual void wakeUp() = 0;
    virtual void playPause() = 0;
    virtual void pause() = 0;
    virtual void previous() = 0;
    virtual void next() = 0;
    virtual void stop() = 0;
    virtual void reset() = 0;
    virtual void setRepeat() = 0;
    virtual void setShuffle() = 0;
    virtual void setCurrentPosition(quint64 ms) = 0;
    virtual void changePosition(quint64 ms) = 0;
    virtual void changeCurrentTrack(Core::Track* track) = 0;
    virtual void volumeUp() = 0;
    virtual void volumeDown() = 0;
    virtual void setVolume(double vol) = 0;
};
}; // namespace Player
}; // namespace Core
