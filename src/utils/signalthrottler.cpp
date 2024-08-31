/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <utils/signalthrottler.h>

#include <QTimerEvent>

namespace Fooyin {
SignalThrottler::SignalThrottler(QObject* parent)
    : QObject{parent}
    , m_timeout{50}
    , m_timerType{Qt::CoarseTimer}
    , m_pendingEmit{false}
{ }

SignalThrottler::~SignalThrottler()
{
    maybeEmitTriggered();
}

int SignalThrottler::timeout() const
{
    return m_timeout;
}

void SignalThrottler::setTimeout(int timeout)
{
    if(m_timeout == timeout) {
        return;
    }
    m_timeout = timeout;
    emit timeoutChanged(timeout);
}

void SignalThrottler::setTimeout(std::chrono::milliseconds timeout)
{
    setTimeout(static_cast<int>(timeout.count()));
}

Qt::TimerType SignalThrottler::timerType() const
{
    return m_timerType;
}

void SignalThrottler::setTimerType(Qt::TimerType timerType)
{
    if(m_timerType == timerType) {
        return;
    }
    m_timerType = timerType;
    emit timerTypeChanged(timerType);
}

void SignalThrottler::throttle()
{
    m_pendingEmit = true;

    if(!m_timer.isActive()) {
        m_timer.start(m_timeout, m_timerType, this);
    }
}

void SignalThrottler::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_timer.timerId()) {
        maybeEmitTriggered();
    }
    QObject::timerEvent(event);
}

void SignalThrottler::maybeEmitTriggered()
{
    if(m_pendingEmit) {
        emitTriggered();
    }
    else {
        m_timer.stop();
    }
}

void SignalThrottler::emitTriggered()
{
    m_pendingEmit = false;
    emit triggered();
}
} // namespace Fooyin
