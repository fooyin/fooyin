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

#include "gui/metadatalookup/metadataapply.h"

#include <gtest/gtest.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
namespace {
Release testRelease()
{
    Release release;
    release.summary.id = u"release-id"_s;
    release.summary.identifiers
        = {{u"MUSICBRAINZ_ALBUMID"_s, {u"release-id"_s}}, {u"MUSICBRAINZ_RELEASEGROUPID"_s, {u"group-id"_s}}};
    release.summary.title          = u"New Album"_s;
    release.summary.artistCredit   = {{u"Album Artist"_s, {}, u"album-artist-id"_s}};
    release.summary.date           = u"2024-06-07"_s;
    release.summary.originalDate   = u"2024-06-01"_s;
    release.summary.country        = u"JP"_s;
    release.summary.status         = u"Official"_s;
    release.summary.labels         = {u"Label"_s};
    release.summary.catalogNumbers = {u"CAT-1"_s};
    release.summary.barcode        = u"123456"_s;
    release.summary.releaseTypes   = {u"Album"_s, u"Soundtrack"_s};
    release.genres                 = {u"anime"_s};
    ReleaseMedium medium;
    medium.position = 1;
    medium.title    = u"Main Disc"_s;
    medium.format   = u"CD"_s;
    ReleaseTrack remote;
    remote.identifiers
        = {{u"MUSICBRAINZ_RELEASETRACKID"_s, {u"release-track-id"_s}}, {u"MUSICBRAINZ_TRACKID"_s, {u"recording-id"_s}}};
    remote.title          = u"New Title"_s;
    remote.artistCredit   = {{u"Track Artist"_s, {}, u"track-artist-id"_s}};
    remote.isrcs          = {u"JPAAA2400001"_s};
    remote.mediumPosition = 1;
    remote.position       = 1;
    remote.number         = u"1"_s;
    remote.total          = 1;
    remote.durationMs     = 180000;
    medium.tracks.push_back(std::move(remote));
    release.media.push_back(std::move(medium));
    return release;
}

Track originalTrack()
{
    Track track{u"/music/file.flac"_s};
    track.setId(42);
    track.setLibraryId(7);
    track.setTitle(u"Old Title"_s);
    track.setArtists({u"Old Artist"_s});
    track.setAlbum(u"Old Album"_s);
    track.setDuration(180000);
    track.setPlayCount(9);
    track.setRatingStars(8);
    track.setRGTrackGain(-6.5F);
    track.replaceExtraTag(u"CUSTOM"_s, u"Keep me"_s);
    return track;
}
} // namespace

TEST(MetadataApplyTest, ReplacesLookupFieldsAndPreservesIdentity)
{
    const Track original = originalTrack();
    const auto result    = applyReleaseMetadata({original}, testRelease(), {{.localIndex = 0, .remoteIndex = 0}}, {});
    ASSERT_EQ(result.tracks.size(), 1);
    ASSERT_EQ(result.trackIndices, (std::vector<size_t>{0}));
    const Track& updated = result.tracks.front();
    EXPECT_EQ(updated.filepath(), original.filepath());
    EXPECT_EQ(updated.id(), 42);
    EXPECT_EQ(updated.libraryId(), 7);
    EXPECT_EQ(updated.duration(), 180000);
    EXPECT_EQ(updated.playCount(), 9);
    EXPECT_EQ(updated.ratingStars(), 8);
    EXPECT_FLOAT_EQ(updated.rgTrackGain(), -6.5F);
    EXPECT_EQ(updated.title(), u"New Title"_s);
    EXPECT_EQ(updated.album(), u"New Album"_s);
    EXPECT_EQ(updated.artists(), QStringList({u"Track Artist"_s}));
    EXPECT_EQ(updated.extraTag(u"CUSTOM"_s), QStringList({u"Keep me"_s}));
    EXPECT_EQ(updated.extraTag(u"MUSICBRAINZ_TRACKID"_s), QStringList({u"recording-id"_s}));
    EXPECT_EQ(updated.extraTag(u"MUSICBRAINZ_ALBUMID"_s), QStringList({u"release-id"_s}));
    EXPECT_EQ(updated.extraTag(u"ISRC"_s), QStringList({u"JPAAA2400001"_s}));
    EXPECT_EQ(updated.extraTag(u"MEDIA"_s), QStringList({u"CD"_s}));
    EXPECT_FALSE(result.changes.empty());
}

