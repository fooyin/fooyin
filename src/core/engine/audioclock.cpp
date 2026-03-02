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

#include "audioclock.h"

#include <QLoggingCategory>
#include <QTimerEvent>

#include <algorithm>
#include <cmath>
#include <limits>

constexpr auto PositionSyncIntervalMs    = 50;
constexpr auto PositionPublishIntervalMs = 33;
constexpr auto SyncTimerGapWarnMs        = 500;
constexpr auto PublishTimerGapWarnMs     = 500;

constexpr auto MinEffectiveRate     = 0.05;
constexpr auto MaxEffectiveRate     = 8.0;
constexpr auto MinDelayToSourceRate = 0.01;
constexpr auto MaxDelayToSourceRate = 8.0;

Q_LOGGING_CATEGORY(AUDIO_CLOCK, "fy.audioclock")

namespace {
uint64_t saturatingSub(uint64_t lhs, uint64_t rhs)
{
    return rhs > lhs ? 0 : (lhs - rhs);
}

double elapsedMs(const std::chrono::steady_clock::time_point now, const std::chrono::steady_clock::time_point since)
{
    if(now <= since) {
        return 0.0;
    }

    return std::chrono::duration<double, std::milli>(now - since).count();
}
} // namespace

namespace Fooyin {
AudioClock::AudioClock(QObject* parent)
    : QObject{parent}
    , m_state{State::Stopped}
    , m_generation{0}
    , m_hasAnchor{false}
    , m_anchorPosMs{0.0}
    , m_lastOutputMs{0.0}
    , m_rateUsed{1.0}
    , m_lastReportedPositionMs{0}
    , m_syncTimerId{0}
    , m_publishTimerId{0}
    , m_hasLastSyncTimerTick{false}
    , m_hasLastPublishTimerTick{false}
    , m_syncTimerGapLogActive{false}
    , m_publishTimerGapLogActive{false}
{ }

void AudioClock::start()
{
    if(m_syncTimerId == 0) {
        m_syncTimerId = startTimer(PositionSyncIntervalMs, Qt::CoarseTimer);
    }
    if(m_publishTimerId == 0) {
        m_publishTimerId = startTimer(PositionPublishIntervalMs, Qt::PreciseTimer);
    }

    m_hasLastSyncTimerTick     = false;
    m_hasLastPublishTimerTick  = false;
    m_syncTimerGapLogActive    = false;
    m_publishTimerGapLogActive = false;
}

void AudioClock::stop()
{
    if(m_syncTimerId != 0) {
        killTimer(m_syncTimerId);
        m_syncTimerId = 0;
    }
    if(m_publishTimerId != 0) {
        killTimer(m_publishTimerId);
        m_publishTimerId = 0;
    }

    m_hasLastSyncTimerTick     = false;
    m_hasLastPublishTimerTick  = false;
    m_syncTimerGapLogActive    = false;
    m_publishTimerGapLogActive = false;
}

void AudioClock::applyPosition(uint64_t sourcePositionMs, uint64_t outputDelayMs, double delayToSourceScale,
                               uint64_t generation, const UpdateMode mode, bool emitNow)
{
    const auto now = Clock::now();
    const auto presented
        = static_cast<double>(presentedFromSource(sourcePositionMs, outputDelayMs, delayToSourceScale));
    m_rateUsed = clampRate(delayToSourceScale);

    const bool forceDiscontinuity
        = mode == UpdateMode::Discontinuity || !m_hasAnchor || m_state != State::Playing || generation != m_generation;

    if(forceDiscontinuity) {
        m_generation   = generation;
        m_lastOutputMs = presented;
        reanchor(presented, now);
    }
    else {
        const double predicted = predictedPositionMs(now);
        const double outputPos = std::max({presented, m_lastOutputMs, predicted});
        m_lastOutputMs         = outputPos;
        reanchor(outputPos, now);
    }

    if(emitNow) {
        emitPosition(toPositionMs(predictedPositionMs(now)));
    }
}

void AudioClock::setPlaying(uint64_t fallbackPositionMs)
{
    const auto now        = Clock::now();
    const double position = m_hasAnchor ? predictedPositionMs(now) : static_cast<double>(fallbackPositionMs);
    reanchor(position, now);
    m_lastOutputMs = position;
    m_state        = State::Playing;
}

void AudioClock::setPaused()
{
    const auto now        = Clock::now();
    const double position = predictedPositionMs(now);
    reanchor(position, now);
    m_lastOutputMs = position;
    m_state        = State::Paused;
    emitPosition(toPositionMs(position));
}

void AudioClock::setStopped()
{
    m_state      = State::Stopped;
    m_generation = 0;
    resetClock();
}

uint64_t AudioClock::position() const
{
    return m_lastReportedPositionMs.load(std::memory_order_relaxed);
}

uint64_t AudioClock::generation() const
{
    return m_generation;
}

void AudioClock::timerEvent(QTimerEvent* event)
{
    const int timerId = event->timerId();
    const auto now    = Clock::now();

    if(timerId == m_syncTimerId) {
        if(m_hasLastSyncTimerTick) {
            const auto gapMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastSyncTimerTick).count();
            if(gapMs >= SyncTimerGapWarnMs) {
                if(!m_syncTimerGapLogActive) {
                    m_syncTimerGapLogActive = true;
                    qCWarning(AUDIO_CLOCK) << "Position sync timer gap detected:" << "gapMs=" << gapMs
                                           << "expectedMs=" << PositionSyncIntervalMs;
                }
            }
            else {
                m_syncTimerGapLogActive = false;
            }
        }
        else {
            m_syncTimerGapLogActive = false;
        }
        m_lastSyncTimerTick    = now;
        m_hasLastSyncTimerTick = true;
        emit requestSyncPosition();
        return;
    }

