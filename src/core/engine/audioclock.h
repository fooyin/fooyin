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

#include <QObject>

#include <atomic>
#include <chrono>
#include <cstdint>

class QTimerEvent;

namespace Fooyin {
/*!
 * Engine-side playback position clock.
 *
 * `AudioClock` combines rendered source position with output latency and a
 * short-term rate estimate to publish smooth position updates between explicit
 * pipeline sync points.
 */
class FYCORE_EXPORT AudioClock : public QObject
{
    Q_OBJECT

public:
    enum class State : uint8_t
    {
        Stopped = 0,
        Paused,
        Playing,
    };

    enum class UpdateMode : uint8_t
    {
        //! Normal monotonic update; keep predictive anchor continuity.
        Continuous = 0,
        //! Discontinuity (seek/switch); force re-anchor.
        Discontinuity,
    };

    explicit AudioClock(QObject* parent = nullptr);

    //! Start periodic sync/publish timers.
    void start();
    //! Stop timers and clear predictive anchor state.
    void stop();

    //! Apply latest source position + delay context from pipeline.
    void applyPosition(uint64_t sourcePositionMs, uint64_t outputDelayMs, double delayToSourceScale,
                       uint64_t generation, UpdateMode mode, bool emitNow);

    //! Set playing state and optionally re-anchor from fallback position.
    void setPlaying(uint64_t fallbackPositionMs);
    //! Set paused state and freeze anchor at current predicted position.
    void setPaused();
    //! Set stopped state and reset to zero.
    void setStopped();

    //! Last published/predicted position in milliseconds.
    [[nodiscard]] uint64_t position() const;
    //! Monotonic context generation from producer updates.
    [[nodiscard]] uint64_t generation() const;

signals:
    //! Request immediate sync sample from pipeline/engine.
    void requestSyncPosition();
    //! Published clock position update in milliseconds.
    void positionChanged(uint64_t positionMs);

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    [[nodiscard]] static uint64_t presentedFromSource(uint64_t sourcePositionMs, uint64_t outputDelayMs,
                                                      double delayToSourceScale);
    [[nodiscard]] static double clampRate(double rate);
    [[nodiscard]] static uint64_t toPositionMs(double positionMs);
    [[nodiscard]] double predictedPositionMs(TimePoint now) const;
    void reanchor(double positionMs, TimePoint now);
    void emitPosition(uint64_t positionMs);
    void resetClock();

    State m_state;
    uint64_t m_generation;

    bool m_hasAnchor;
    double m_anchorPosMs;
    TimePoint m_anchorTime;
    double m_lastOutputMs;
    double m_rateUsed;

    std::atomic<uint64_t> m_lastReportedPositionMs;

    int m_syncTimerId;
    int m_publishTimerId;

    TimePoint m_lastSyncTimerTick;
    TimePoint m_lastPublishTimerTick;
    bool m_hasLastSyncTimerTick;
    bool m_hasLastPublishTimerTick;

    bool m_syncTimerGapLogActive;
    bool m_publishTimerGapLogActive;
};
} // namespace Fooyin
