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

#include "framerate.h"

#include <QBasicTimer>
#include <QElapsedTimer>

namespace Fooyin::Gui {
class FrameTimer
{
public:
    void start(int fps, QObject* receiver)
    {
        m_intervalNs = 1'000'000'000 / std::clamp(fps, 1, FrameRate::maxFps());
        m_clock.start();
        scheduleNext(receiver);
    }

    void stop()
    {
        m_timer.stop();
        m_clock.invalidate();
    }

    [[nodiscard]] bool isActive() const
    {
        return m_timer.isActive();
    }

    [[nodiscard]] int timerId() const
    {
        return m_timer.timerId();
    }

    void scheduleNext(QObject* receiver)
    {
        if(!m_clock.isValid()) {
            return;
        }

        const qint64 elapsedNs  = m_clock.nsecsElapsed();
        const qint64 deadlineNs = ((elapsedNs / m_intervalNs) + 1) * m_intervalNs;
        const int delayMs       = static_cast<int>((deadlineNs - elapsedNs + 999'999) / 1'000'000);
        m_timer.start(delayMs, Qt::PreciseTimer, receiver);
    }

private:
    QBasicTimer m_timer;
    QElapsedTimer m_clock;
    qint64 m_intervalNs{0};
};
} // namespace Fooyin::Gui
