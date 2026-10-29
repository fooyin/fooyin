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

#include <core/player/playbackqueue.h>

#include <core/track.h>

#include <gtest/gtest.h>

#include <array>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
namespace {
PlaylistTrack makeTrack(const QString& path, int index)
{
    Track track{path, 0};
    track.setId(index + 1);
    track.generateHash();
    return {.track = track, .playlistId = UId::create(), .entryId = UId::create(), .indexInPlaylist = index};
}
} // namespace

TEST(PlaybackQueueTest, SequenceTracksHaveOccurrenceIdentity)
{
    PlaybackQueue queue;
    const auto repeated = makeTrack(u"/tmp/repeated.flac"_s, 0);

    queue.replaceSequence({repeated, repeated, repeated}, 1);

    ASSERT_EQ(queue.items().size(), 3);
    EXPECT_NE(queue.items()[0].id, queue.items()[1].id);
    EXPECT_NE(queue.items()[1].id, queue.items()[2].id);
    EXPECT_EQ(queue.currentItemId(), queue.items()[1].id);
    EXPECT_EQ(queue.relativeItem(-1, Playlist::Default)->id, queue.items()[0].id);
    EXPECT_EQ(queue.relativeItem(1, Playlist::Default)->id, queue.items()[2].id);
}

TEST(PlaybackQueueTest, RemovingItemsPreservesCursorByOccurrence)
{
    PlaybackQueue queue;
    QueueTracks tracks{makeTrack(u"/tmp/one.flac"_s, 0), makeTrack(u"/tmp/two.flac"_s, 1),
                       makeTrack(u"/tmp/three.flac"_s, 2), makeTrack(u"/tmp/four.flac"_s, 3)};
    queue.replaceSequence(tracks, 2);
    const auto currentId = queue.currentItemId();
    const auto firstId   = queue.items().front().id;

    ASSERT_TRUE(queue.removeItem(firstId).has_value());
    EXPECT_EQ(queue.currentItemId(), currentId);
    EXPECT_EQ(queue.currentIndex(), 1);

    EXPECT_FALSE(queue.removeItem(currentId).has_value());
    EXPECT_EQ(queue.currentItemId(), currentId);
    EXPECT_EQ(queue.currentIndex(), 1);
    ASSERT_NE(queue.relativeItem(1, Playlist::Default), nullptr);
    EXPECT_EQ(queue.relativeItem(1, Playlist::Default)->track, tracks[3]);
}

TEST(PlaybackQueueTest, ClearingSequenceRemovesCurrentOccurrence)
{
    PlaybackQueue queue;
    queue.replaceSequence(
        {makeTrack(u"/tmp/one.flac"_s, 0), makeTrack(u"/tmp/two.flac"_s, 1), makeTrack(u"/tmp/three.flac"_s, 2)}, 1);
    queue.clear();

    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.currentItemId(), 0);
    EXPECT_EQ(queue.currentIndex(), -1);
}

TEST(PlaybackQueueTest, PruningHistoryRetainsConfiguredTracksBeforeCurrent)
{
    PlaybackQueue queue;
    queue.replaceSequence({makeTrack(u"/tmp/one.flac"_s, 0), makeTrack(u"/tmp/two.flac"_s, 1),
                           makeTrack(u"/tmp/three.flac"_s, 2), makeTrack(u"/tmp/four.flac"_s, 3)},
                          3);
    const auto currentId = queue.currentItemId();

    const auto removed = queue.pruneHistory(1);

    ASSERT_EQ(removed.size(), 2);
    EXPECT_EQ(removed[0].track.indexInPlaylist, 0);
    EXPECT_EQ(removed[1].track.indexInPlaylist, 1);
    ASSERT_EQ(queue.items().size(), 2);
    EXPECT_EQ(queue.items()[0].track.indexInPlaylist, 2);
    EXPECT_EQ(queue.currentItemId(), currentId);
    EXPECT_EQ(queue.currentIndex(), 1);
}

