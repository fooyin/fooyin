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

#include "core/playback/playbackcoordinator.h"
#include "core/engine/enginehelpers.h"

#include <core/track.h>

#include <gtest/gtest.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
namespace {
Track makeTrack(const QString& path, int subsong, uint64_t offsetMs, uint64_t durationMs, int id = -1)
{
    Track track{path, subsong};
    track.setOffset(offsetMs);
    track.setDuration(durationMs);
    if(id >= 0) {
        track.setId(id);
    }
    return track;
}
} // namespace

TEST(PlaybackCoordinatorTest, SuppressesOnlyExactGeneration)
{
    std::optional<uint64_t> suppressedGeneration{10};
    const Track track = makeTrack(u"/music/a.flac"_s, 0, 0, 1000, 1);

    EXPECT_EQ(PlaybackCoordinator::evaluateEndForAutoAdvance(track, track, 10, suppressedGeneration),
              PlaybackCoordinator::EndAction::Suppress);
    EXPECT_FALSE(suppressedGeneration.has_value());
    EXPECT_EQ(PlaybackCoordinator::evaluateEndForAutoAdvance(track, track, 11, suppressedGeneration),
              PlaybackCoordinator::EndAction::Advance);
}

TEST(PlaybackCoordinatorTest, SuppressedStaleEndDoesNotSuppressNextRealEnd)
{
    std::optional<uint64_t> suppressedGeneration{100};
    const Track oldTrack = makeTrack(u"/music/old.flac"_s, 0, 0, 1000, 1);
    const Track newTrack = makeTrack(u"/music/new.flac"_s, 0, 0, 1000, 2);

    EXPECT_EQ(PlaybackCoordinator::evaluateEndForAutoAdvance(newTrack, oldTrack, 100, suppressedGeneration),
              PlaybackCoordinator::EndAction::Suppress);
    EXPECT_FALSE(suppressedGeneration.has_value());
    EXPECT_EQ(PlaybackCoordinator::evaluateEndForAutoAdvance(newTrack, newTrack, 101, suppressedGeneration),
              PlaybackCoordinator::EndAction::Advance);
}

TEST(PlaybackCoordinatorTest, IgnoresStaleEndWithoutSuppression)
{
    std::optional<uint64_t> suppressedGeneration;
    const Track oldTrack = makeTrack(u"/music/old.flac"_s, 0, 0, 1000, 1);
    const Track newTrack = makeTrack(u"/music/new.flac"_s, 0, 0, 1000, 2);

    EXPECT_EQ(PlaybackCoordinator::evaluateEndForAutoAdvance(newTrack, oldTrack, 55, suppressedGeneration),
              PlaybackCoordinator::EndAction::IgnoreStale);
}

TEST(PlaybackCoordinatorTest, EvaluateEndFallsBackToSegmentIdentityWhenIdsUnknown)
{
    std::optional<uint64_t> suppressedGeneration;

    const Track currentTrack = makeTrack(u"/music/segment.flac"_s, 3, 10000, 2000, -1);
    const Track endedTrack   = makeTrack(u"/music/segment.flac"_s, 3, 10000, 2000, -1);
    EXPECT_EQ(PlaybackCoordinator::evaluateEndForAutoAdvance(currentTrack, endedTrack, 1, suppressedGeneration),
              PlaybackCoordinator::EndAction::Advance);

    const Track mismatchedDuration = makeTrack(u"/music/segment.flac"_s, 3, 10000, 2500, -1);
    EXPECT_EQ(PlaybackCoordinator::evaluateEndForAutoAdvance(currentTrack, mismatchedDuration, 2, suppressedGeneration),
              PlaybackCoordinator::EndAction::IgnoreStale);
}
} // namespace Fooyin::Testing
