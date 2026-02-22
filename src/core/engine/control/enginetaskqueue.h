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

#include "fycore_export.h"

#include <utils/compatutils.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>

class QObject;

namespace Fooyin {
/*!
 * Engine-thread task queue with wake-event integration and barrier deferral.
 *
 * `PipelineWake` and `Timer` tasks can be coalesced and deferred behind a
 * temporary barrier. Worker and internal tasks bypass barrier deferral.
 */
class FYCORE_EXPORT EngineTaskQueue
{
public:
    class ScopedBarrier
    {
    public:
        explicit ScopedBarrier(EngineTaskQueue& queue)
            : m_queue{&queue}
        {
            m_queue->setBarrier(true);
        }

        ~ScopedBarrier()
        {
            release();
        }

        ScopedBarrier(const ScopedBarrier&)            = delete;
        ScopedBarrier& operator=(const ScopedBarrier&) = delete;

        ScopedBarrier(ScopedBarrier&& other) noexcept
            : m_queue{other.m_queue}
        {
            other.m_queue = nullptr;
        }

        ScopedBarrier& operator=(ScopedBarrier&& other) noexcept
        {
            if(this == &other) {
                return *this;
            }

            release();
            m_queue       = other.m_queue;
            other.m_queue = nullptr;
            return *this;
        }

        void release()
        {
            if(!m_queue) {
                return;
            }

            m_queue->setBarrier(false);
            m_queue->clearWakePostedFlag();
            m_queue->postWakeIfNeeded();
            m_queue = nullptr;
        }

    private:
        EngineTaskQueue* m_queue;
    };

    enum class TaskType : uint8_t
    {
        PipelineWake = 0,
        Timer,
        Worker,
        Internal
    };

    struct Task
    {
        TaskType type{TaskType::PipelineWake};
        int coalesceKey{-1};
        MoveOnlyFunction<void()> fn;
    };

    explicit EngineTaskQueue(QObject* wakeTarget = nullptr, int wakeEventType = 0);
    [[nodiscard]] ScopedBarrier barrierScope()
    {
        return ScopedBarrier{*this};
    }

    //! Set wake target/event used when queue transitions from idle to runnable.
    void setWakeTarget(QObject* target, int wakeEventType = 0);
    //! Enable/disable barrier that defers coalesced wake/timer tasks.
    void setBarrier(bool active);

    //! Enter shutdown mode (reject new tasks, wake/drain safely).
    void beginShutdown();
    //! Drop queued and deferred tasks.
    void clear();
    //! Run queued tasks synchronously on caller thread until empty.
    void drain();
    //! Enqueue a task; coalesced kinds may replace older queued tasks with same key.
    void enqueue(TaskType type, MoveOnlyFunction<void()> task, int coalesceKey = -1);

    //! Clear wake-posted latch after external wake event has been handled.
    void clearWakePostedFlag();
    //! Post wake event if runnable tasks exist and no wake is currently posted.
    bool postWakeIfNeeded();

    [[nodiscard]] bool isBarrierActive() const;
    [[nodiscard]] bool isDraining() const;
    [[nodiscard]] bool isShuttingDown() const;
    [[nodiscard]] bool isWakePosted() const;
    [[nodiscard]] std::size_t size() const;

private:
    [[nodiscard]] bool hasRunnableTasksLocked() const;
    [[nodiscard]] static int defaultWakeEventType();
    [[nodiscard]] static bool isCoalescedTaskKind(TaskType type);
    [[nodiscard]] static bool isBarrierDeferredType(TaskType type);
    void postWake();

    mutable std::mutex m_mutex;
    std::deque<Task> m_tasks;         // Runnable tasks drained on task wake events
    std::deque<Task> m_deferredTasks; // Barrier-deferred PipelineWake/Timer tasks held until barrier release
    std::size_t m_droppedCount;
    std::size_t m_evictedCount;
    bool m_draining;
    bool m_wakePosted;

    // Atomics are for lock-free lifecycle/wake state queries; queue storage remains mutex-owned
    std::atomic<bool> m_shuttingDown;
    std::atomic<bool> m_barrierActive;
    std::atomic<QObject*> m_wakeTarget;
    std::atomic<int> m_wakeEventType;
};
} // namespace Fooyin
