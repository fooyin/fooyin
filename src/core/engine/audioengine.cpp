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

#include <core/engine/audioengine.h>

namespace Fooyin {
AudioEngine::AudioEngine(QObject* parent)
    : QObject{parent}
    , m_status{NoTrack}
    , m_state{StoppedState}
    , m_position{0}
{ }

PlaybackState AudioEngine::state() const
{
    return m_state;
}

TrackStatus AudioEngine::trackStatus() const
{
    return m_status;
}

uint64_t AudioEngine::position() const
{
    return m_position;
}

void AudioEngine::changeState(PlaybackState state)
{
    if(std::exchange(m_state, state) != state) {
        emit stateChanged(m_state);
    }
}

void AudioEngine::changeTrackStatus(TrackStatus status)
{
    if(std::exchange(m_status, status) != status) {
        emit trackStatusChanged(m_status);
    }
}

void AudioEngine::startup() { }

void AudioEngine::shutdown() { }
} // namespace Fooyin

#include "core/engine/moc_audioengine.cpp"
