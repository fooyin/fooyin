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

#include "positiontracker.h"

#include <QTimerEvent>

constexpr auto PositionSyncIntervalMs    = 50;
constexpr auto PositionPublishIntervalMs = 16;

namespace Fooyin {
PositionTracker::PositionTracker(QObject* parent)
    : QObject{parent}
    , m_lastReportedPositionMs{0}
    , m_syncTimerId{0}
    , m_publishTimerId{0}
{ }

void PositionTracker::start()
{
    if(m_syncTimerId == 0) {
        m_syncTimerId = startTimer(PositionSyncIntervalMs, Qt::CoarseTimer);
    }
    if(m_publishTimerId == 0) {
        m_publishTimerId = startTimer(PositionPublishIntervalMs, Qt::PreciseTimer);
    }
}

void PositionTracker::stop()
{
    if(m_syncTimerId != 0) {
        killTimer(m_syncTimerId);
        m_syncTimerId = 0;
    }
    if(m_publishTimerId != 0) {
        killTimer(m_publishTimerId);
        m_publishTimerId = 0;
    }
}

void PositionTracker::applyPosition(const uint64_t sourcePositionMs, const uint64_t outputDelayMs,
                                    const double delayToSourceScale, const AudioClock::UpdateMode mode,
                                    const bool emitNow)
{
    const auto now = AudioClock::Clock::now();
    const uint64_t presentationPosition
        = m_clock.applyAuthoritative(sourcePositionMs, outputDelayMs, delayToSourceScale, now, mode);

    if(emitNow) {
        emitPosition(presentationPosition);
    }
}

void PositionTracker::setPlaying(uint64_t currentPositionMs)
{
    const auto now = AudioClock::Clock::now();
    m_clock.setPlaying(now, currentPositionMs);
    emitPosition(m_clock.position(now));
}

void PositionTracker::setPaused()
{
    const auto now = AudioClock::Clock::now();
    m_clock.setPaused(now);
    emitPosition(m_clock.position(now));
}

void PositionTracker::setStopped()
{
    m_clock.setStopped();
}

uint64_t PositionTracker::position() const
{
    return m_lastReportedPositionMs.load(std::memory_order_acquire);
}

void PositionTracker::timerEvent(QTimerEvent* event)
{
    const int timerId = event->timerId();

    if(timerId == m_syncTimerId) {
        emit requestSyncPosition();
        return;
    }

    if(timerId == m_publishTimerId) {
        emitPosition(m_clock.position(AudioClock::Clock::now()));
        return;
    }

    QObject::timerEvent(event);
}

void PositionTracker::emitPosition(const uint64_t positionMs)
{
    if(m_lastReportedPositionMs.exchange(positionMs, std::memory_order_acq_rel) == positionMs) {
        return;
    }

    emit positionChanged(positionMs);
}
} // namespace Fooyin
