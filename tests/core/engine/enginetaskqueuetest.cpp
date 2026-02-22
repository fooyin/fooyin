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

#include "core/engine/control/enginetaskqueue.h"

#include <QCoreApplication>

#include <gtest/gtest.h>

namespace Fooyin::Testing {
namespace {
using TaskKind = EngineTaskQueue::TaskType;

QCoreApplication* ensureCoreApplication()
{
    if(auto* app = QCoreApplication::instance()) {
        return app;
    }

    static int argc{1};
    static char appName[] = "fooyin-enginetaskqueue-test";
    static char* argv[]   = {appName, nullptr};
    static QCoreApplication app{argc, argv};
    return &app;
}
} // namespace

TEST(EngineTaskQueueTest, BarrierDefersPipelineWakeAndTimerUntilBarrierClears)
{
    ensureCoreApplication();

    EngineTaskQueue queue;

    int pipelineWakeRuns{0};
    int timerRuns{0};
    int workerRuns{0};

    queue.setBarrier(true);
    queue.enqueue(TaskKind::PipelineWake, [&pipelineWakeRuns]() { ++pipelineWakeRuns; });
    queue.enqueue(TaskKind::PipelineWake, [&pipelineWakeRuns]() { ++pipelineWakeRuns; });
    queue.enqueue(TaskKind::Timer, [&timerRuns]() { ++timerRuns; }, 7);
    queue.enqueue(TaskKind::Timer, [&timerRuns]() { ++timerRuns; }, 7);
    queue.enqueue(TaskKind::Worker, [&workerRuns]() { ++workerRuns; });
    queue.drain();

    EXPECT_EQ(workerRuns, 1);
    EXPECT_EQ(pipelineWakeRuns, 0);
    EXPECT_EQ(timerRuns, 0);
    EXPECT_EQ(queue.size(), 2U);

    queue.setBarrier(false);
    queue.drain();

    EXPECT_EQ(workerRuns, 1);
    EXPECT_EQ(pipelineWakeRuns, 1);
    EXPECT_EQ(timerRuns, 1);
    EXPECT_EQ(queue.size(), 0U);
}

TEST(EngineTaskQueueTest, BarrierActivationDefersAlreadyQueuedPipelineAndTimerTasks)
{
    ensureCoreApplication();

    EngineTaskQueue queue;

    int pipelineWakeRuns{0};
    int timerRuns{0};
    int workerRuns{0};

    queue.enqueue(TaskKind::PipelineWake, [&pipelineWakeRuns]() { ++pipelineWakeRuns; });
    queue.enqueue(TaskKind::Timer, [&timerRuns]() { ++timerRuns; }, 17);

    queue.setBarrier(true);
    queue.enqueue(TaskKind::Worker, [&workerRuns]() { ++workerRuns; });
    queue.drain();

    EXPECT_EQ(workerRuns, 1);
    EXPECT_EQ(pipelineWakeRuns, 0);
    EXPECT_EQ(timerRuns, 0);
    EXPECT_EQ(queue.size(), 2U);

    queue.setBarrier(false);
    queue.drain();

    EXPECT_EQ(workerRuns, 1);
    EXPECT_EQ(pipelineWakeRuns, 1);
    EXPECT_EQ(timerRuns, 1);
    EXPECT_EQ(queue.size(), 0U);
}

TEST(EngineTaskQueueTest, BeginShutdownClearsPendingTasksAndRejectsNewTasks)
{
    ensureCoreApplication();

    EngineTaskQueue queue;

    int executed{0};
    queue.enqueue(TaskKind::Internal, [&executed]() { ++executed; });
    EXPECT_EQ(queue.size(), 1U);

    queue.beginShutdown();
    EXPECT_TRUE(queue.isShuttingDown());
    EXPECT_EQ(queue.size(), 0U);

    queue.enqueue(TaskKind::Internal, [&executed]() { ++executed; });
    queue.drain();

    EXPECT_EQ(executed, 0);
    EXPECT_EQ(queue.size(), 0U);
}

TEST(EngineTaskQueueTest, TimerCoalescingRespectsCoalesceKey)
{
    ensureCoreApplication();

    EngineTaskQueue queue;

    int timerKeyOneRuns{0};
    int timerKeyTwoRuns{0};

    queue.enqueue(TaskKind::Timer, [&timerKeyOneRuns]() { ++timerKeyOneRuns; }, 11);
    queue.enqueue(TaskKind::Timer, [&timerKeyOneRuns]() { ++timerKeyOneRuns; }, 11);
    queue.enqueue(TaskKind::Timer, [&timerKeyTwoRuns]() { ++timerKeyTwoRuns; }, 22);

    queue.drain();

    EXPECT_EQ(timerKeyOneRuns, 1);
    EXPECT_EQ(timerKeyTwoRuns, 1);
    EXPECT_EQ(queue.size(), 0U);
}
} // namespace Fooyin::Testing