    if(timerId == m_publishTimerId) {
        if(m_hasLastPublishTimerTick) {
            const auto gapMs
                = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastPublishTimerTick).count();
            if(gapMs >= PublishTimerGapWarnMs) {
                if(!m_publishTimerGapLogActive) {
                    m_publishTimerGapLogActive = true;
                    qCWarning(AUDIO_CLOCK) << "Position publish timer gap detected:" << "gapMs=" << gapMs
                                           << "expectedMs=" << PositionPublishIntervalMs;
                }
            }
            else {
                m_publishTimerGapLogActive = false;
            }
        }
        else {
            m_publishTimerGapLogActive = false;
        }
        m_lastPublishTimerTick    = now;
        m_hasLastPublishTimerTick = true;
        emitPosition(toPositionMs(predictedPositionMs(now)));
        return;
    }

    QObject::timerEvent(event);
}

uint64_t AudioClock::presentedFromSource(uint64_t sourcePositionMs, uint64_t outputDelayMs, double delayToSourceScale)
{
    const double scale       = std::clamp(delayToSourceScale, MinDelayToSourceRate, MaxDelayToSourceRate);
    const double delaySource = static_cast<double>(outputDelayMs) * scale;
    const auto delayMs       = static_cast<uint64_t>(std::llround(std::max(0.0, delaySource)));

    return saturatingSub(sourcePositionMs, delayMs);
}

double AudioClock::clampRate(double rate)
{
    return std::clamp(rate, std::max(0.01, MinEffectiveRate), std::max({0.01, MinEffectiveRate, MaxEffectiveRate}));
}

uint64_t AudioClock::toPositionMs(double positionMs)
{
    if(!std::isfinite(positionMs)) {
        return 0;
    }

    const double clamped = std::clamp(positionMs, 0.0, static_cast<double>(std::numeric_limits<uint64_t>::max()));
    return static_cast<uint64_t>(std::llround(clamped));
}

double AudioClock::predictedPositionMs(const TimePoint now) const
{
    if(!m_hasAnchor) {
        return 0.0;
    }

    if(m_state != State::Playing) {
        return m_anchorPosMs;
    }

    const double elapsed      = elapsedMs(now, m_anchorTime);
    const double scaledOffset = elapsed * m_rateUsed;
    return std::max(0.0, m_anchorPosMs + scaledOffset);
}

void AudioClock::reanchor(double positionMs, const TimePoint now)
{
    m_hasAnchor   = true;
    m_anchorPosMs = std::max(0.0, positionMs);
    m_anchorTime  = now;
}

void AudioClock::emitPosition(uint64_t positionMs)
{
    if(m_lastReportedPositionMs.exchange(positionMs, std::memory_order_relaxed) == positionMs) {
        return;
    }

    emit positionChanged(positionMs);
}

void AudioClock::resetClock()
{
    m_hasAnchor    = false;
    m_anchorPosMs  = 0.0;
    m_anchorTime   = {};
    m_lastOutputMs = 0.0;
    m_rateUsed     = 1.0;
    m_lastReportedPositionMs.store(0, std::memory_order_relaxed);
}
} // namespace Fooyin
