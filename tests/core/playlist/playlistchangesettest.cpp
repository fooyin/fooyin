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

#include <core/playlist/playlist.h>
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

PlaylistTrack makePlaylistTrack(const QString& path, const UId& playlistId, const UId& entryId, int index,
                                uint64_t durationMs = 1000, const QString& hash = u"default"_s)
{
    return {.track           = makeTrack(path, durationMs, hash),
            .playlistId      = playlistId,
            .entryId         = entryId,
            .indexInPlaylist = index};
}
} // namespace

TEST(PlaylistChangesetTest, BuildsInsertionPatchForAppendedTracks)
{
    const UId playlistId = UId::create();
    const UId a          = UId::create();
    const UId b          = UId::create();
    const UId c          = UId::create();
    const UId d          = UId::create();
    const PlaylistTrackList oldTracks{makePlaylistTrack(u"/music/a.flac"_s, playlistId, a, 0),
                                      makePlaylistTrack(u"/music/b.flac"_s, playlistId, b, 1)};
    const PlaylistTrackList newTracks{makePlaylistTrack(u"/music/a.flac"_s, playlistId, a, 0),
                                      makePlaylistTrack(u"/music/b.flac"_s, playlistId, b, 1),
                                      makePlaylistTrack(u"/music/c.flac"_s, playlistId, c, 2),
                                      makePlaylistTrack(u"/music/d.flac"_s, playlistId, d, 3)};

    const auto changeSet = buildPlaylistChangeset(oldTracks, newTracks);
    ASSERT_TRUE(changeSet.has_value());
    EXPECT_FALSE(changeSet->replacesAllEntries);
    EXPECT_FALSE(changeSet->requiresReset);
    EXPECT_TRUE(changeSet->removedEntries.empty());
    ASSERT_EQ(changeSet->insertions.size(), 1);
    EXPECT_EQ(changeSet->insertions.front().index, 2);
    ASSERT_EQ(changeSet->insertions.front().tracks.size(), 2);
    EXPECT_EQ(changeSet->insertions.front().tracks.front().track.uniqueFilepath(), u"/music/c.flac"_s);
    EXPECT_TRUE(changeSet->moves.empty());
    EXPECT_TRUE(changeSet->updatedEntries.empty());
}

TEST(PlaylistChangesetTest, BuildsRemovalPatch)
{
    const UId playlistId = UId::create();
    const UId a          = UId::create();
    const UId b          = UId::create();
    const UId c          = UId::create();
    const PlaylistTrackList oldTracks{makePlaylistTrack(u"/music/a.flac"_s, playlistId, a, 0),
                                      makePlaylistTrack(u"/music/b.flac"_s, playlistId, b, 1),
                                      makePlaylistTrack(u"/music/c.flac"_s, playlistId, c, 2)};
    const PlaylistTrackList newTracks{makePlaylistTrack(u"/music/a.flac"_s, playlistId, a, 0),
                                      makePlaylistTrack(u"/music/c.flac"_s, playlistId, c, 1)};

    const auto changeSet = buildPlaylistChangeset(oldTracks, newTracks);
    ASSERT_TRUE(changeSet.has_value());
    EXPECT_FALSE(changeSet->replacesAllEntries);
    EXPECT_FALSE(changeSet->requiresReset);
    ASSERT_EQ(changeSet->removedEntries.size(), 1);
    EXPECT_EQ(changeSet->removedEntries.front(), b);
    EXPECT_TRUE(changeSet->insertions.empty());
    EXPECT_TRUE(changeSet->moves.empty());
}

