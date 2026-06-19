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

#pragma once

#include "fycore_export.h"

#include <cstdint>
#include <optional>

namespace Fooyin {
class FYCORE_EXPORT PlaybackProgressTracker
{
public:
    struct PositionUpdate
    {
        uint64_t positionMs{0};
        std::optional<uint64_t> positionSeconds;
        bool reachedPlayedThreshold{false};
    };

    PlaybackProgressTracker();

    void reset();
    void onTrackCommitted(uint64_t totalDurationMs, double playedThresholdFraction, uint64_t playedThresholdTimeMs);
    bool restartTracking();

    [[nodiscard]] PositionUpdate restorePosition(uint64_t posMs);
    [[nodiscard]] PositionUpdate restoreProgress(uint64_t posMs, uint64_t timeListenedMs);
    [[nodiscard]] PositionUpdate updatePosition(uint64_t posMs);
    [[nodiscard]] PositionUpdate resetPosition();

    bool markSeekPosition(uint64_t posMs);
    [[nodiscard]] bool updateBitrate(int bitrate);

    [[nodiscard]] uint64_t totalDuration() const;
    [[nodiscard]] uint64_t position() const;
    [[nodiscard]] uint64_t timeListened() const;
    [[nodiscard]] uint64_t playedThreshold() const;
    [[nodiscard]] bool playedThresholdReached() const;
    [[nodiscard]] int bitrate() const;

private:
    [[nodiscard]] PositionUpdate currentPositionUpdate(bool reachedPlayedThreshold = false);

    uint64_t m_totalDuration;
    uint64_t m_position;
    uint64_t m_timeListened;
    uint64_t m_playedThreshold;
    int m_bitrate;
    uint64_t m_lastPositionSecond;
    bool m_hasLastPositionSecond;
    bool m_trackingActive;
    bool m_seeking;
    bool m_counted;
};
} // namespace Fooyin
