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

#include "worker.h"

#include <QAbstractEventDispatcher>
#include <QThread>

namespace Fy::Utils {
Worker::Worker(QObject* parent)
    : QObject{parent}
    , m_state{Idle}
{ }

void Worker::stopThread()
{
    setState(Idle);
}

void Worker::closeThread()
{
    stopThread();
}

Worker::State Worker::state() const
{
    return m_state.load(std::memory_order_acquire);
}

void Worker::setState(State state)
{
    m_state.store(state, std::memory_order_release);
}

bool Worker::isRunning()
{
    return m_state == Running;
}

bool Worker::mayRun() const
{
    // Process event queue to check for stop signals
    auto* dispatcher = QThread::currentThread()->eventDispatcher();
    if(!dispatcher) {
        return false;
    }
    dispatcher->processEvents(QEventLoop::AllEvents);
    return m_state == Running;
}
} // namespace Fy::Utils
