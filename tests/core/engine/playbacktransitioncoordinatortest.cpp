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

#include "core/engine/control/playbacktransitioncoordinator.h"

#include <gtest/gtest.h>

namespace Fooyin::Testing {

TEST(PlaybackTransitionCoordinatorTest, MarksReadyAndEndsTrack)
{
    PlaybackTransitionCoordinator state;

    PlaybackTransitionCoordinator::TrackEndingInput input;
    input.positionMs             = 9600;
    input.durationMs             = 10000;
    input.remainingOutputMs      = 600;
    input.endOfInput             = false;
    input.bufferEmpty            = false;
    input.autoCrossfadeEnabled   = true;
    input.gaplessEnabled         = false;
    input.autoFadeOutMs          = 500;
    input.autoFadeInMs           = 500;
    input.gaplessPrepareWindowMs = 300;

    const auto aboutToFinish = state.evaluateTrackEnding(input);
    EXPECT_TRUE(aboutToFinish.aboutToFinish);
    EXPECT_TRUE(aboutToFinish.readyToSwitch);
    EXPECT_FALSE(aboutToFinish.endReached);
    EXPECT_TRUE(state.isReadyForAutoTransition());
    EXPECT_TRUE(state.isReadyForAutoCrossfade());
    EXPECT_FALSE(state.isReadyForGaplessHandoff());
    EXPECT_EQ(state.autoTransitionMode(), PlaybackTransitionCoordinator::AutoTransitionMode::Crossfade);

    input.endOfInput      = true;
    input.bufferEmpty     = false;
    const auto pendingEnd = state.evaluateTrackEnding(input);
    EXPECT_FALSE(pendingEnd.aboutToFinish);
    EXPECT_FALSE(pendingEnd.endReached);

    input.bufferEmpty = true;
    const auto ended  = state.evaluateTrackEnding(input);
    EXPECT_FALSE(ended.aboutToFinish);
    EXPECT_TRUE(ended.endReached);
    EXPECT_FALSE(state.isReadyForAutoTransition());
}

TEST(PlaybackTransitionCoordinatorTest, ExposesPendingSeekWhenBuffered)
{
    PlaybackTransitionCoordinator state;
    state.beginPendingSeek(1500, 400, 200, 42);

    EXPECT_TRUE(state.hasPendingSeek());
    EXPECT_FALSE(state.pendingSeekIfBuffered(399).has_value());

    const auto ready = state.pendingSeekIfBuffered(400);
    ASSERT_TRUE(ready.has_value());
    EXPECT_EQ(ready->seekPositionMs, 1500);
    EXPECT_EQ(ready->fadeOutDurationMs, 400);
    EXPECT_EQ(ready->fadeInDurationMs, 200);
    EXPECT_EQ(ready->transitionId, 42);
}

TEST(PlaybackTransitionCoordinatorTest, CancelPendingSeekClearsPendingState)
{
    PlaybackTransitionCoordinator state;
    state.beginPendingSeek(1000, 300, 150, 7);
    EXPECT_TRUE(state.hasPendingSeek());
    state.cancelPendingSeek();
    EXPECT_FALSE(state.hasPendingSeek());
    EXPECT_FALSE(state.pendingSeekIfBuffered(300).has_value());
}

TEST(PlaybackTransitionCoordinatorTest, EndOfInputDoesNotBecomeReadyUntilRemainingThreshold)
{
    PlaybackTransitionCoordinator state;

    PlaybackTransitionCoordinator::TrackEndingInput input;
    input.positionMs             = 0;
    input.durationMs             = 0;
    input.remainingOutputMs      = 1000;
    input.endOfInput             = true;
    input.bufferEmpty            = false;
    input.autoCrossfadeEnabled   = true;
    input.gaplessEnabled         = false;
    input.autoFadeOutMs          = 500;
    input.autoFadeInMs           = 500;
    input.gaplessPrepareWindowMs = 300;

    const auto aboutToFinish = state.evaluateTrackEnding(input);
    EXPECT_FALSE(aboutToFinish.aboutToFinish);
    EXPECT_FALSE(aboutToFinish.readyToSwitch);
    EXPECT_FALSE(aboutToFinish.endReached);
    EXPECT_FALSE(state.isReadyForAutoTransition());

    input.remainingOutputMs = 500;
    const auto ready        = state.evaluateTrackEnding(input);
    EXPECT_TRUE(ready.aboutToFinish);
    EXPECT_TRUE(ready.readyToSwitch);
    EXPECT_FALSE(ready.endReached);
    EXPECT_TRUE(state.isReadyForAutoCrossfade());
    EXPECT_EQ(state.autoTransitionMode(), PlaybackTransitionCoordinator::AutoTransitionMode::Crossfade);

    input.bufferEmpty = true;
    const auto ended  = state.evaluateTrackEnding(input);
    EXPECT_FALSE(ended.aboutToFinish);
    EXPECT_TRUE(ended.endReached);
    EXPECT_FALSE(state.isReadyForAutoTransition());
}

TEST(PlaybackTransitionCoordinatorTest, GaplessReadinessIsIndependentFromCrossfadeReadiness)
{
    PlaybackTransitionCoordinator state;

    PlaybackTransitionCoordinator::TrackEndingInput input;
    input.positionMs             = 9750;
    input.durationMs             = 10000;
    input.remainingOutputMs      = 250;
    input.endOfInput             = false;
    input.bufferEmpty            = false;
    input.autoCrossfadeEnabled   = false;
    input.gaplessEnabled         = true;
    input.autoFadeOutMs          = 0;
    input.autoFadeInMs           = 0;
    input.gaplessPrepareWindowMs = 300;

    const auto aboutToFinish = state.evaluateTrackEnding(input);
    EXPECT_TRUE(aboutToFinish.aboutToFinish);
    EXPECT_TRUE(aboutToFinish.readyToSwitch);
    EXPECT_FALSE(aboutToFinish.endReached);
    EXPECT_TRUE(state.isReadyForAutoTransition());
    EXPECT_FALSE(state.isReadyForAutoCrossfade());
    EXPECT_TRUE(state.isReadyForGaplessHandoff());
    EXPECT_EQ(state.autoTransitionMode(), PlaybackTransitionCoordinator::AutoTransitionMode::Gapless);
}

TEST(PlaybackTransitionCoordinatorTest, BoundaryFadeReadinessSignalsAboutToFinishButNotReadyToSwitch)
{
    PlaybackTransitionCoordinator state;

    PlaybackTransitionCoordinator::TrackEndingInput input;
    input.positionMs             = 9500;
    input.durationMs             = 10000;
    input.remainingOutputMs      = 700;
    input.endOfInput             = false;
    input.bufferEmpty            = false;
    input.autoCrossfadeEnabled   = false;
    input.boundaryFadeEnabled    = true;
    input.gaplessEnabled         = false;
    input.boundaryFadeOutMs      = 500;
    input.gaplessPrepareWindowMs = 300;

    const auto aboutToFinish = state.evaluateTrackEnding(input);
    EXPECT_TRUE(aboutToFinish.aboutToFinish);
    EXPECT_FALSE(aboutToFinish.readyToSwitch);
    EXPECT_FALSE(aboutToFinish.endReached);
    EXPECT_TRUE(state.isReadyForGaplessHandoff());
    EXPECT_EQ(state.autoTransitionMode(), PlaybackTransitionCoordinator::AutoTransitionMode::BoundaryFade);
}

TEST(PlaybackTransitionCoordinatorTest, BoundaryFadeModePreferredOverGaplessWhenBothReady)
{
    PlaybackTransitionCoordinator state;

    PlaybackTransitionCoordinator::TrackEndingInput input;
    input.positionMs             = 9700;
    input.durationMs             = 10000;
    input.remainingOutputMs      = 450;
    input.endOfInput             = false;
    input.bufferEmpty            = false;
    input.autoCrossfadeEnabled   = false;
    input.boundaryFadeEnabled    = true;
    input.gaplessEnabled         = true;
    input.boundaryFadeOutMs      = 200;
    input.gaplessPrepareWindowMs = 300;

    const auto aboutToFinish = state.evaluateTrackEnding(input);
    EXPECT_TRUE(aboutToFinish.aboutToFinish);
    EXPECT_FALSE(aboutToFinish.readyToSwitch);
    EXPECT_FALSE(aboutToFinish.endReached);
    EXPECT_TRUE(state.isReadyForGaplessHandoff());
    EXPECT_EQ(state.autoTransitionMode(), PlaybackTransitionCoordinator::AutoTransitionMode::BoundaryFade);
}

TEST(PlaybackTransitionCoordinatorTest, BoundaryFadeWithZeroFadeOutStillPreparesAsBoundaryFade)
{
    PlaybackTransitionCoordinator state;

    PlaybackTransitionCoordinator::TrackEndingInput input;
    input.positionMs             = 9700;
    input.durationMs             = 10000;
    input.remainingOutputMs      = 450;
    input.endOfInput             = false;
    input.bufferEmpty            = false;
    input.autoCrossfadeEnabled   = false;
    input.boundaryFadeEnabled    = true;
    input.gaplessEnabled         = false;
    input.boundaryFadeOutMs      = 0;
    input.gaplessPrepareWindowMs = 300;

    const auto aboutToFinish = state.evaluateTrackEnding(input);
    EXPECT_TRUE(aboutToFinish.aboutToFinish);
    EXPECT_FALSE(aboutToFinish.readyToSwitch);
    EXPECT_FALSE(aboutToFinish.endReached);
    EXPECT_TRUE(state.isReadyForGaplessHandoff());
    EXPECT_EQ(state.autoTransitionMode(), PlaybackTransitionCoordinator::AutoTransitionMode::BoundaryFade);
}

TEST(PlaybackTransitionCoordinatorTest, CrossfadeModeIsPreferredWhenGaplessBecomesReadyFirst)
{
    PlaybackTransitionCoordinator state;

    PlaybackTransitionCoordinator::TrackEndingInput input;
    input.positionMs             = 9700;
    input.durationMs             = 10000;
    input.remainingOutputMs      = 450;
    input.endOfInput             = false;
    input.bufferEmpty            = false;
    input.autoCrossfadeEnabled   = true;
    input.gaplessEnabled         = true;
    input.autoFadeOutMs          = 200;
    input.autoFadeInMs           = 200;
    input.gaplessPrepareWindowMs = 300;

    const auto aboutToFinish = state.evaluateTrackEnding(input);
    EXPECT_TRUE(aboutToFinish.aboutToFinish);
    EXPECT_FALSE(aboutToFinish.readyToSwitch);
    EXPECT_FALSE(aboutToFinish.endReached);
    EXPECT_TRUE(state.isReadyForAutoCrossfade());
    EXPECT_FALSE(state.isReadyForGaplessHandoff());
    EXPECT_EQ(state.autoTransitionMode(), PlaybackTransitionCoordinator::AutoTransitionMode::Crossfade);

    input.positionMs = 9800;
    const auto ready = state.evaluateTrackEnding(input);
    EXPECT_FALSE(ready.aboutToFinish);
    EXPECT_TRUE(ready.readyToSwitch);
    EXPECT_FALSE(ready.endReached);
}

TEST(PlaybackTransitionCoordinatorTest, GaplessTimelineWindowAccountsForOutputDelay)
{
    PlaybackTransitionCoordinator state;

    PlaybackTransitionCoordinator::TrackEndingInput input;
    input.positionMs             = 9200;
    input.durationMs             = 10000;
    input.remainingOutputMs      = 700;
    input.endOfInput             = false;
    input.bufferEmpty            = false;
    input.autoCrossfadeEnabled   = false;
    input.gaplessEnabled         = true;
    input.autoFadeOutMs          = 0;
    input.autoFadeInMs           = 0;
    input.gaplessPrepareWindowMs = 300;
    input.timelineDelayMs        = 400;

    const auto beforeWindow = state.evaluateTrackEnding(input);
    EXPECT_FALSE(beforeWindow.aboutToFinish);
    EXPECT_FALSE(beforeWindow.readyToSwitch);

    input.positionMs         = 9400;
    const auto aboutToFinish = state.evaluateTrackEnding(input);
    EXPECT_TRUE(aboutToFinish.aboutToFinish);
    EXPECT_TRUE(aboutToFinish.readyToSwitch);
    EXPECT_TRUE(state.isReadyForGaplessHandoff());
}

TEST(PlaybackTransitionCoordinatorTest, CrossfadeReadyWindowAccountsForOutputDelay)
{
    PlaybackTransitionCoordinator state;

    PlaybackTransitionCoordinator::TrackEndingInput input;
    input.positionMs             = 9200;
    input.durationMs             = 10000;
    input.remainingOutputMs      = 900;
    input.endOfInput             = false;
    input.bufferEmpty            = false;
    input.autoCrossfadeEnabled   = true;
    input.gaplessEnabled         = false;
    input.autoFadeOutMs          = 500;
    input.autoFadeInMs           = 200;
    input.gaplessPrepareWindowMs = 300;
    input.timelineDelayMs        = 400;

    const auto aboutToFinish = state.evaluateTrackEnding(input);
    EXPECT_TRUE(aboutToFinish.aboutToFinish);
    EXPECT_FALSE(aboutToFinish.readyToSwitch);
    EXPECT_TRUE(state.isReadyForAutoCrossfade());

    input.positionMs = 9400;
    const auto ready = state.evaluateTrackEnding(input);
    EXPECT_FALSE(ready.aboutToFinish);
    EXPECT_TRUE(ready.readyToSwitch);
}

TEST(PlaybackTransitionCoordinatorTest, CrossfadePreparationLeadQueuesBeforeOverlapWindow)
{
    PlaybackTransitionCoordinator state;

    PlaybackTransitionCoordinator::TrackEndingInput input;
    input.positionMs             = 7000;
    input.durationMs             = 10000;
    input.remainingOutputMs      = 3000;
    input.endOfInput             = false;
    input.bufferEmpty            = false;
    input.autoCrossfadeEnabled   = true;
    input.gaplessEnabled         = false;
    input.autoFadeOutMs          = 2000;
    input.autoFadeInMs           = 2000;
    input.autoPrepareLeadMs      = 1000;
    input.gaplessPrepareWindowMs = 300;

    const auto aboutToFinish = state.evaluateTrackEnding(input);
    EXPECT_TRUE(aboutToFinish.aboutToFinish);
    EXPECT_FALSE(aboutToFinish.readyToSwitch);
    EXPECT_TRUE(state.isReadyForAutoCrossfade());

    input.positionMs = 8000;
    const auto ready = state.evaluateTrackEnding(input);
    EXPECT_FALSE(ready.aboutToFinish);
    EXPECT_TRUE(ready.readyToSwitch);
}

TEST(PlaybackTransitionCoordinatorTest, EndOfInputWithoutTransitionsWaitsForBufferDrain)
{
    PlaybackTransitionCoordinator state;

    PlaybackTransitionCoordinator::TrackEndingInput input;
    input.remainingOutputMs    = 1000;
    input.endOfInput           = true;
    input.bufferEmpty          = false;
    input.autoCrossfadeEnabled = false;
    input.gaplessEnabled       = false;

    const auto pending = state.evaluateTrackEnding(input);
    EXPECT_TRUE(pending.aboutToFinish);
    EXPECT_FALSE(pending.endReached);

    input.bufferEmpty = true;
    const auto ended  = state.evaluateTrackEnding(input);
    EXPECT_FALSE(ended.aboutToFinish);
    EXPECT_TRUE(ended.endReached);
}

TEST(PlaybackTransitionCoordinatorTest, HardEndSignalsAndClearsReadiness)
{
    PlaybackTransitionCoordinator state;

    PlaybackTransitionCoordinator::TrackEndingInput input;
    input.endOfInput           = true;
    input.bufferEmpty          = true;
    input.autoCrossfadeEnabled = true;
    input.gaplessEnabled       = true;

    const auto ended = state.evaluateTrackEnding(input);
    EXPECT_FALSE(ended.aboutToFinish);
    EXPECT_TRUE(ended.endReached);
    EXPECT_FALSE(state.isReadyForAutoTransition());
}

TEST(PlaybackTransitionCoordinatorTest, GaplessWindowEmitsAboutToFinish)
{
    PlaybackTransitionCoordinator state;

    PlaybackTransitionCoordinator::TrackEndingInput input;
    input.positionMs             = 9900;
    input.durationMs             = 10000;
    input.remainingOutputMs      = 500;
    input.endOfInput             = false;
    input.bufferEmpty            = false;
    input.autoCrossfadeEnabled   = false;
    input.gaplessEnabled         = true;
    input.autoFadeOutMs          = 0;
    input.autoFadeInMs           = 0;
    input.gaplessPrepareWindowMs = 300;

    const auto result = state.evaluateTrackEnding(input);
    EXPECT_TRUE(result.aboutToFinish);
    EXPECT_TRUE(result.readyToSwitch);
    EXPECT_FALSE(result.endReached);
    EXPECT_TRUE(state.isReadyForGaplessHandoff());
}

TEST(PlaybackTransitionCoordinatorTest, DurationBoundaryWithoutEndOfInputEmitsAboutToFinish)
{
    PlaybackTransitionCoordinator state;

    PlaybackTransitionCoordinator::TrackEndingInput input;
    input.positionMs             = 5000;
    input.durationMs             = 5000;
    input.remainingOutputMs      = 250;
    input.endOfInput             = false;
    input.bufferEmpty            = false;
    input.autoCrossfadeEnabled   = false;
    input.gaplessEnabled         = false;
    input.autoFadeOutMs          = 0;
    input.autoFadeInMs           = 0;
    input.gaplessPrepareWindowMs = 300;

    const auto result = state.evaluateTrackEnding(input);
    EXPECT_TRUE(result.aboutToFinish);
    EXPECT_FALSE(result.readyToSwitch);
    EXPECT_TRUE(result.endReached);
    EXPECT_FALSE(state.isReadyForAutoTransition());
}

TEST(PlaybackTransitionCoordinatorTest, CrossfadeReadyToSwitchUsesMinOverlapWindow)
{
    PlaybackTransitionCoordinator state;

    PlaybackTransitionCoordinator::TrackEndingInput input;
    input.positionMs             = 9500;
    input.durationMs             = 10000;
    input.remainingOutputMs      = 700;
    input.endOfInput             = false;
    input.bufferEmpty            = false;
    input.autoCrossfadeEnabled   = true;
    input.gaplessEnabled         = false;
    input.autoFadeOutMs          = 500;
    input.autoFadeInMs           = 200;
    input.gaplessPrepareWindowMs = 300;

    const auto aboutToFinish = state.evaluateTrackEnding(input);
    EXPECT_TRUE(aboutToFinish.aboutToFinish);
    EXPECT_FALSE(aboutToFinish.readyToSwitch);
    EXPECT_FALSE(aboutToFinish.endReached);
    EXPECT_TRUE(state.isReadyForAutoCrossfade());

    input.positionMs = 9800;
    const auto ready = state.evaluateTrackEnding(input);
    EXPECT_FALSE(ready.aboutToFinish);
    EXPECT_TRUE(ready.readyToSwitch);
    EXPECT_FALSE(ready.endReached);
}

TEST(PlaybackTransitionCoordinatorTest, ZeroFadeInDefersSwitchUntilTrackBoundary)
{
    PlaybackTransitionCoordinator state;

    PlaybackTransitionCoordinator::TrackEndingInput input;
    input.positionMs             = 9500;
    input.durationMs             = 10000;
    input.remainingOutputMs      = 700;
    input.endOfInput             = false;
    input.bufferEmpty            = false;
    input.autoCrossfadeEnabled   = true;
    input.gaplessEnabled         = false;
    input.autoFadeOutMs          = 500;
    input.autoFadeInMs           = 0;
    input.gaplessPrepareWindowMs = 300;

    const auto aboutToFinish = state.evaluateTrackEnding(input);
    EXPECT_TRUE(aboutToFinish.aboutToFinish);
    EXPECT_FALSE(aboutToFinish.readyToSwitch);
    EXPECT_TRUE(state.isReadyForAutoCrossfade());

    input.positionMs = 10000;
    const auto ready = state.evaluateTrackEnding(input);
    EXPECT_FALSE(ready.aboutToFinish);
    EXPECT_TRUE(ready.readyToSwitch);
}

TEST(PlaybackTransitionCoordinatorTest, GaplessDrainWindowAccountsForOutputDelayWhenEndOfInput)
{
    PlaybackTransitionCoordinator state;

    PlaybackTransitionCoordinator::TrackEndingInput input;
    input.positionMs             = 0;
    input.durationMs             = 0;
    input.remainingOutputMs      = 801;
    input.endOfInput             = true;
    input.bufferEmpty            = false;
    input.autoCrossfadeEnabled   = false;
    input.gaplessEnabled         = true;
    input.autoFadeOutMs          = 0;
    input.autoFadeInMs           = 0;
    input.gaplessPrepareWindowMs = 300;
    input.timelineDelayMs        = 500;

    const auto aboutToFinish = state.evaluateTrackEnding(input);
    EXPECT_TRUE(aboutToFinish.aboutToFinish);
    EXPECT_FALSE(aboutToFinish.readyToSwitch);
    EXPECT_TRUE(state.isReadyForGaplessHandoff());

    input.remainingOutputMs = 800;
    const auto ready        = state.evaluateTrackEnding(input);
    EXPECT_FALSE(ready.aboutToFinish);
    EXPECT_TRUE(ready.readyToSwitch);
}

TEST(PlaybackTransitionCoordinatorTest, CrossfadeDrainWindowAccountsForOutputDelayWhenEndOfInput)
{
    PlaybackTransitionCoordinator state;

    PlaybackTransitionCoordinator::TrackEndingInput input;
    input.positionMs             = 0;
    input.durationMs             = 0;
    input.remainingOutputMs      = 901;
    input.endOfInput             = true;
    input.bufferEmpty            = false;
    input.autoCrossfadeEnabled   = true;
    input.gaplessEnabled         = false;
    input.autoFadeOutMs          = 500;
    input.autoFadeInMs           = 500;
    input.gaplessPrepareWindowMs = 300;
    input.timelineDelayMs        = 400;

    const auto pending = state.evaluateTrackEnding(input);
    EXPECT_FALSE(pending.aboutToFinish);
    EXPECT_FALSE(pending.readyToSwitch);
    EXPECT_FALSE(state.isReadyForAutoCrossfade());

    input.remainingOutputMs = 900;
    const auto ready        = state.evaluateTrackEnding(input);
    EXPECT_TRUE(ready.aboutToFinish);
    EXPECT_TRUE(ready.readyToSwitch);
    EXPECT_TRUE(state.isReadyForAutoCrossfade());
}

TEST(PlaybackTransitionCoordinatorTest, BoundaryReachedEmitsOnceAtDuration)
{
    PlaybackTransitionCoordinator state;

    PlaybackTransitionCoordinator::TrackEndingInput input;
    input.positionMs = 9999;
    input.durationMs = 10000;

    const auto beforeBoundary = state.evaluateTrackEnding(input);
    EXPECT_FALSE(beforeBoundary.boundaryReached);

    input.positionMs      = 10000;
    const auto atBoundary = state.evaluateTrackEnding(input);
    EXPECT_TRUE(atBoundary.boundaryReached);

    input.positionMs        = 10100;
    const auto pastBoundary = state.evaluateTrackEnding(input);
    EXPECT_FALSE(pastBoundary.boundaryReached);
}

TEST(PlaybackTransitionCoordinatorTest, DurationBoundaryIgnoredWhenDisabled)
{
    PlaybackTransitionCoordinator state;

    PlaybackTransitionCoordinator::TrackEndingInput input;
    input.positionMs              = 5000;
    input.durationMs              = 5000;
    input.durationBoundaryEnabled = false;
    input.remainingOutputMs       = 250;
    input.endOfInput              = false;
    input.bufferEmpty             = false;
    input.autoCrossfadeEnabled    = false;
    input.gaplessEnabled          = false;
    input.autoFadeOutMs           = 0;
    input.autoFadeInMs            = 0;
    input.gaplessPrepareWindowMs  = 300;

    const auto result = state.evaluateTrackEnding(input);
    EXPECT_FALSE(result.aboutToFinish);
    EXPECT_FALSE(result.readyToSwitch);
    EXPECT_FALSE(result.boundaryReached);
    EXPECT_FALSE(result.endReached);
}

TEST(PlaybackTransitionCoordinatorTest, TimelineHintsCanBeUsedWithoutDurationBoundary)
{
    PlaybackTransitionCoordinator state;

    PlaybackTransitionCoordinator::TrackEndingInput input;
    input.positionMs                     = 9900;
    input.durationMs                     = 10000;
    input.durationBoundaryEnabled        = false;
    input.predictiveTimelineHintsEnabled = true;
    input.remainingOutputMs              = 500;
    input.endOfInput                     = false;
    input.bufferEmpty                    = false;
    input.autoCrossfadeEnabled           = true;
    input.gaplessEnabled                 = false;
    input.autoFadeOutMs                  = 300;
    input.autoFadeInMs                   = 300;
    input.gaplessPrepareWindowMs         = 300;

    const auto result = state.evaluateTrackEnding(input);
    EXPECT_TRUE(result.aboutToFinish);
    EXPECT_TRUE(result.readyToSwitch);
    EXPECT_FALSE(result.boundaryReached);
    EXPECT_FALSE(result.endReached);
    EXPECT_TRUE(state.isReadyForAutoTransition());
}

TEST(PlaybackTransitionCoordinatorTest, TimelineHintsCanBeDisabledIndependentlyFromDurationBoundary)
{
    PlaybackTransitionCoordinator state;

    PlaybackTransitionCoordinator::TrackEndingInput input;
    input.positionMs                     = 9900;
    input.durationMs                     = 10000;
    input.durationBoundaryEnabled        = false;
    input.predictiveTimelineHintsEnabled = false;
    input.remainingOutputMs              = 500;
    input.endOfInput                     = false;
    input.bufferEmpty                    = false;
    input.autoCrossfadeEnabled           = true;
    input.gaplessEnabled                 = false;
    input.autoFadeOutMs                  = 300;
    input.autoFadeInMs                   = 300;
    input.gaplessPrepareWindowMs         = 300;

    const auto result = state.evaluateTrackEnding(input);
    EXPECT_FALSE(result.aboutToFinish);
    EXPECT_FALSE(result.readyToSwitch);
    EXPECT_FALSE(result.boundaryReached);
    EXPECT_FALSE(result.endReached);
    EXPECT_FALSE(state.isReadyForAutoTransition());
}

TEST(PlaybackTransitionCoordinatorTest, HardEndSignalsBoundaryWhenDurationUnknown)
{
    PlaybackTransitionCoordinator state;

    PlaybackTransitionCoordinator::TrackEndingInput input;
    input.endOfInput  = true;
    input.bufferEmpty = true;
    input.durationMs  = 0;

    const auto ended = state.evaluateTrackEnding(input);
    EXPECT_TRUE(ended.boundaryReached);
    EXPECT_TRUE(ended.endReached);
}

} // namespace Fooyin::Testing
