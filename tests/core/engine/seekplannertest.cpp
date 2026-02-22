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

#include "core/engine/control/seekplanner.h"

#include <gtest/gtest.h>

namespace Fooyin::Testing {
TEST(SeekPlannerTest, ReturnsNoOpWhenDecoderInvalid)
{
    const SeekPlanContext context{
        .decoderValid = false,
    };

    const SeekPlan plan = planSeek(context);
    EXPECT_EQ(plan.strategy, SeekStrategy::NoOp);
}

TEST(SeekPlannerTest, UsesSimpleSeekWhenNotPlaying)
{
    const SeekPlanContext context{
        .decoderValid = true,
        .isPlaying    = false,
    };

    const SeekPlan plan = planSeek(context);
    EXPECT_EQ(plan.strategy, SeekStrategy::SimpleSeek);
}

TEST(SeekPlannerTest, UsesSimpleSeekNearTrackEndWhenAutoCrossfadeWindowReached)
{
    const SeekPlanContext context{
        .decoderValid        = true,
        .isPlaying           = true,
        .crossfadeEnabled    = true,
        .trackDurationMs     = 10000,
        .autoCrossfadeOutMs  = 2000,
        .requestedPositionMs = 9001,
    };

    const SeekPlan plan = planSeek(context);
    EXPECT_EQ(plan.strategy, SeekStrategy::SimpleSeek);
}

TEST(SeekPlannerTest, UsesSimpleSeekWhenCurrentStreamMissing)
{
    const SeekPlanContext context{
        .decoderValid     = true,
        .isPlaying        = true,
        .crossfadeEnabled = true,
        .seekFadeOutMs    = 200,
        .seekFadeInMs     = 100,
        .hasCurrentStream = false,
    };

    const SeekPlan plan = planSeek(context);
    EXPECT_EQ(plan.strategy, SeekStrategy::SimpleSeek);
}

TEST(SeekPlannerTest, UsesSimpleSeekWhenCrossfadingDisabled)
{
    const SeekPlanContext context{
        .decoderValid     = true,
        .isPlaying        = true,
        .crossfadeEnabled = false,
        .seekFadeOutMs    = 200,
        .seekFadeInMs     = 100,
        .hasCurrentStream = true,
    };

    const SeekPlan plan = planSeek(context);
    EXPECT_EQ(plan.strategy, SeekStrategy::SimpleSeek);
}

TEST(SeekPlannerTest, PlansCrossfadeSeekWithReserveAndClampedFade)
{
    const SeekPlanContext context{
        .decoderValid           = true,
        .isPlaying              = true,
        .crossfadeEnabled       = true,
        .trackDurationMs        = 10000,
        .autoCrossfadeOutMs     = 2000,
        .seekFadeOutMs          = 120,
        .seekFadeInMs           = 80,
        .hasCurrentStream       = true,
        .requestedPositionMs    = 1000,
        .bufferedDurationMs     = 200,
        .decoderHighWatermarkMs = 150,
        .bufferLengthMs         = 400,
    };

    const SeekPlan plan = planSeek(context);
    EXPECT_EQ(plan.strategy, SeekStrategy::CrossfadeSeek);
    EXPECT_EQ(plan.reserveTargetMs, 150);
    EXPECT_EQ(plan.fadeOutDurationMs, 120);
    EXPECT_EQ(plan.fadeInDurationMs, 80);
}

TEST(SeekPlannerTest, FallsBackToSimpleSeekWhenInsufficientBufferedAudio)
{
    const SeekPlanContext context{
        .decoderValid           = true,
        .isPlaying              = true,
        .crossfadeEnabled       = true,
        .seekFadeOutMs          = 100,
        .seekFadeInMs           = 50,
        .hasCurrentStream       = true,
        .bufferedDurationMs     = 0,
        .decoderHighWatermarkMs = 100,
        .bufferLengthMs         = 400,
    };

    const SeekPlan plan = planSeek(context);
    EXPECT_EQ(plan.strategy, SeekStrategy::SimpleSeek);
    EXPECT_EQ(plan.reserveTargetMs, 100);
    EXPECT_EQ(plan.fadeOutDurationMs, 0);
    EXPECT_EQ(plan.fadeInDurationMs, 0);
}
} // namespace Fooyin::Testing
