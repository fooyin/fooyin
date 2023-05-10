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

#include <chrono>

namespace Fy::Core::Engine::FFmpeg {
class AudioClock
{
public:
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    AudioClock();

    void sync(uint64_t position = 0);
    void sync(const TimePoint& tp, uint64_t position);

    uint64_t currentPosition() const;

    void setPaused(bool paused);

    TimePoint timeFromPosition(uint64_t position, bool ignorePause = false) const;
    uint64_t positionFromTime(TimePoint tp, bool ignorePause = false) const;

private:
    using TrackTime = std::chrono::milliseconds;

    template <typename T>
    static Clock::duration toClockTime(const T& t)
    {
        return std::chrono::duration_cast<Clock::duration>(t);
    }

    template <typename T>
    static TrackTime toTrackTime(const T& t)
    {
        return std::chrono::duration_cast<TrackTime>(t);
    }

    bool m_paused;
    TrackTime m_position;
    TimePoint m_timePoint;
};
} // namespace Fy::Core::Engine::FFmpeg