TEST(PlaybackQueueTest, NegativeHistoryLimitRetainsAllTracks)
{
    PlaybackQueue queue;
    queue.replaceSequence({makeTrack(u"/tmp/one.flac"_s, 0), makeTrack(u"/tmp/two.flac"_s, 1)}, 1);

    EXPECT_TRUE(queue.pruneHistory(-1).empty());
    EXPECT_EQ(queue.items().size(), 2);
    EXPECT_EQ(queue.currentIndex(), 1);
}

TEST(PlaybackQueueTest, MovingItemsKeepsCurrentOccurrence)
{
    PlaybackQueue queue;
    queue.replaceSequence({makeTrack(u"/tmp/one.flac"_s, 0), makeTrack(u"/tmp/two.flac"_s, 1),
                           makeTrack(u"/tmp/three.flac"_s, 2), makeTrack(u"/tmp/four.flac"_s, 3)},
                          1);
    const auto currentId = queue.currentItemId();
    const auto lastId    = queue.items().back().id;

    ASSERT_TRUE(queue.moveItems(std::span{&lastId, size_t{1}}, 0));
    EXPECT_EQ(queue.items().front().id, lastId);
    EXPECT_EQ(queue.currentItemId(), currentId);
    EXPECT_EQ(queue.currentIndex(), 2);
}

TEST(PlaybackQueueTest, ReorderingItemsPreservesCurrentOccurrence)
{
    PlaybackQueue queue;
    queue.replaceSequence(
        {makeTrack(u"/tmp/one.flac"_s, 0), makeTrack(u"/tmp/two.flac"_s, 1), makeTrack(u"/tmp/three.flac"_s, 2)}, 1);
    const auto firstId   = queue.items()[0].id;
    const auto currentId = queue.currentItemId();
    const auto lastId    = queue.items()[2].id;
    const std::array reorderedIds{lastId, firstId, currentId};

    ASSERT_TRUE(queue.reorderItems(reorderedIds));
    EXPECT_EQ(queue.items()[0].id, lastId);
    EXPECT_EQ(queue.items()[1].id, firstId);
    EXPECT_EQ(queue.items()[2].id, currentId);
    EXPECT_EQ(queue.currentIndex(), 2);
}

TEST(PlaybackQueueTest, ReorderingItemsRejectsDuplicateIds)
{
    PlaybackQueue queue;
    queue.replaceSequence({makeTrack(u"/tmp/one.flac"_s, 0), makeTrack(u"/tmp/two.flac"_s, 1)}, 0);
    const auto originalItems = queue.items();
    const std::array duplicateIds{originalItems[0].id, originalItems[0].id};

    EXPECT_FALSE(queue.reorderItems(duplicateIds));
    EXPECT_EQ(queue.items(), originalItems);
}

TEST(PlaybackQueueTest, SnapshotRestoresOrderAndCursor)
{
    PlaybackQueue queue;
    queue.replaceSequence({makeTrack(u"/tmp/one.flac"_s, 0), makeTrack(u"/tmp/two.flac"_s, 1)}, 1,
                          PlaybackQueueItemOrigin::PlaylistGenerated, std::array{4, 2});
    auto snapshot = queue.snapshot();
    for(auto& item : snapshot.items) {
        item.id = 0;
    }

    PlaybackQueue restored;
    restored.restore(std::move(snapshot));

    ASSERT_EQ(restored.items().size(), 2);
    EXPECT_NE(restored.items()[0].id, 0);
    EXPECT_NE(restored.items()[0].id, restored.items()[1].id);
    EXPECT_EQ(restored.items()[0].sourceOrder, 4);
    EXPECT_EQ(restored.items()[1].sourceOrder, 2);
    EXPECT_EQ(restored.currentIndex(), 1);
}
} // namespace Fooyin::Testing
