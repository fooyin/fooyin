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

#include "pipewirethreadloop.h"

#include "pipewireutils.h"

#include <QDebug>

namespace Fooyin::Pipewire {
ThreadLoopGuard::ThreadLoopGuard(PipewireThreadLoop* loop)
    : m_loop{loop}
{
    m_loop->lock();
}

ThreadLoopGuard::~ThreadLoopGuard()
{
    m_loop->unlock();
}

int PipewireThreadLoop::wait(int secs)
{
    return pw_thread_loop_timed_wait(m_loop.get(), secs);
}

PipewireThreadLoop::PipewireThreadLoop()
    : m_loop{pw_thread_loop_new("fooyin/pipewire", nullptr)}
{
    if(!m_loop) {
        qCWarning(PIPEWIRE) << "Could not create thread loop";
    }
}

pw_loop* PipewireThreadLoop::loop() const
{
    if(!m_loop) {
        return nullptr;
    }

    return pw_thread_loop_get_loop(m_loop.get());
}

bool PipewireThreadLoop::start()
{
    return pw_thread_loop_start(m_loop.get()) == 0;
}

void PipewireThreadLoop::stop()
{
    pw_thread_loop_stop(m_loop.get());
}

void PipewireThreadLoop::signal(bool accept)
{
    pw_thread_loop_signal(m_loop.get(), accept);
}

void PipewireThreadLoop::lock() const
{
    pw_thread_loop_lock(m_loop.get());
}

void PipewireThreadLoop::unlock() const
{
    pw_thread_loop_unlock(m_loop.get());
}
} // namespace Fooyin::Pipewire
