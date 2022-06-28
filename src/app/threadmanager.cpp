/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "utils/worker.h"

#include <QThread>

struct ThreadManager::Private
{
    QList<QThread*> threads;
    QList<Worker*> workers;
};

ThreadManager::ThreadManager(QObject* parent)
    : QObject{parent}
    , p(std::make_unique<Private>())
{ }

ThreadManager::~ThreadManager() = default;

void ThreadManager::close()
{
    emit stop();

    for(const auto& thread : p->threads) {
        thread->quit();
        thread->wait();
    }
}

void ThreadManager::moveToNewThread(Worker* worker)
{
    auto* thread = new QThread(this);
    p->threads.append(thread);
    thread->start();

    worker->moveToThread(thread);
    connect(this, &ThreadManager::stop, worker, &Worker::stopThread);
    p->workers.append(worker);
}
