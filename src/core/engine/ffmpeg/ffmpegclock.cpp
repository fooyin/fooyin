/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include "ffmpegclock.h"

namespace Fooyin {
AudioClock::AudioClock()
    : m_paused{true}
{
    sync();
}

void AudioClock::sync(uint64_t position)
{
    sync(Clock::now(), position);
}

void AudioClock::sync(const TimePoint& tp, uint64_t position)
{
    m_position  = TrackTime{position};
    m_timePoint = tp;
}

uint64_t AudioClock::currentPosition() const
{
    return positionFromTime(Clock::now());
}

void AudioClock::setPaused(bool paused)
{
    if(m_paused == paused) {
        return;
    }
    const auto now = Clock::now();
    if(!m_paused) {
        m_position = m_position + toTrackTime(now - m_timePoint);
    }
    m_timePoint = now;
    m_paused    = paused;
}

uint64_t AudioClock::positionFromTime(TimePoint tp, bool ignorePause) const
{
    tp                  = m_paused && !ignorePause ? m_timePoint : tp;
    const TrackTime pos = m_position + toTrackTime(tp - m_timePoint);
    return pos.count();
}

AudioClock::TimePoint AudioClock::timeFromPosition(uint64_t position, bool ignorePause) const
{
    auto pos = m_paused && !ignorePause ? m_position : TrackTime{position};
    return m_timePoint + toClockTime(pos - m_position);
}
} // namespace Fooyin
