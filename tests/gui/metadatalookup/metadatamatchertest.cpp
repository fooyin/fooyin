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

#include "gui/metadatalookup/metadatamatcher.h"

#include <gtest/gtest.h>

#include <tuple>

using namespace Qt::StringLiterals;
using namespace Fooyin;
using namespace Fooyin;

namespace Fooyin::Testing {
namespace {
Release testRelease()
{
    Release release;
    release.summary.id = u"release"_s;

    ReleaseMedium medium;
    medium.position = 1;

    for(const auto& [title, position, duration] : std::vector<std::tuple<QString, int, int64_t>>{
            {u"First Song"_s, 1, 180000}, {u"Second Song"_s, 2, 200000}, {u"Finale"_s, 3, 240000}}) {
        ReleaseTrack remote;
        remote.title          = title;
        remote.mediumPosition = 1;
        remote.position       = position;
        remote.number         = QString::number(position);
        remote.total          = 3;
        remote.durationMs     = duration;

        medium.tracks.push_back(std::move(remote));
    }
    release.media.push_back(std::move(medium));

    return release;
}

Track track(const QString& title, int number, uint64_t duration)
{
    Track result{u"/tmp/%1.flac"_s.arg(number)};
    result.setTitle(title);
    result.setTrackNumber(QString::number(number));
    result.setDiscNumber(u"1"_s);
    result.setDuration(duration);
    return result;
}
} // namespace

TEST(MetadataMatcherTest, MatchesNumberedAlbum)
{
    const TrackList tracks{track(u"First Song"_s, 1, 180500), track(u"Second Song"_s, 2, 199000),
                           track(u"Finale"_s, 3, 240000)};

    const auto matches = matchTracks(tracks, testRelease());
    ASSERT_EQ(matches.size(), 3);

    for(size_t i{0}; i < matches.size(); ++i) {
        ASSERT_TRUE(matches.at(i).remoteIndex);
        EXPECT_EQ(*matches.at(i).remoteIndex, i);
        EXPECT_FALSE(matches.at(i).ambiguous);
    }
}

TEST(MetadataMatcherTest, MatchesShuffledTracksByTitleAndDuration)
{
    Track finale = track(u"Finale"_s, 0, 240000);
    finale.setTrackNumber({});
    Track first = track(u"First Song"_s, 0, 180000);
    first.setTrackNumber({});

    const auto matches = matchTracks({finale, first}, testRelease());
    ASSERT_TRUE(matches.at(0).remoteIndex);
    ASSERT_TRUE(matches.at(1).remoteIndex);
    EXPECT_EQ(*matches.at(0).remoteIndex, 2);
    EXPECT_EQ(*matches.at(1).remoteIndex, 0);
}

TEST(MetadataMatcherTest, NeverAssignsRemoteTrackTwice)
{
    const TrackList tracks{track(u"First Song"_s, 0, 180000), track(u"First Song"_s, 0, 180000)};
    const auto matches = matchTracks(tracks, testRelease());

    ASSERT_TRUE(matches.at(0).remoteIndex);
    if(matches.at(1).remoteIndex) {
        EXPECT_NE(*matches.at(0).remoteIndex, *matches.at(1).remoteIndex);
    }
}

TEST(MetadataMatcherTest, MatchesDiscTracksByPhysicalPosition)
{
    TrackList tracks{track(u"Track 01"_s, 1, 180500), track(u"Track 02"_s, 2, 199000), track(u"Track 03"_s, 3, 240000)};
    for(Track& localTrack : tracks) {
        localTrack.setDiscNumber({});
    }

    const auto matches = matchTracksByPosition(tracks, testRelease());
    ASSERT_EQ(matches.size(), tracks.size());
    for(size_t index{0}; index < matches.size(); ++index) {
        ASSERT_TRUE(matches.at(index).remoteIndex);
        EXPECT_EQ(*matches.at(index).remoteIndex, index);
        EXPECT_EQ(matches.at(index).confidence, 100);
        EXPECT_FALSE(matches.at(index).ambiguous);
    }
}
} // namespace Fooyin::Testing
