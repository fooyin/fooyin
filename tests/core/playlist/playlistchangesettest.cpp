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

#include <core/playlist/playlistchangeset.h>
#include <core/track.h>

#include <gtest/gtest.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
namespace {
Track makeTrack(const QString& path, uint64_t durationMs = 1000, const QString& hash = u"default"_s)
{
    Track track{path};
    track.setDuration(durationMs);
    track.setHash(hash);
    return track;
}
} // namespace

TEST(PlaylistChangesetTest, BuildsInsertionPatchForAppendedTracks)
{
    const TrackList oldTracks{makeTrack(u"/music/a.flac"_s), makeTrack(u"/music/b.flac"_s)};
    const TrackList newTracks{makeTrack(u"/music/a.flac"_s), makeTrack(u"/music/b.flac"_s),
                              makeTrack(u"/music/c.flac"_s), makeTrack(u"/music/d.flac"_s)};

    const auto changeSet = buildPlaylistChangeset(oldTracks, newTracks);
    ASSERT_TRUE(changeSet.has_value());
    EXPECT_FALSE(changeSet->requiresReset);
    EXPECT_TRUE(changeSet->removedIndexes.empty());
    ASSERT_EQ(changeSet->insertions.size(), 1U);
    EXPECT_EQ(changeSet->insertions.front().index, 2);
    ASSERT_EQ(changeSet->insertions.front().tracks.size(), 2U);
    EXPECT_EQ(changeSet->insertions.front().tracks.front().uniqueFilepath(), u"/music/c.flac"_s);
    EXPECT_TRUE(changeSet->moves.empty());
    EXPECT_TRUE(changeSet->updatedIndexes.empty());
}

TEST(PlaylistChangesetTest, BuildsRemovalPatch)
{
    const TrackList oldTracks{makeTrack(u"/music/a.flac"_s), makeTrack(u"/music/b.flac"_s),
                              makeTrack(u"/music/c.flac"_s)};
    const TrackList newTracks{makeTrack(u"/music/a.flac"_s), makeTrack(u"/music/c.flac"_s)};

    const auto changeSet = buildPlaylistChangeset(oldTracks, newTracks);
    ASSERT_TRUE(changeSet.has_value());
    EXPECT_FALSE(changeSet->requiresReset);
    ASSERT_EQ(changeSet->removedIndexes.size(), 1U);
    EXPECT_EQ(changeSet->removedIndexes.front(), 1);
    EXPECT_TRUE(changeSet->insertions.empty());
    EXPECT_TRUE(changeSet->moves.empty());
}

TEST(PlaylistChangesetTest, BuildsMovePatchForReorderedTracks)
{
    const TrackList oldTracks{makeTrack(u"/music/a.flac"_s), makeTrack(u"/music/b.flac"_s),
                              makeTrack(u"/music/c.flac"_s)};
    const TrackList newTracks{makeTrack(u"/music/c.flac"_s), makeTrack(u"/music/a.flac"_s),
                              makeTrack(u"/music/b.flac"_s)};

    const auto changeSet = buildPlaylistChangeset(oldTracks, newTracks);
    ASSERT_TRUE(changeSet.has_value());
    EXPECT_FALSE(changeSet->requiresReset);
    EXPECT_TRUE(changeSet->removedIndexes.empty());
    EXPECT_TRUE(changeSet->insertions.empty());
    ASSERT_EQ(changeSet->moves.size(), 1U);
    EXPECT_EQ(changeSet->moves.front().sourceIndex, 2);
    EXPECT_EQ(changeSet->moves.front().targetIndex, 0);
}

TEST(PlaylistChangesetTest, BuildsUpdatedIndexesForChangedTrackMetadata)
{
    const TrackList oldTracks{makeTrack(u"/music/a.flac"_s, 1000, u"old"_s), makeTrack(u"/music/b.flac"_s)};
    const TrackList newTracks{makeTrack(u"/music/a.flac"_s, 1000, u"new"_s), makeTrack(u"/music/b.flac"_s)};

    const auto changeSet = buildPlaylistChangeset(oldTracks, newTracks);
    ASSERT_TRUE(changeSet.has_value());
    EXPECT_FALSE(changeSet->requiresReset);
    ASSERT_EQ(changeSet->updatedIndexes.size(), 1U);
    EXPECT_EQ(changeSet->updatedIndexes.front(), 0);
}

TEST(PlaylistChangesetTest, BuildsUpdatedIndexesForExplicitlyUpdatedPaths)
{
    const TrackList oldTracks{makeTrack(u"/music/a.flac"_s), makeTrack(u"/music/b.flac"_s)};
    const TrackList newTracks{makeTrack(u"/music/a.flac"_s), makeTrack(u"/music/b.flac"_s)};

    const auto changeSet = buildPlaylistChangeset(oldTracks, newTracks, TrackKeySet{u"/music/b.flac"_s});
    ASSERT_TRUE(changeSet.has_value());
    EXPECT_FALSE(changeSet->requiresReset);
    ASSERT_EQ(changeSet->updatedIndexes.size(), 1U);
    EXPECT_EQ(changeSet->updatedIndexes.front(), 1);
}

TEST(PlaylistChangesetTest, FallsBackWhenTrackPathsAreDuplicated)
{
    const TrackList oldTracks{makeTrack(u"/music/a.flac"_s), makeTrack(u"/music/a.flac"_s)};
    const TrackList newTracks{makeTrack(u"/music/a.flac"_s), makeTrack(u"/music/b.flac"_s)};

    EXPECT_FALSE(buildPlaylistChangeset(oldTracks, newTracks).has_value());
}

TEST(PlaylistChangesetTest, MarksLargeChangesetForReset)
{
    TrackList oldTracks;
    TrackList newTracks;

    oldTracks.reserve(600);
    newTracks.reserve(600);

    for(int i{0}; i < 600; ++i) {
        oldTracks.push_back(makeTrack(u"/music/old_%1.flac"_s.arg(i)));
        newTracks.push_back(makeTrack(u"/music/new_%1.flac"_s.arg(i)));
    }

    const auto changeSet = buildPlaylistChangeset(oldTracks, newTracks);
    ASSERT_TRUE(changeSet.has_value());
    EXPECT_TRUE(changeSet->requiresReset);
}
} // namespace Fooyin::Testing
