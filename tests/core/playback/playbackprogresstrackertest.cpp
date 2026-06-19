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

#include "core/playback/playbackprogresstracker.h"

#include <gtest/gtest.h>

namespace Fooyin::Testing {
TEST(PlaybackProgressTrackerTest, CountsPlaybackTimeAndPlayedThresholdOnce)
{
    PlaybackProgressTracker tracker;
    tracker.onTrackCommitted(1000, 0.5, 240000);

    const auto firstUpdate = tracker.updatePosition(100);
    EXPECT_EQ(firstUpdate.positionMs, 100);
    ASSERT_TRUE(firstUpdate.positionSeconds.has_value());
    EXPECT_EQ(*firstUpdate.positionSeconds, 0);
    EXPECT_FALSE(firstUpdate.reachedPlayedThreshold);
    EXPECT_EQ(tracker.timeListened(), 100);
    EXPECT_EQ(tracker.playedThreshold(), 400);

    const auto thresholdUpdate = tracker.updatePosition(500);
    EXPECT_EQ(thresholdUpdate.positionMs, 500);
    EXPECT_FALSE(thresholdUpdate.positionSeconds.has_value());
    EXPECT_TRUE(thresholdUpdate.reachedPlayedThreshold);
    EXPECT_EQ(tracker.timeListened(), 500);

    const auto finalUpdate = tracker.updatePosition(700);
    EXPECT_FALSE(finalUpdate.reachedPlayedThreshold);
    EXPECT_EQ(tracker.timeListened(), 700);
}

TEST(PlaybackProgressTrackerTest, SeekPreventsSkippedTimeFromCounting)
{
    PlaybackProgressTracker tracker;
    tracker.onTrackCommitted(2000, 0.5, 240000);

    const auto initialUpdate = tracker.updatePosition(100);
    EXPECT_EQ(initialUpdate.positionMs, 100);
    EXPECT_EQ(tracker.timeListened(), 100);

    EXPECT_TRUE(tracker.markSeekPosition(900));

    const auto afterSeek = tracker.updatePosition(950);
    EXPECT_FALSE(afterSeek.reachedPlayedThreshold);
    EXPECT_EQ(tracker.timeListened(), 100);

    const auto resumedUpdate = tracker.updatePosition(980);
    EXPECT_EQ(resumedUpdate.positionMs, 980);
    EXPECT_EQ(tracker.timeListened(), 130);
}

TEST(PlaybackProgressTrackerTest, ResetAndBitrateUpdatesBehaveAsExpected)
{
    PlaybackProgressTracker tracker;
    tracker.onTrackCommitted(3000, 0.5, 240000);
    const auto playbackUpdate = tracker.updatePosition(1200);
    EXPECT_EQ(playbackUpdate.positionMs, 1200);

    EXPECT_TRUE(tracker.updateBitrate(320));
    EXPECT_FALSE(tracker.updateBitrate(320));
    EXPECT_EQ(tracker.bitrate(), 320);

    const auto resetUpdate = tracker.resetPosition();
    EXPECT_EQ(resetUpdate.positionMs, 0);
    ASSERT_TRUE(resetUpdate.positionSeconds.has_value());
    EXPECT_EQ(*resetUpdate.positionSeconds, 0);
    EXPECT_EQ(tracker.position(), 0);

    tracker.reset();
    EXPECT_EQ(tracker.totalDuration(), 0);
    EXPECT_EQ(tracker.position(), 0);
    EXPECT_EQ(tracker.timeListened(), 0);
    EXPECT_EQ(tracker.playedThreshold(), 0);
    EXPECT_EQ(tracker.bitrate(), 320);
}

TEST(PlaybackProgressTrackerTest, RestoredProgressPreservesPartialThresholdProgress)
{
    PlaybackProgressTracker tracker;
    tracker.onTrackCommitted(1000, 0.5, 240000);

    const auto restoredUpdate = tracker.restoreProgress(500, 350);
    EXPECT_EQ(restoredUpdate.positionMs, 500);
    ASSERT_TRUE(restoredUpdate.positionSeconds.has_value());
    EXPECT_EQ(*restoredUpdate.positionSeconds, 0);
    EXPECT_EQ(tracker.timeListened(), 350);
    EXPECT_FALSE(tracker.playedThresholdReached());

    const auto nextUpdate = tracker.updatePosition(560);
    EXPECT_TRUE(nextUpdate.reachedPlayedThreshold);
    EXPECT_TRUE(tracker.playedThresholdReached());
    EXPECT_EQ(tracker.timeListened(), 410);
}

TEST(PlaybackProgressTrackerTest, ResetPositionPreventsLateEndSampleFromCountingSkippedTrackTime)
{
    PlaybackProgressTracker tracker;
    tracker.onTrackCommitted(10000, 0.99, 240000);

    const auto initialUpdate = tracker.updatePosition(1000);
    EXPECT_FALSE(initialUpdate.reachedPlayedThreshold);
    EXPECT_EQ(tracker.timeListened(), 1000);

    EXPECT_TRUE(tracker.markSeekPosition(9800));

    const auto afterSeek = tracker.updatePosition(9850);
    EXPECT_FALSE(afterSeek.reachedPlayedThreshold);
    EXPECT_EQ(tracker.timeListened(), 1000);

    const auto stoppedUpdate = tracker.resetPosition();
    EXPECT_EQ(stoppedUpdate.positionMs, 0);

    const auto lateEndUpdate = tracker.updatePosition(10000);
    EXPECT_FALSE(lateEndUpdate.reachedPlayedThreshold);
    EXPECT_EQ(tracker.timeListened(), 1000);
}

TEST(PlaybackProgressTrackerTest, EarlierElapsedThresholdWins)
{
    PlaybackProgressTracker tracker;
    tracker.onTrackCommitted(10000, 0.9, 4000);

    EXPECT_EQ(tracker.playedThreshold(), 4000);

    const auto preThresholdUpdate = tracker.updatePosition(3000);
    EXPECT_FALSE(preThresholdUpdate.reachedPlayedThreshold);

    const auto thresholdUpdate = tracker.updatePosition(4000);
    EXPECT_TRUE(thresholdUpdate.reachedPlayedThreshold);
    EXPECT_TRUE(tracker.playedThresholdReached());
}

TEST(PlaybackProgressTrackerTest, EarlierPercentageThresholdWins)
{
    PlaybackProgressTracker tracker;
    tracker.onTrackCommitted(1000, 0.25, 60000);

    EXPECT_EQ(tracker.playedThreshold(), 200);

    const auto thresholdUpdate = tracker.updatePosition(200);
    EXPECT_TRUE(thresholdUpdate.reachedPlayedThreshold);
    EXPECT_TRUE(tracker.playedThresholdReached());
}
} // namespace Fooyin::Testing
