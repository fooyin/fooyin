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

#include "core/engine/control/trackloadplanner.h"

#include <gtest/gtest.h>

namespace Fooyin::Testing {
TEST(TrackLoadPlannerTest, PrefersSegmentSwitchAndKeepsAutoFallback)
{
    const TrackLoadContext context{
        .manualChange                = false,
        .isPlaying                   = true,
        .decoderValid                = true,
        .hasPendingTransportFade     = false,
        .isContiguousSameFileSegment = true,
    };

    const LoadPlan plan = planTrackLoad(context);

    EXPECT_EQ(plan.firstStrategy, LoadStrategy::SegmentSwitch);
    EXPECT_TRUE(plan.trySegmentSwitch);
    EXPECT_FALSE(plan.tryManualFadeDefer);
    EXPECT_TRUE(plan.tryAutoTransition);
}

TEST(TrackLoadPlannerTest, PrefersManualFadeDeferForManualChange)
{
    const TrackLoadContext context{
        .manualChange                = true,
        .isPlaying                   = true,
        .decoderValid                = true,
        .hasPendingTransportFade     = false,
        .isContiguousSameFileSegment = true,
    };

    const LoadPlan plan = planTrackLoad(context);

    EXPECT_EQ(plan.firstStrategy, LoadStrategy::ManualFadeDefer);
    EXPECT_FALSE(plan.trySegmentSwitch);
    EXPECT_TRUE(plan.tryManualFadeDefer);
    EXPECT_FALSE(plan.tryAutoTransition);
}

TEST(TrackLoadPlannerTest, PrefersAutoTransitionWhenEligible)
{
    const TrackLoadContext context{
        .manualChange                = false,
        .isPlaying                   = false,
        .decoderValid                = false,
        .hasPendingTransportFade     = false,
        .isContiguousSameFileSegment = false,
    };

    const LoadPlan plan = planTrackLoad(context);

    EXPECT_EQ(plan.firstStrategy, LoadStrategy::AutoTransition);
    EXPECT_FALSE(plan.trySegmentSwitch);
    EXPECT_FALSE(plan.tryManualFadeDefer);
    EXPECT_TRUE(plan.tryAutoTransition);
}

TEST(TrackLoadPlannerTest, FallsBackToFullReinitWhenPendingTransportFadeBlocksAuto)
{
    const TrackLoadContext context{
        .manualChange                = false,
        .isPlaying                   = false,
        .decoderValid                = false,
        .hasPendingTransportFade     = true,
        .isContiguousSameFileSegment = false,
    };

    const LoadPlan plan = planTrackLoad(context);

    EXPECT_EQ(plan.firstStrategy, LoadStrategy::FullReinit);
    EXPECT_FALSE(plan.trySegmentSwitch);
    EXPECT_FALSE(plan.tryManualFadeDefer);
    EXPECT_FALSE(plan.tryAutoTransition);
}
} // namespace Fooyin::Testing
