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

#include <core/playlist/playlist.h>

#include <QObject>

namespace Fooyin {
class Track;

enum class PlayState
{
    Playing = 0,
    Paused,
    Stopped,
};

/*!
 * Handles track playback
 */
class FYCORE_EXPORT PlayerManager : public QObject
{
    Q_OBJECT

public:
    explicit PlayerManager(QObject* parent = nullptr)
        : QObject{parent} {};

    /** Returns the current state (playing, paused or stopped). */
    [[nodiscard]] virtual PlayState playState() const = 0;

    /** Returns the current playlist mode (shuffle and repeat flags). */
    [[nodiscard]] virtual Playlist::PlayModes playMode() const = 0;

    /** Returns the current playback position in ms. */
    [[nodiscard]] virtual uint64_t currentPosition() const = 0;

    /*!
     * Returns the currently playing track.
     * @note the track will be invalid if the state is 'Stopped'.
     */
    [[nodiscard]] virtual Track currentTrack() const = 0;

    /** Starts playback of the current playlist. */
    virtual void play() = 0;

    /** Toggles playback. */
    virtual void playPause() = 0;

    virtual void pause()    = 0;
    virtual void previous() = 0;
    virtual void next()     = 0;
    virtual void stop()     = 0;

    /** Stops playback and clears position and current track. */
    virtual void reset() = 0;

    virtual void setPlayMode(Playlist::PlayModes mode)  = 0;
    virtual void setCurrentPosition(uint64_t ms)        = 0;
    virtual void changePosition(uint64_t ms)            = 0;
    virtual void changeCurrentTrack(const Track& track) = 0;

signals:
    void playStateChanged(PlayState state);
    void playModeChanged(Playlist::PlayModes mode);
    void nextTrack();
    void previousTrack();
    void positionChanged(uint64_t ms);
    void positionMoved(uint64_t ms);
    void currentTrackChanged(const Track& track);
};
} // namespace Fooyin