TEST(PlaylistChangesetTest, BuildsMovePatchForReorderedTracks)
{
    const UId playlistId = UId::create();
    const UId a          = UId::create();
    const UId b          = UId::create();
    const UId c          = UId::create();
    const PlaylistTrackList oldTracks{makePlaylistTrack(u"/music/a.flac"_s, playlistId, a, 0),
                                      makePlaylistTrack(u"/music/b.flac"_s, playlistId, b, 1),
                                      makePlaylistTrack(u"/music/c.flac"_s, playlistId, c, 2)};
    const PlaylistTrackList newTracks{makePlaylistTrack(u"/music/c.flac"_s, playlistId, c, 0),
                                      makePlaylistTrack(u"/music/a.flac"_s, playlistId, a, 1),
                                      makePlaylistTrack(u"/music/b.flac"_s, playlistId, b, 2)};

    const auto changeSet = buildPlaylistChangeset(oldTracks, newTracks);
    ASSERT_TRUE(changeSet.has_value());
    EXPECT_FALSE(changeSet->replacesAllEntries);
    EXPECT_FALSE(changeSet->requiresReset);
    EXPECT_TRUE(changeSet->removedEntries.empty());
    EXPECT_TRUE(changeSet->insertions.empty());
    ASSERT_EQ(changeSet->moves.size(), 1);
    EXPECT_EQ(changeSet->moves.front().entryId, c);
    EXPECT_EQ(changeSet->moves.front().targetIndex, 0);
}

TEST(PlaylistChangesetTest, BuildsUpdatedIndexesForChangedTrackMetadata)
{
    const UId playlistId = UId::create();
    const UId a          = UId::create();
    const UId b          = UId::create();
    const PlaylistTrackList oldTracks{makePlaylistTrack(u"/music/a.flac"_s, playlistId, a, 0, 1000, u"old"_s),
                                      makePlaylistTrack(u"/music/b.flac"_s, playlistId, b, 1)};
    const PlaylistTrackList newTracks{makePlaylistTrack(u"/music/a.flac"_s, playlistId, a, 0, 1000, u"new"_s),
                                      makePlaylistTrack(u"/music/b.flac"_s, playlistId, b, 1)};

    const auto changeSet = buildPlaylistChangeset(oldTracks, newTracks);
    ASSERT_TRUE(changeSet.has_value());
    EXPECT_FALSE(changeSet->replacesAllEntries);
    EXPECT_FALSE(changeSet->requiresReset);
    ASSERT_EQ(changeSet->updatedEntries.size(), 1);
    EXPECT_EQ(changeSet->updatedEntries.front(), a);
}

TEST(PlaylistChangesetTest, BuildsUpdatedIndexesForChangedExtraTags)
{
    const UId playlistId = UId::create();
    const UId a          = UId::create();
    const UId b          = UId::create();

    PlaylistTrack oldTrack = makePlaylistTrack(u"/music/a.flac"_s, playlistId, a, 0);
    oldTrack.track.replaceExtraTag(u"CUSTOM"_s, QStringList{u"Before"_s});
    const PlaylistTrack unchangedTrack = makePlaylistTrack(u"/music/b.flac"_s, playlistId, b, 1);

    PlaylistTrack newTrack = makePlaylistTrack(u"/music/a.flac"_s, playlistId, a, 0);
    newTrack.track.replaceExtraTag(u"CUSTOM"_s, QStringList{u"After"_s});

    const auto changeSet = buildPlaylistChangeset({oldTrack, unchangedTrack}, {newTrack, unchangedTrack});
    ASSERT_TRUE(changeSet.has_value());
    EXPECT_FALSE(changeSet->requiresReset);
    ASSERT_EQ(changeSet->updatedEntries.size(), 1);
    EXPECT_EQ(changeSet->updatedEntries.front(), a);
}

