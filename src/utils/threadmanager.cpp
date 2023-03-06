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

#include "threadmanager.h"

#include "worker.h"

#include <QThread>

namespace Fy::Utils {
ThreadManager::ThreadManager(QObject* parent)
    : QObject{parent}
{ }

void ThreadManager::shutdown()
{
    emit closeThread();

    for(const auto& thread : m_threads) {
        thread->quit();
        thread->wait();
    }
}

void ThreadManager::moveToNewThread(Worker* worker)
{
    auto* thread = new QThread(this);
    m_threads.emplace_back(thread);
    thread->start();

    worker->moveToThread(thread);
    connect(this, &ThreadManager::closeThread, worker, &Worker::closeThread);
    m_workers.emplace_back(worker);
}
} // namespace Fy::Utils
