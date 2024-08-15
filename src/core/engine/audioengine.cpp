/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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
PlaybackState AudioEngine::playbackState() const
{
    return m_playbackState.load(std::memory_order_acquire);
}

TrackStatus AudioEngine::trackStatus() const
{
    return m_trackStatus.load(std::memory_order_acquire);
}

PlaybackState AudioEngine::updateState(PlaybackState state)
{
    const PlaybackState prevState = playbackState();
    m_playbackState.store(state, std::memory_order_release);
    if(prevState != state) {
        emit stateChanged(state);
    }
    return prevState;
}

TrackStatus AudioEngine::updateTrackStatus(TrackStatus status)
{
    const TrackStatus prevStatus = trackStatus();
    m_trackStatus.store(status, std::memory_order_release);
    if(prevStatus != status) {
        emit trackStatusChanged(status);
    }
    return prevStatus;
}
} // namespace Fooyin