TEST(PlaylistChangesetTest, BuildsUpdatedIndexesForExplicitlyUpdatedPaths)
{
    const UId playlistId = UId::create();
    const UId a          = UId::create();
    const UId b          = UId::create();
    const PlaylistTrackList oldTracks{makePlaylistTrack(u"/music/a.flac"_s, playlistId, a, 0),
                                      makePlaylistTrack(u"/music/b.flac"_s, playlistId, b, 1)};
    const PlaylistTrackList newTracks{makePlaylistTrack(u"/music/a.flac"_s, playlistId, a, 0),
                                      makePlaylistTrack(u"/music/b.flac"_s, playlistId, b, 1)};

    const auto changeSet = buildPlaylistChangeset(oldTracks, newTracks, TrackEntryIdSet{b});
    ASSERT_TRUE(changeSet.has_value());
    EXPECT_FALSE(changeSet->replacesAllEntries);
    EXPECT_FALSE(changeSet->requiresReset);
    ASSERT_EQ(changeSet->updatedEntries.size(), 1);
    EXPECT_EQ(changeSet->updatedEntries.front(), b);
}

TEST(PlaylistChangesetTest, FallsBackWhenEntryIdsAreDuplicated)
{
    const UId playlistId = UId::create();
    const UId duplicated = UId::create();
    const PlaylistTrackList oldTracks{makePlaylistTrack(u"/music/a.flac"_s, playlistId, duplicated, 0),
                                      makePlaylistTrack(u"/music/a.flac"_s, playlistId, duplicated, 1)};
    const PlaylistTrackList newTracks{makePlaylistTrack(u"/music/a.flac"_s, playlistId, duplicated, 0),
                                      makePlaylistTrack(u"/music/b.flac"_s, playlistId, UId::create(), 1)};

    EXPECT_FALSE(buildPlaylistChangeset(oldTracks, newTracks).has_value());
    EXPECT_FALSE(buildPlaylistChangeset(newTracks, oldTracks).has_value());
}

TEST(PlaylistChangesetTest, FallsBackWhenEntryIdsAreInvalid)
{
    const UId playlistId = UId::create();
    const PlaylistTrackList validTracks{makePlaylistTrack(u"/music/a.flac"_s, playlistId, UId::create(), 0)};
    const PlaylistTrackList invalidTracks{makePlaylistTrack(u"/music/a.flac"_s, playlistId, {}, 0)};

    EXPECT_FALSE(buildPlaylistChangeset(invalidTracks, validTracks).has_value());
    EXPECT_FALSE(buildPlaylistChangeset(validTracks, invalidTracks).has_value());
}

TEST(PlaylistChangesetTest, MarksLargeChangesetForReset)
{
    const UId playlistId = UId::create();
    PlaylistTrackList oldTracks;
    PlaylistTrackList newTracks;

    oldTracks.reserve(600);
    newTracks.reserve(600);

    for(int i{0}; i < 600; ++i) {
        oldTracks.push_back(makePlaylistTrack(u"/music/old_%1.flac"_s.arg(i), playlistId, UId::create(), i));
        newTracks.push_back(makePlaylistTrack(u"/music/new_%1.flac"_s.arg(i), playlistId, UId::create(), i));
    }

    const auto changeSet = buildPlaylistChangeset(oldTracks, newTracks);
    ASSERT_TRUE(changeSet.has_value());
    EXPECT_TRUE(changeSet->replacesAllEntries);
    EXPECT_TRUE(changeSet->requiresReset);
}

TEST(PlaylistChangesetTest, MarksReplacementWhenNoEntriesAreRetained)
{
    const UId playlistId = UId::create();
    const PlaylistTrackList oldTracks{makePlaylistTrack(u"/music/a.flac"_s, playlistId, UId::create(), 0),
                                      makePlaylistTrack(u"/music/b.flac"_s, playlistId, UId::create(), 1)};
    const PlaylistTrackList newTracks{makePlaylistTrack(u"/music/c.flac"_s, playlistId, UId::create(), 0),
                                      makePlaylistTrack(u"/music/d.flac"_s, playlistId, UId::create(), 1)};

    const auto changeSet = buildPlaylistChangeset(oldTracks, newTracks);
    ASSERT_TRUE(changeSet.has_value());
    EXPECT_TRUE(changeSet->replacesAllEntries);
    EXPECT_TRUE(changeSet->requiresReset);
}
} // namespace Fooyin::Testing