TEST(MetadataApplyTest, FillMissingLeavesExistingValues)
{
    MetadataApplyOptions options;
    options.policy = ExistingMetadataPolicy::FillMissing;

    const auto result
        = applyReleaseMetadata({originalTrack()}, testRelease(), {{.localIndex = 0, .remoteIndex = 0}}, options);
    ASSERT_EQ(result.tracks.size(), 1);
    EXPECT_EQ(result.tracks.front().title(), u"Old Title"_s);
    EXPECT_EQ(result.tracks.front().album(), u"Old Album"_s);
    EXPECT_EQ(result.tracks.front().extraTag(u"MUSICBRAINZ_TRACKID"_s), QStringList({u"recording-id"_s}));
}

TEST(MetadataApplyTest, WipeRemovesCustomTagsButPreservesProtectedState)
{
    MetadataApplyOptions options;
    options.policy = ExistingMetadataPolicy::WipeWritableTags;

    const Track original = originalTrack();
    const auto result = applyReleaseMetadata({original}, testRelease(), {{.localIndex = 0, .remoteIndex = 0}}, options);
    ASSERT_EQ(result.tracks.size(), 1);
    const Track& updated = result.tracks.front();
    EXPECT_FALSE(updated.hasExtraTag(u"CUSTOM"_s));
    EXPECT_EQ(updated.id(), original.id());
    EXPECT_EQ(updated.filepath(), original.filepath());
    EXPECT_EQ(updated.duration(), original.duration());
    EXPECT_EQ(updated.playCount(), original.playCount());
    EXPECT_EQ(updated.ratingStars(), original.ratingStars());
    EXPECT_FLOAT_EQ(updated.rgTrackGain(), original.rgTrackGain());
}

TEST(MetadataApplyTest, LeavesUnmatchedTracksUntouched)
{
    const auto result = applyReleaseMetadata({originalTrack()}, testRelease(), {TrackMatch{}}, {});
    EXPECT_TRUE(result.tracks.empty());
    EXPECT_TRUE(result.trackIndices.empty());
    EXPECT_TRUE(result.changes.empty());
}

TEST(MetadataApplyTest, WritesIdentifiersSuppliedByTheActiveProvider)
{
    Release release             = testRelease();
    release.summary.identifiers = {{u"PROVIDER_RELEASE_ID"_s, {u"123456"_s}}, {u"PROVIDER_GROUP_ID"_s, {u"654321"_s}}};
    release.media.front().tracks.front().identifiers = {{u"PROVIDER_ARTIST_ID"_s, {u"300"_s}}};

    const auto result = applyReleaseMetadata({originalTrack()}, release, {{.localIndex = 0, .remoteIndex = 0}}, {});
    ASSERT_EQ(result.tracks.size(), 1);
    const Track& updated = result.tracks.front();
    EXPECT_EQ(updated.extraTag(u"PROVIDER_RELEASE_ID"_s), QStringList({u"123456"_s}));
    EXPECT_EQ(updated.extraTag(u"PROVIDER_GROUP_ID"_s), QStringList({u"654321"_s}));
    EXPECT_EQ(updated.extraTag(u"PROVIDER_ARTIST_ID"_s), QStringList({u"300"_s}));
    EXPECT_FALSE(updated.hasExtraTag(u"MUSICBRAINZ_ALBUMID"_s));
}
} // namespace Fooyin::Testing
