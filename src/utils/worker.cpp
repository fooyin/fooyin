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

#include <utils/worker.h>

namespace Fooyin {
Worker::Worker(QObject* parent)
    : QObject{parent}
    , m_state{Idle}
{ }

void Worker::stopThread()
{
    setState(Idle);
}

void Worker::pauseThread()
{
    setState(Paused);
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

bool Worker::mayRun() const
{
    return m_state == Running;
}
} // namespace Fooyin

#include "utils/moc_worker.cpp"
