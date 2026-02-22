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

#include "enginetaskqueue.h"

#include <QCoreApplication>
#include <QEvent>
#include <QLoggingCategory>
#include <QObject>

#include <algorithm>
#include <utility>

Q_LOGGING_CATEGORY(ENGINE_TASK_QUEUE, "fy.engine.taskqueue")

constexpr auto MaxQueuedEngineTasks = 64;

namespace Fooyin {
EngineTaskQueue::EngineTaskQueue(QObject* wakeTarget, int wakeEventType)
    : m_droppedCount{0}
    , m_evictedCount{0}
    , m_draining{false}
    , m_wakePosted{false}
    , m_shuttingDown{false}
    , m_barrierActive{false}
    , m_wakeTarget{wakeTarget}
    , m_wakeEventType{wakeEventType > 0 ? wakeEventType : defaultWakeEventType()}
{ }

void EngineTaskQueue::setWakeTarget(QObject* target, int wakeEventType)
{
    m_wakeTarget.store(target, std::memory_order_release);
    m_wakeEventType.store(wakeEventType > 0 ? wakeEventType : defaultWakeEventType(), std::memory_order_release);
    postWakeIfNeeded();
}

void EngineTaskQueue::setBarrier(const bool active)
{
    const bool wasActive = m_barrierActive.exchange(active, std::memory_order_acq_rel);
    if(wasActive == active) {
        return;
    }

    if(active) {
        const std::scoped_lock lock{m_mutex};
        std::deque<Task> existingRunnableTasks;
        existingRunnableTasks.swap(m_tasks);

        std::deque<Task> stillRunnableTasks;
        std::deque<Task> newlyDeferredTasks;
        while(!existingRunnableTasks.empty()) {
            Task task = std::move(existingRunnableTasks.front());
            existingRunnableTasks.pop_front();

            if(isBarrierDeferredType(task.type)) {
                newlyDeferredTasks.push_back(std::move(task));
            }
            else {
                stillRunnableTasks.push_back(std::move(task));
            }
        }

        m_tasks = std::move(stillRunnableTasks);

        // Keep pre-barrier deferred tasks ahead of tasks enqueued after barrier activation
        while(!newlyDeferredTasks.empty()) {
            m_deferredTasks.push_front(std::move(newlyDeferredTasks.back()));
            newlyDeferredTasks.pop_back();
        }

        if(m_tasks.empty()) {
            m_wakePosted = false;
        }

        return;
    }

    {
        const std::scoped_lock lock{m_mutex};
        // Preserve deferred ordering and prioritise deferred tasks immediately
        // after barrier release by moving them to the front in original order
        while(!m_deferredTasks.empty()) {
            m_tasks.push_front(std::move(m_deferredTasks.back()));
            m_deferredTasks.pop_back();
        }
    }

    postWakeIfNeeded();
}

void EngineTaskQueue::beginShutdown()
{
    m_shuttingDown.store(true, std::memory_order_release);
    clear();
}

void EngineTaskQueue::clear()
{
    const std::scoped_lock lock{m_mutex};
    m_tasks.clear();
    m_deferredTasks.clear();
    m_draining   = false;
    m_wakePosted = false;
}

void EngineTaskQueue::enqueue(const TaskType type, MoveOnlyFunction<void()> task, const int coalesceKey)
{
    if(!task || m_shuttingDown.load(std::memory_order_acquire)) {
        return;
    }

    bool shouldPostWake{false};
    bool droppedIncoming{false};
    bool evictedExisting{false};
    std::size_t droppedCount{0};
    std::size_t evictedCount{0};
    TaskType evictedType{TaskType::Internal};

    {
        const std::scoped_lock lock{m_mutex};
        if(m_shuttingDown.load(std::memory_order_relaxed)) {
            return;
        }

        if(isCoalescedTaskKind(type)) {
            const auto eraseMatchingTask = [type, coalesceKey](std::deque<Task>& queue) {
                std::erase_if(queue, [type, coalesceKey](const Task& queuedTask) {
                    if(queuedTask.type != type) {
                        return false;
                    }
                    if(type == TaskType::Timer && coalesceKey >= 0 && queuedTask.coalesceKey >= 0) {
                        return queuedTask.coalesceKey == coalesceKey;
                    }
                    return true;
                });
            };

            eraseMatchingTask(m_tasks);
            eraseMatchingTask(m_deferredTasks);
        }

        const bool barrierActive = m_barrierActive.load(std::memory_order_relaxed);
        const bool deferIncoming = barrierActive && isBarrierDeferredType(type);
        auto& targetQueue        = deferIncoming ? m_deferredTasks : m_tasks;

        if((m_tasks.size() + m_deferredTasks.size()) >= MaxQueuedEngineTasks) {
            const auto eraseCoalesced = [](std::deque<Task>& queue, TaskType& removedType) -> bool {
                auto dropIt = std::ranges::find_if(
                    queue, [](const Task& queuedTask) { return isCoalescedTaskKind(queuedTask.type); });
                if(dropIt == queue.end()) {
                    return false;
                }
                removedType = static_cast<TaskType>(dropIt->type);
                queue.erase(dropIt);
                return true;
            };

            if(eraseCoalesced(m_deferredTasks, evictedType) || eraseCoalesced(m_tasks, evictedType)) {
                ++m_evictedCount;
                evictedExisting = true;
                evictedCount    = m_evictedCount;
            }
            else {
                ++m_droppedCount;
                droppedIncoming = true;
                droppedCount    = m_droppedCount;
            }
        }

        if(!droppedIncoming) {
            targetQueue.push_back(Task{.type = type, .coalesceKey = coalesceKey, .fn = std::move(task)});
            if(!m_wakePosted && hasRunnableTasksLocked()) {
                m_wakePosted   = true;
                shouldPostWake = true;
            }
        }
    }

    if(droppedIncoming && ((droppedCount % 64) == 1)) {
        qCWarning(ENGINE_TASK_QUEUE) << "Engine task queue saturated; dropping task type" << static_cast<int>(type)
                                     << "dropped=" << droppedCount;
    }
    if(evictedExisting && ((evictedCount % 64) == 1)) {
        qCInfo(ENGINE_TASK_QUEUE) << "Engine task queue saturated; evicting coalesced task type"
                                  << static_cast<int>(evictedType) << "for incoming type" << static_cast<int>(type)
                                  << "evicted=" << evictedCount;
    }

    if(shouldPostWake) {
        postWake();
    }
}

void EngineTaskQueue::drain()
{
    {
        const std::scoped_lock lock{m_mutex};
        if(m_draining) {
            return;
        }
        m_draining = true;
    }

    while(true) {
        Task task;
        {
            const std::scoped_lock lock{m_mutex};
            if(m_tasks.empty()) {
                m_wakePosted = false;
                break;
            }

            task = std::move(m_tasks.front());
            m_tasks.pop_front();
        }

        if(!task.fn) {
            continue;
        }

        if(m_shuttingDown.load(std::memory_order_acquire)) {
            continue;
        }
        task.fn();
    }

    {
        const std::scoped_lock lock{m_mutex};
        m_draining = false;
    }
}

void EngineTaskQueue::clearWakePostedFlag()
{
    const std::scoped_lock lock{m_mutex};
    m_wakePosted = false;
}

bool EngineTaskQueue::postWakeIfNeeded()
{
    if(m_shuttingDown.load(std::memory_order_acquire)) {
        return false;
    }

    bool shouldPost{false};
    {
        const std::scoped_lock lock{m_mutex};
        if(!hasRunnableTasksLocked()) {
            return false;
        }
        if(!m_wakePosted) {
            m_wakePosted = true;
            shouldPost   = true;
        }
    }

    if(!shouldPost) {
        return false;
    }

    postWake();
    return true;
}

bool EngineTaskQueue::isBarrierActive() const
{
    return m_barrierActive.load(std::memory_order_acquire);
}

bool EngineTaskQueue::isDraining() const
{
    const std::scoped_lock lock{m_mutex};
    return m_draining;
}

bool EngineTaskQueue::isShuttingDown() const
{
    return m_shuttingDown.load(std::memory_order_acquire);
}

bool EngineTaskQueue::isWakePosted() const
{
    const std::scoped_lock lock{m_mutex};
    return m_wakePosted;
}

std::size_t EngineTaskQueue::size() const
{
    const std::scoped_lock lock{m_mutex};
    return m_tasks.size() + m_deferredTasks.size();
}

int EngineTaskQueue::defaultWakeEventType()
{
    // Shared queue wake event type for all EngineTaskQueue instances.
    static const int eventType = QEvent::registerEventType();
    return eventType;
}

bool EngineTaskQueue::isCoalescedTaskKind(const TaskType kind)
{
    switch(kind) {
        case TaskType::PipelineWake:
        case TaskType::Timer:
            return true;
        case TaskType::Worker:
        case TaskType::Internal:
            return false;
    }

    return false;
}

bool EngineTaskQueue::isBarrierDeferredType(const TaskType kind)
{
    switch(kind) {
        case TaskType::PipelineWake:
        case TaskType::Timer:
            return true;
        case TaskType::Worker:
        case TaskType::Internal:
            return false;
    }

    return false;
}

bool EngineTaskQueue::hasRunnableTasksLocked() const
{
    return !m_tasks.empty();
}

void EngineTaskQueue::postWake()
{
    auto* target = m_wakeTarget.load(std::memory_order_acquire);
    if(!target) {
        clearWakePostedFlag();
        return;
    }

    const int wakeEventType = m_wakeEventType.load(std::memory_order_acquire);
    QCoreApplication::postEvent(target, new QEvent(static_cast<QEvent::Type>(wakeEventType)));
}
} // namespace Fooyin
