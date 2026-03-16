/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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
    , m_closing{false}
{ }

void Worker::initialiseThread()
{
    m_closing.store(false, std::memory_order_release);
    resetStopSource();
}

void Worker::stopThread()
{
    requestStop();
    setState(Idle);
}

void Worker::pauseThread()
{
    requestStop();
    setState(Paused);
}

void Worker::closeThread()
{
    requestStop();
    m_closing.store(true, std::memory_order_release);
}

Worker::State Worker::state() const
{
    return m_state.load(std::memory_order_acquire);
}

void Worker::setState(State state)
{
    if(state == Running) {
        resetStopSource();
    }

    m_state.store(state, std::memory_order_release);
}

bool Worker::mayRun() const
{
    return state() == Running && !closing() && !stopRequested();
}

bool Worker::closing() const
{
    return m_closing.load(std::memory_order_acquire);
}

std::stop_token Worker::stopToken() const
{
    return m_stopSource.get_token();
}

bool Worker::stopRequested() const
{
    return stopToken().stop_requested();
}

void Worker::resetStopSource()
{
    m_stopSource = std::stop_source{};
}

void Worker::requestStop()
{
    m_stopSource.request_stop();
}
} // namespace Fooyin

#include "utils/moc_worker.cpp"
