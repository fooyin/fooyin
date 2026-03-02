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

#include <utils/compatutils.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>

namespace Fooyin {
class AudioPipeline;

class PipelineThreadHost
{
public:
    using PipelineCommand = MoveOnlyFunction<void(AudioPipeline*)>;

    PipelineThreadHost();

    [[nodiscard]] bool isRunning() const;
    void setRunning(bool running);
    [[nodiscard]] bool isShutdownRequested() const;
    void setShutdownRequested(bool shutdownRequested);
    void markAudioThreadReady(uint64_t threadIdHash);
    void clearAudioThreadIdentity();

    template <typename Fn>
        requires std::invocable<Fn>
    void start(Fn&& threadFunc)
    {
        m_thread = std::thread(std::forward<Fn>(threadFunc));
    }
    [[nodiscard]] bool joinable() const;
    void join();

    [[nodiscard]] bool enqueueCommand(PipelineCommand cmd, size_t maxQueuedCommands) const;
    [[nodiscard]] bool dequeueCommands(std::deque<PipelineCommand>& commandsOut, size_t maxCommandsPerCycle);

    void clearCommands();
    void wake() const;

    template <typename Rep, typename Period>
    void waitFor(const std::chrono::duration<Rep, Period>& duration)
    {
        const uint64_t wakeSnapshot = m_wakeSequence.load(std::memory_order_relaxed);
        std::unique_lock lock{m_wakeMutex};
        m_wakeCondition.wait_for(lock, duration, [this, wakeSnapshot]() {
            return m_shutdownRequested.load(std::memory_order_relaxed)
                || m_wakeSequence.load(std::memory_order_relaxed) != wakeSnapshot
                || m_queuedCommandCount.load(std::memory_order_relaxed) > 0;
        });
    }

    static uint64_t currentThreadIdHash();
    [[nodiscard]] bool isAudioThread() const;

private:
    std::thread m_thread;
    std::atomic<uint64_t> m_audioThreadIdHash;
    std::atomic<bool> m_audioThreadReady;
    std::atomic<bool> m_running;
    std::atomic<bool> m_shutdownRequested;
    mutable std::atomic<uint64_t> m_wakeSequence;

    mutable std::mutex m_commandMutex;
    std::mutex m_wakeMutex;
    mutable std::condition_variable m_wakeCondition;
    mutable std::deque<PipelineCommand> m_commandQueue;
    mutable std::atomic<size_t> m_queuedCommandCount;
};
} // namespace Fooyin
