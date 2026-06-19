/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "playbackprogresstracker.h"

#include <algorithm>
#include <utility>

namespace Fooyin {
PlaybackProgressTracker::PlaybackProgressTracker()
    : m_totalDuration{0}
    , m_position{0}
    , m_timeListened{0}
    , m_playedThreshold{0}
    , m_bitrate{0}
    , m_lastPositionSecond{0}
    , m_hasLastPositionSecond{false}
    , m_trackingActive{false}
    , m_seeking{false}
    , m_counted{false}
{ }

void PlaybackProgressTracker::reset()
{
    m_totalDuration         = 0;
    m_position              = 0;
    m_timeListened          = 0;
    m_playedThreshold       = 0;
    m_lastPositionSecond    = 0;
    m_hasLastPositionSecond = false;
    m_trackingActive        = false;
    m_seeking               = false;
    m_counted               = false;
}

void PlaybackProgressTracker::onTrackCommitted(uint64_t totalDurationMs, double playedThresholdFraction,
                                               uint64_t playedThresholdTimeMs)
{
    m_totalDuration   = totalDurationMs;
    m_position        = 0;
    m_timeListened    = 0;
    m_playedThreshold = 0;
    m_trackingActive  = true;
    m_seeking         = false;
    m_counted         = false;

    if(m_totalDuration > 200) {
        const auto playedThresholdFractionMs
            = static_cast<uint64_t>(static_cast<double>(m_totalDuration - 200) * playedThresholdFraction);
        m_playedThreshold = std::min(playedThresholdFractionMs, playedThresholdTimeMs);
    }
}

bool PlaybackProgressTracker::restartTracking()
{
    if(m_totalDuration == 0 || m_trackingActive) {
        return false;
    }

    m_position       = 0;
    m_timeListened   = 0;
    m_trackingActive = true;
    m_seeking        = false;
    m_counted        = false;
    return true;
}

PlaybackProgressTracker::PositionUpdate PlaybackProgressTracker::restorePosition(uint64_t posMs)
{
    m_position = posMs;
    m_seeking  = false;
    return currentPositionUpdate();
}

PlaybackProgressTracker::PositionUpdate PlaybackProgressTracker::restoreProgress(uint64_t posMs,
                                                                                 uint64_t timeListenedMs)
{
    m_position       = posMs;
    m_timeListened   = timeListenedMs;
    m_trackingActive = true;
    m_seeking        = false;
    m_counted        = false;

    if(m_playedThreshold > 0 && m_timeListened >= m_playedThreshold) {
        m_timeListened = m_playedThreshold - 1;
    }
    else if(m_playedThreshold == 0) {
        m_timeListened = 0;
    }

    return currentPositionUpdate();
}

PlaybackProgressTracker::PositionUpdate PlaybackProgressTracker::updatePosition(uint64_t posMs)
{
    if(m_trackingActive && !m_seeking && posMs > m_position) {
        m_timeListened += (posMs - m_position);
    }

    m_seeking  = false;
    m_position = posMs;

    bool reachedPlayedThreshold = false;
    if(m_trackingActive && !m_counted && m_timeListened >= m_playedThreshold) {
        m_counted              = true;
        reachedPlayedThreshold = true;
    }

    return currentPositionUpdate(reachedPlayedThreshold);
}

PlaybackProgressTracker::PositionUpdate PlaybackProgressTracker::resetPosition()
{
    m_position       = 0;
    m_trackingActive = false;
    m_seeking        = false;
    return currentPositionUpdate();
}

bool PlaybackProgressTracker::markSeekPosition(uint64_t posMs)
{
    if(std::exchange(m_position, posMs) == posMs) {
        return false;
    }

    m_trackingActive = true;
    m_seeking        = true;
    return true;
}

bool PlaybackProgressTracker::updateBitrate(int bitrate)
{
    return std::exchange(m_bitrate, bitrate) != bitrate;
}

uint64_t PlaybackProgressTracker::totalDuration() const
{
    return m_totalDuration;
}

uint64_t PlaybackProgressTracker::position() const
{
    return m_position;
}

uint64_t PlaybackProgressTracker::timeListened() const
{
    return m_timeListened;
}

uint64_t PlaybackProgressTracker::playedThreshold() const
{
    return m_playedThreshold;
}

bool PlaybackProgressTracker::playedThresholdReached() const
{
    return m_counted;
}

int PlaybackProgressTracker::bitrate() const
{
    return m_bitrate;
}

PlaybackProgressTracker::PositionUpdate PlaybackProgressTracker::currentPositionUpdate(bool reachedPlayedThreshold)
{
    PositionUpdate update{
        .positionMs             = m_position,
        .positionSeconds        = {},
        .reachedPlayedThreshold = reachedPlayedThreshold,
    };

    const uint64_t seconds = m_position / 1000;
    if(!m_hasLastPositionSecond || m_lastPositionSecond != seconds) {
        m_lastPositionSecond    = seconds;
        m_hasLastPositionSecond = true;
        update.positionSeconds  = seconds;
    }

    return update;
}
} // namespace Fooyin
