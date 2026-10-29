/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <luket@pm.me>
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
    , m_activeStopToken{m_stopSource.get_token()}
{
    qRegisterMetaType<std::stop_token>();
}

void Worker::initialiseThread() { }

void Worker::stopThread()
{
    requestStop(Idle);
}

void Worker::pauseThread()
{
    requestStop(Paused);
}

void Worker::closeThread()
{
    requestStop(Idle, true);
}

Worker::State Worker::state() const
{
    return m_state.load(std::memory_order_acquire);
}

void Worker::setState(State state)
{
    m_state.store(state, std::memory_order_release);
}

std::stop_token Worker::prepareWork()
{
    const std::scoped_lock lock{m_stopSourceMutex};
    if(!closing() && !m_stopSource.stop_possible()) {
        m_stopSource = std::stop_source{};
    }
    return m_stopSource.get_token();
}

bool Worker::startWork(std::stop_token token)
{
    const std::scoped_lock lock{m_stopSourceMutex};
    if(closing() || !token.stop_possible() || token.stop_requested() || token != m_stopSource.get_token()) {
        return false;
    }
    m_activeStopToken = token;
    setState(Running);
    return true;
}

bool Worker::mayRun() const
{
    return mayRun(m_activeStopToken);
}

bool Worker::mayRun(std::stop_token token) const
{
    return state() == Running && !closing() && !token.stop_requested();
}

bool Worker::closing() const
{
    return m_closing.load(std::memory_order_acquire);
}

std::stop_token Worker::stopToken() const
{
    return m_activeStopToken;
}

bool Worker::stopRequested() const
{
    return closing() || m_activeStopToken.stop_requested();
}

void Worker::requestStop(State state, bool close)
{
    std::stop_source source{std::nostopstate};
    {
        const std::scoped_lock lock{m_stopSourceMutex};
        setState(state);
        if(close) {
            m_closing.store(true, std::memory_order_release);
        }
        source = std::exchange(m_stopSource, std::stop_source{std::nostopstate});
    }
    source.request_stop();
}

} // namespace Fooyin

#include "utils/moc_worker.cpp"
