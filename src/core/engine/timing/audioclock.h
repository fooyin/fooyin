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

#include <chrono>
#include <cstdint>

namespace Fooyin {
class FYCORE_EXPORT AudioClock final
{
public:
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    enum class State : uint8_t
    {
        Stopped = 0,
        Paused,
        Playing,
    };

    enum class UpdateMode : uint8_t
    {
        Continuous = 0,
        Discontinuity,
    };

    AudioClock();

    void reset();
    void setStopped();
    void setPaused(TimePoint now);
    void setPlaying(TimePoint now, uint64_t fallbackPositionMs);

    uint64_t applyAuthoritative(uint64_t sourcePositionMs, uint64_t outputDelayMs, double delayToSourceScale,
                                TimePoint now, UpdateMode mode);
    [[nodiscard]] uint64_t position(TimePoint now) const;
    [[nodiscard]] State state() const;
    [[nodiscard]] double effectiveRate() const;

private:
    [[nodiscard]] double predictedPosition(TimePoint now) const;
    void reanchor(double positionMs, TimePoint now);

    State m_state;

    bool m_hasAnchor;
    double m_anchorPosMs;
    TimePoint m_anchorTime;
    bool m_hasLastSync;
    TimePoint m_lastSyncTime;

    double m_lastOutputMs;
    double m_effectiveRate;
    double m_rateTrim;
    double m_rateUsed;
};
} // namespace Fooyin
