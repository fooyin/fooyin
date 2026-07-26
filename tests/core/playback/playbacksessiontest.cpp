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

#include "core/playback/playbacksession.h"

#include <core/track.h>

#include <gtest/gtest.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
namespace {
Track makeTrack(const QString& path, int id, uint64_t durationMs)
{
    Track track{path, 0};
    track.setId(id);
    track.setDuration(durationMs);
    track.generateHash();
    return track;
}

PlaylistTrack makePlaylistTrack(const QString& path, int id, uint64_t durationMs, int index)
{
    return PlaylistTrack{
        .track           = makeTrack(path, id, durationMs),
        .playlistId      = UId::create(),
        .entryId         = UId::create(),
        .indexInPlaylist = index,
    };
}
} // namespace

TEST(PlaybackSessionTest, StartsIdleAndAcceptsRequests)
{
    const PlaybackSession session;

    EXPECT_TRUE(session.isIdle());
    EXPECT_FALSE(session.hasCurrentTrack());
    EXPECT_FALSE(session.hasPendingRequest());
    EXPECT_TRUE(session.canAcceptRequest());
    EXPECT_FALSE(session.isQueueTrack());
    EXPECT_EQ(session.currentItemId(), 0);
}

TEST(PlaybackSessionTest, RequestTrackChangeStoresPendingContext)
{
    PlaybackSession session;
    const PlaylistTrack track = makePlaylistTrack(u"/tmp/request.flac"_s, 1, 1000, 2);

    const auto request = session.requestTrackChange({
        .track        = track,
        .context      = {.reason = Player::AdvanceReason::ManualSelection, .userInitiated = true},
        .isQueueTrack = false,
        .itemId       = 42,
    });

    EXPECT_EQ(request.track, track);
    EXPECT_TRUE(session.hasPendingRequest());
    EXPECT_FALSE(session.canAcceptRequest());
    EXPECT_FALSE(session.isIdle());
    EXPECT_EQ(session.pendingChangeContext().reason, Player::AdvanceReason::ManualSelection);
    EXPECT_TRUE(session.pendingChangeContext().userInitiated);
}

TEST(PlaybackSessionTest, CommitRequestUsesPendingMetadataAndClearsPendingState)
{
    PlaybackSession session;
    const PlaylistTrack track = makePlaylistTrack(u"/tmp/commit.flac"_s, 2, 1200, 1);
    session.scheduleTrack(makePlaylistTrack(u"/tmp/scheduled.flac"_s, 3, 900, 4),
                          PlaybackSession::ScheduledTrackKind::StopAfterCurrentResume);
    EXPECT_EQ(session.scheduledTrackKind(), PlaybackSession::ScheduledTrackKind::StopAfterCurrentResume);

    const auto pendingRequest = session.requestTrackChange({
        .track        = track,
        .context      = {.reason = Player::AdvanceReason::ManualNext, .userInitiated = true},
        .isQueueTrack = true,
        .itemId       = 77,
    });
    EXPECT_EQ(pendingRequest.itemId, 77);

    const auto result = session.commitRequest({
        .track        = track,
        .context      = {.reason = Player::AdvanceReason::PreparedCommit, .userInitiated = false},
        .isQueueTrack = false,
        .itemId       = 77,
    });

    EXPECT_TRUE(result.matchedPendingRequest);
    EXPECT_TRUE(result.isQueueTrack);
    EXPECT_TRUE(session.hasCurrentTrack());
    EXPECT_EQ(session.currentTrack(), track);
    EXPECT_EQ(session.lastChangeContext().reason, Player::AdvanceReason::ManualNext);
    EXPECT_TRUE(session.lastChangeContext().userInitiated);
    EXPECT_FALSE(session.hasPendingRequest());
    EXPECT_TRUE(session.canAcceptRequest());
    EXPECT_FALSE(session.scheduledTrack().isValid());
    EXPECT_EQ(session.scheduledTrackKind(), PlaybackSession::ScheduledTrackKind::Normal);
    EXPECT_TRUE(session.isQueueTrack());
    EXPECT_EQ(session.currentItemId(), 77);
}

TEST(PlaybackSessionTest, CommitRequestFallsBackToRequestMetadataWithoutPendingMatch)
{
    PlaybackSession session;
    const PlaylistTrack track = makePlaylistTrack(u"/tmp/direct.flac"_s, 4, 800, 0);

    const auto result = session.commitRequest({
        .track        = track,
        .context      = {.reason = Player::AdvanceReason::PreparedCrossfadeCommit, .userInitiated = false},
        .isQueueTrack = false,
        .itemId       = 9,
    });

    EXPECT_FALSE(result.matchedPendingRequest);
    EXPECT_FALSE(result.isQueueTrack);
    EXPECT_EQ(session.currentTrack(), track);
    EXPECT_EQ(session.lastChangeContext().reason, Player::AdvanceReason::PreparedCrossfadeCommit);
    EXPECT_FALSE(session.lastChangeContext().userInitiated);
    EXPECT_EQ(session.currentItemId(), 9);
}

TEST(PlaybackSessionTest, UpdateHelpersOnlyMutateMatchingCurrentTrack)
{
    PlaybackSession session;
    const PlaylistTrack original = makePlaylistTrack(u"/tmp/current.flac"_s, 5, 1400, 5);
    const auto initialCommit     = session.commitRequest({
        .track        = original,
        .context      = {.reason = Player::AdvanceReason::ManualSelection, .userInitiated = true},
        .isQueueTrack = false,
        .itemId       = 1,
    });
    EXPECT_FALSE(initialCommit.isQueueTrack);

    Track updatedTrack = original.track;
    updatedTrack.setTitle(u"Updated"_s);

    EXPECT_TRUE(session.updateCurrentTrack(updatedTrack));
    EXPECT_EQ(session.currentTrack().track.title(), u"Updated"_s);
    EXPECT_TRUE(session.updateCurrentTrackPlaylist(UId::create()));
    EXPECT_TRUE(session.updateCurrentTrackEntry(UId::create()));
    EXPECT_TRUE(session.updateCurrentTrackIndex(7));

    const Track otherTrack = makeTrack(u"/tmp/other.flac"_s, 6, 1500);
    EXPECT_FALSE(session.updateCurrentTrack(otherTrack));
}
} // namespace Fooyin::Testing
