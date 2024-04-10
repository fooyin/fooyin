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

#pragma once

#include <pipewire/thread-loop.h>

#include <memory>

namespace Fooyin::Pipewire {
class PipewireThreadLoop;

class ThreadLoopGuard
{
public:
    explicit ThreadLoopGuard(PipewireThreadLoop* loop);
    ~ThreadLoopGuard();

private:
    PipewireThreadLoop* m_loop;
};

class PipewireThreadLoop
{
public:
    PipewireThreadLoop();

    [[nodiscard]] pw_loop* loop() const;

    bool start();
    void stop();

    void signal(bool accept);
    int wait(int secs);

private:
    friend ThreadLoopGuard;

    void lock() const;
    void unlock() const;

    struct PwThreadLoopDeleter
    {
        void operator()(pw_thread_loop* loop)
        {
            if(loop) {
                pw_thread_loop_destroy(loop);
            }
        }
    };
    using PwThreadLoopUPtr = std::unique_ptr<pw_thread_loop, PwThreadLoopDeleter>;

    PwThreadLoopUPtr m_loop;
};
} // namespace Fooyin::Pipewire
