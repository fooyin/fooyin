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

#include "audioclock.h"

#include <QObject>

#include <atomic>

class QTimerEvent;

namespace Fooyin {
class PositionTracker : public QObject
{
    Q_OBJECT

public:
    explicit PositionTracker(QObject* parent = nullptr);

    void start();
    void stop();

    void applyPosition(uint64_t sourcePositionMs, uint64_t outputDelayMs, double delayToSourceScale,
                       AudioClock::UpdateMode mode, bool emitNow);

    void setPlaying(uint64_t currentPositionMs);
    void setPaused();
    void setStopped();

    [[nodiscard]] uint64_t position() const;

signals:
    void requestSyncPosition();
    void positionChanged(uint64_t positionMs);

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    void emitPosition(uint64_t positionMs);

    AudioClock m_clock;
    std::atomic<uint64_t> m_lastReportedPositionMs;
    int m_syncTimerId;
    int m_publishTimerId;
};
} // namespace Fooyin
