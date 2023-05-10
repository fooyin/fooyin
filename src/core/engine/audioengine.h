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

#include "audioplayer.h"

#include <QObject>

namespace Fy::Core::Engine {
class AudioOutput;

class AudioEngine
{
public:
    virtual ~AudioEngine();

    virtual PlaybackState state() const;
    virtual TrackStatus trackStatus() const;

    virtual uint64_t position() const;
    virtual void seek(uint64_t position) = 0;

    virtual void changeTrack(const QString& media) = 0;

    virtual void play()  = 0;
    virtual void pause() = 0;
    virtual void stop()  = 0;

    virtual void setAudioOutput(AudioOutput* output);

    void positionChanged(uint64_t position);

    void stateChanged(PlaybackState newState);
    void trackStatusChanged(TrackStatus status);

    void trackFinished();

protected:
    explicit AudioEngine(AudioPlayer* parent = nullptr);

private:
    AudioPlayer* m_player;
    TrackStatus m_status;
    PlaybackState m_state;
    uint64_t m_position;
};
} // namespace Fy::Core::Engine
