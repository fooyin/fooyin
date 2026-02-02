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

#include <algorithm>
#include <cmath>
#include <limits>

constexpr auto DeadbandMs           = 2.0;
constexpr auto MaxLeadMs            = 120.0;
constexpr auto MaxSlewMsPerSecond   = 120.0;
constexpr auto RateTrimTauSeconds   = 0.5;
constexpr auto MaxRateTrim          = 0.25;
constexpr auto MinEffectiveRate     = 0.25;
constexpr auto MaxEffectiveRate     = 4.0;
constexpr auto MinDelayToSourceRate = 0.01;
constexpr auto MaxDelayToSourceRate = 8.0;

namespace {
using Clock     = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

double clampEffectiveRate(double value)
{
    return std::clamp(value, std::max(0.01, MinEffectiveRate), std::max({0.01, MinEffectiveRate, MaxEffectiveRate}));
}

uint64_t saturatingSub(const uint64_t lhs, const uint64_t rhs)
{
    return rhs > lhs ? 0 : (lhs - rhs);
}

uint64_t toPositionMs(const double value)
{
    if(!std::isfinite(value)) {
        return 0;
    }

    const double clamped = std::clamp(value, 0.0, static_cast<double>(std::numeric_limits<uint64_t>::max()));
    return static_cast<uint64_t>(std::llround(clamped));
}

double elapsedMs(const TimePoint now, const TimePoint since)
{
    if(now <= since) {
        return 0.0;
    }

    return std::chrono::duration<double, std::milli>(now - since).count();
}

uint64_t delayInSourceTimelineMs(const uint64_t outputDelayMs, const double delayToSourceScale)
{
    const double scale = std::clamp(delayToSourceScale, MinDelayToSourceRate, MaxDelayToSourceRate);
    const double delay = static_cast<double>(outputDelayMs) * scale;

    return static_cast<uint64_t>(std::llround(std::max(0.0, delay)));
}

uint64_t presentedFromSource(const uint64_t sourcePositionMs, const uint64_t outputDelayMs,
                             const double delayToSourceScale)
{
    return saturatingSub(sourcePositionMs, delayInSourceTimelineMs(outputDelayMs, delayToSourceScale));
}
} // namespace

namespace Fooyin {
AudioClock::AudioClock()
    : m_state{State::Stopped}
    , m_hasAnchor{}
    , m_anchorPosMs{0.0}
    , m_hasLastSync{}
    , m_lastOutputMs{0.0}
    , m_effectiveRate{1.0}
    , m_rateTrim{0.0}
    , m_rateUsed{1.0}
{ }

void AudioClock::reset()
{
    m_state         = State::Stopped;
    m_hasAnchor     = false;
    m_anchorPosMs   = 0.0;
    m_anchorTime    = {};
    m_hasLastSync   = false;
    m_lastSyncTime  = {};
    m_lastOutputMs  = 0.0;
    m_effectiveRate = 1.0;
    m_rateTrim      = 0.0;
    m_rateUsed      = 1.0;
}

void AudioClock::setStopped()
{
    reset();
}

void AudioClock::setPaused(const TimePoint now)
{
    const double currentPos = predictedPosition(now);
    reanchor(currentPos, now);

    m_state        = State::Paused;
    m_lastOutputMs = currentPos;
    m_hasLastSync  = false;
}

void AudioClock::setPlaying(const TimePoint now, const uint64_t fallbackPositionMs)
{
    const double currentPos = m_hasAnchor ? predictedPosition(now) : static_cast<double>(fallbackPositionMs);
    reanchor(currentPos, now);

    m_state        = State::Playing;
    m_lastOutputMs = currentPos;
    m_hasLastSync  = false;
}

uint64_t AudioClock::applyAuthoritative(const uint64_t sourcePositionMs, const uint64_t outputDelayMs,
                                        const double delayToSourceScale, const TimePoint now, const UpdateMode mode)
{
    m_effectiveRate = clampEffectiveRate(delayToSourceScale);

    const auto targetPresented
        = static_cast<double>(presentedFromSource(sourcePositionMs, outputDelayMs, delayToSourceScale));

    if(mode == UpdateMode::Discontinuity || !m_hasAnchor || m_state != State::Playing) {
        m_rateTrim = 0.0;
        m_rateUsed = m_effectiveRate;
        reanchor(targetPresented, now);
        m_lastOutputMs = targetPresented;
        m_lastSyncTime = now;
        m_hasLastSync  = true;
        return toPositionMs(targetPresented);
    }

    const double dtSeconds
        = (m_hasLastSync && now > m_lastSyncTime) ? std::chrono::duration<double>(now - m_lastSyncTime).count() : 0.0;
    m_lastSyncTime = now;
    m_hasLastSync  = true;

    m_rateUsed              = clampEffectiveRate(m_effectiveRate + m_rateTrim);
    const double predicted  = predictedPosition(now);
    const double errorMs    = targetPresented - predicted;
    const double absErrorMs = std::abs(errorMs);
    double outputPos        = predicted;

    if(absErrorMs > DeadbandMs && dtSeconds > 0.0) {
        const auto maxStep = MaxSlewMsPerSecond * dtSeconds;
        const auto step    = std::clamp(errorMs, -maxStep, maxStep);
        outputPos          = predicted + step;
    }

    if(dtSeconds > 0.0) {
        const auto errorSeconds = errorMs / 1000.0;
        const auto rateDelta    = (errorSeconds / RateTrimTauSeconds) * dtSeconds;
        m_rateTrim              = std::clamp(m_rateTrim + rateDelta, -MaxRateTrim, MaxRateTrim);
        m_rateUsed              = clampEffectiveRate(m_effectiveRate + m_rateTrim);
    }

    const auto maxAllowedPos = targetPresented + MaxLeadMs;
    outputPos                = std::min(outputPos, maxAllowedPos);
    outputPos                = std::max(outputPos, m_lastOutputMs);

    reanchor(outputPos, now);
    m_lastOutputMs = outputPos;
    return toPositionMs(outputPos);
}

uint64_t AudioClock::position(const TimePoint now) const
{
    double predicted = predictedPosition(now);

    if(m_state == State::Playing) {
        predicted = std::max(predicted, m_lastOutputMs);
    }

    return toPositionMs(predicted);
}

AudioClock::State AudioClock::state() const
{
    return m_state;
}

double AudioClock::effectiveRate() const
{
    return m_rateUsed;
}

double AudioClock::predictedPosition(const TimePoint now) const
{
    if(!m_hasAnchor) {
        return 0.0;
    }

    if(m_state != State::Playing) {
        return m_anchorPosMs;
    }

    const auto elapsed       = elapsedMs(now, m_anchorTime);
    const auto scaledElapsed = elapsed * m_rateUsed;
    return std::max(0.0, m_anchorPosMs + scaledElapsed);
}

void AudioClock::reanchor(const double positionMs, const TimePoint now)
{
    m_hasAnchor   = true;
    m_anchorPosMs = std::max(0.0, positionMs);
    m_anchorTime  = now;
}
} // namespace Fooyin
