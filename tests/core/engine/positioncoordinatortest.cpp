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

#include "core/engine/control/positioncoordinator.h"

#include <gtest/gtest.h>

namespace Fooyin::Testing {
TEST(PositionCoordinatorTest, GaplessHoldPinsProgressAtZeroBeforeFallbackWindow)
{
    PositionCoordinator coordinator;
    coordinator.setGaplessHold(7);

    PositionCoordinator::Input input;
    input.playbackState      = Engine::PlaybackState::Playing;
    input.streamId           = 7;
    input.streamState        = AudioStream::State::Playing;
    input.streamPositionMs   = 200;
    input.pipelineDelayMs    = 100;
    input.delayToSourceScale = 1.0;

    const auto output = coordinator.evaluate(input);
    EXPECT_TRUE(output.positionAvailable);
    EXPECT_TRUE(output.discontinuity);
    EXPECT_EQ(output.relativePosMs, 0U);
}

TEST(PositionCoordinatorTest, GaplessHoldFallsBackWhenRenderedMappingNeverArrives)
{
    PositionCoordinator coordinator;
    coordinator.setGaplessHold(7);

    PositionCoordinator::Input input;
    input.playbackState      = Engine::PlaybackState::Playing;
    input.streamId           = 7;
    input.streamState        = AudioStream::State::Playing;
    input.streamPositionMs   = 500;
    input.pipelineDelayMs    = 100;
    input.delayToSourceScale = 1.0;

    const auto recoveredOutput = coordinator.evaluate(input);
    EXPECT_TRUE(recoveredOutput.positionAvailable);
    EXPECT_FALSE(recoveredOutput.discontinuity);
    EXPECT_EQ(recoveredOutput.relativePosMs, 500U);

    input.streamPositionMs                            = 650;
    input.pipelineStatus.renderedSegment.valid        = true;
    input.pipelineStatus.renderedSegment.streamId     = 7;
    input.pipelineStatus.renderedSegment.sourceEndMs  = 650;
    input.pipelineStatus.renderedSegment.outputFrames = 1;

    const auto steadyStateOutput = coordinator.evaluate(input);
    EXPECT_TRUE(steadyStateOutput.positionAvailable);
    EXPECT_EQ(steadyStateOutput.relativePosMs, 650U);
}
} // namespace Fooyin::Testing
