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

#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>

namespace Fy::Utils {
template <typename QueueItem>
class ThreadQueue
{
public:
    ThreadQueue(bool blocking = true);

    ThreadQueue(ThreadQueue&& other);
    ThreadQueue& operator=(ThreadQueue const& other);
    ThreadQueue& operator=(ThreadQueue&& other);

    void clear();

    bool empty() const;
    std::size_t size() const;

    void enque(QueueItem item);
    QueueItem& front();
    QueueItem dequeue();

private:
    mutable std::mutex m_mtx;
    std::condition_variable m_cv;
    std::deque<QueueItem> m_queue;
    bool m_blocking;
};

template <typename QueueItem>
ThreadQueue<QueueItem>::ThreadQueue(bool blocking)
    : m_blocking{blocking}
{ }

template <typename QueueItem>
ThreadQueue<QueueItem>::ThreadQueue(ThreadQueue&& other)
{
    std::lock_guard lock(other.m_mtx);
    m_queue = std::move(other.m_queue);
}

template <typename QueueItem>
ThreadQueue<QueueItem>& ThreadQueue<QueueItem>::ThreadQueue::operator=(ThreadQueue const& other)
{
    std::lock_guard lock(m_mtx);
    std::lock_guard otherLock(other.m_mtx);

    m_queue = other.m_queue;

    return *this;
}

template <typename QueueItem>
void ThreadQueue<QueueItem>::clear()
{
    std::lock_guard lock(m_mtx);
    m_queue.clear();
}

template <typename QueueItem>
ThreadQueue<QueueItem>& ThreadQueue<QueueItem>::ThreadQueue::operator=(ThreadQueue&& other)
{
    std::lock_guard lock(m_mtx);
    std::lock_guard otherLock(other.m_mtx);

    m_queue = std::move(other.m_queue);

    return *this;
}

template <typename QueueItem>
bool ThreadQueue<QueueItem>::ThreadQueue::empty() const
{
    std::lock_guard lock(m_mtx);
    return m_queue.empty();
}

template <typename QueueItem>
std::size_t ThreadQueue<QueueItem>::ThreadQueue::size() const
{
    std::lock_guard lock(m_mtx);
    return m_queue.size();
}

template <typename QueueItem>
void ThreadQueue<QueueItem>::ThreadQueue::enque(QueueItem item)
{
    {
        std::lock_guard lock(m_mtx);
        m_queue.push_back(std::move(item));
    }

    m_cv.notify_one();
}

template <typename QueueItem>
QueueItem& ThreadQueue<QueueItem>::ThreadQueue::front()
{
    std::unique_lock lock(m_mtx);

    if(m_blocking) {
        while(m_queue.empty()) {
            m_cv.wait(lock);
        }
    }

    return m_queue.front();
}

template <typename QueueItem>
QueueItem ThreadQueue<QueueItem>::ThreadQueue::dequeue()
{
    std::unique_lock lock(m_mtx);

    if(m_blocking) {
        while(m_queue.empty()) {
            m_cv.wait(lock);
        }
    }

    QueueItem item = m_queue.front();
    m_queue.pop_front();
    return item;
}
} // namespace Fy::Utils
