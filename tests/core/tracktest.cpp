/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
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

#include <core/constants.h>
#include <core/engine/enginehelpers.h>
#include <core/track.h>
#include <core/trackmetadatastore.h>

#include <QDataStream>
#include <QIODevice>

#include <gtest/gtest.h>

#include <cmath>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
namespace {
template <typename Map>
QByteArray serialiseLegacyMap(const Map& map)
{
    QByteArray out;
    QDataStream stream{&out, QIODevice::WriteOnly};
    stream.setVersion(QDataStream::Qt_6_0);
    stream << map;
    return out;
}
} // namespace

TEST(TrackTest, DerivesPathFieldsForRegularFiles)
{
    const Track track{u"/music/Artist/Album/track01.FlAc"_s};

    EXPECT_EQ(track.filename(), u"track01"_s);
    EXPECT_EQ(track.directory(), u"Album"_s);
    EXPECT_EQ(track.extension(), u"flac"_s);
    EXPECT_EQ(track.effectiveTitle(), u"track01"_s);
}

TEST(TrackTest, DerivesPathFieldsForArchiveFiles)
{
    const QString archivePath = u"/music/archive.zip"_s;
    const QString innerPath   = u"disc1/track02.OGG"_s;
    const QString path        = u"unpack://zip|%1|file://%2!%3"_s.arg(archivePath.size()).arg(archivePath, innerPath);

    const Track track{path};

    EXPECT_TRUE(track.isInArchive());
    EXPECT_EQ(track.archivePath(), archivePath);
    EXPECT_EQ(track.pathInArchive(), innerPath);
    EXPECT_EQ(track.filename(), u"track02"_s);
    EXPECT_EQ(track.directory(), u"disc1"_s);
    EXPECT_EQ(track.extension(), u"ogg"_s);
    EXPECT_EQ(track.prettyFilepath(), u"/music/archive.zip/disc1/track02.OGG"_s);
}

TEST(TrackTest, UsesCompactMetadataAccessors)
{
    Track track;
    track.setArtists({u"Artist A"_s, u"Artist B"_s});
    track.setAlbumArtists({u"Album Artist"_s});
    track.setGenres({u"Ambient"_s, u"Drone"_s});
    track.setComposers({u"Composer A"_s, u"Composer B"_s});
    track.setPerformers({u"Performer A"_s, u"Performer B"_s});

    EXPECT_TRUE(track.hasArtists());
    EXPECT_TRUE(track.hasAlbumArtists());
    EXPECT_TRUE(track.hasGenres());
    EXPECT_EQ(track.artistCount(), 2);
    EXPECT_EQ(track.artistAt(1), u"Artist B"_s);
    EXPECT_EQ(track.albumArtistCount(), 1);
    EXPECT_EQ(track.albumArtistAt(0), u"Album Artist"_s);
    EXPECT_EQ(track.genreCount(), 2);
    EXPECT_EQ(track.genreAt(0), u"Ambient"_s);
    EXPECT_EQ(track.artistsJoined(u" / "_s), u"Artist A / Artist B"_s);
    EXPECT_EQ(track.albumArtistsJoined(u" / "_s), u"Album Artist"_s);
    EXPECT_EQ(track.genresJoined(u" / "_s), u"Ambient / Drone"_s);
    EXPECT_EQ(track.composers(), (QStringList{u"Composer A"_s, u"Composer B"_s}));
    EXPECT_EQ(track.composer(), u"Composer A\037Composer B"_s);
    EXPECT_EQ(track.performers(), (QStringList{u"Performer A"_s, u"Performer B"_s}));
    EXPECT_EQ(track.performer(), u"Performer A\037Performer B"_s);
}

TEST(TrackTest, LazilyLoadsExtraTagsFromSerialisedData)
{
    Track source;
    source.addExtraTag(u"custom"_s, u"value"_s);
    source.addExtraTag(u"custom"_s, u"value2"_s);

    const QByteArray blob = source.serialiseExtraTags();

    Track loaded;
    loaded.storeExtraTags(blob);

    EXPECT_EQ(loaded.serialiseExtraTags(), blob);
    EXPECT_EQ(loaded.extraTag(u"CUSTOM"_s), (QStringList{u"value"_s, u"value2"_s}));

    loaded.addExtraTag(u"another"_s, u"entry"_s);
    EXPECT_EQ(loaded.extraTag(u"ANOTHER"_s), (QStringList{u"entry"_s}));
}

TEST(TrackTest, DeserialisesExtraTagsFromLegacyPayload)
{
    const QMap<QString, QStringList> tags{{u"ANOTHER"_s, {u"entry"_s}}, {u"custom"_s, {u"value"_s, u"value2"_s}}};
    const QByteArray blob = serialiseLegacyMap(tags);

    Track loaded;
    loaded.storeExtraTags(blob);

    EXPECT_TRUE(loaded.hasExtraTag(u"CUSTOM"_s));
    EXPECT_TRUE(loaded.hasExtraTag(u"custom"_s));
    EXPECT_EQ(loaded.extraTag(u"CUSTOM"_s), (QStringList{u"value"_s, u"value2"_s}));
    EXPECT_EQ(loaded.extraTag(u"another"_s), (QStringList{u"entry"_s}));

    const auto extraTags = loaded.extraTags();
    EXPECT_EQ(extraTags.size(), 2);
    EXPECT_EQ(extraTags.value(u"ANOTHER"_s), (QStringList{u"entry"_s}));
    EXPECT_EQ(extraTags.value(u"CUSTOM"_s), (QStringList{u"value"_s, u"value2"_s}));
}

TEST(TrackTest, DeserialisesExtraPropertiesFromLegacyPayload)
{
    const QMap<QString, QString> props{{u"BitDepth"_s, u"24"_s}, {u"mod_channels"_s, u"8"_s}};
    const QByteArray blob = serialiseLegacyMap(props);

    Track loaded;
    loaded.storeExtraProperties(blob);

    EXPECT_EQ(loaded.serialiseExtraProperties(), blob);
    EXPECT_TRUE(loaded.hasExtraProperty(u"BitDepth"_s));
    EXPECT_TRUE(loaded.hasExtraProperty(u"mod_channels"_s));

    const auto extraProps = loaded.extraProperties();
    EXPECT_EQ(extraProps.size(), 2);
    EXPECT_EQ(extraProps.value(u"BitDepth"_s), u"24"_s);
    EXPECT_EQ(extraProps.value(u"mod_channels"_s), u"8"_s);
}

TEST(TrackTest, DeserialisesEmptyExtraPropertyValueFromLegacyPayload)
{
    const QMap<QString, QString> props{{u"EmptyValue"_s, {}}, {u"Present"_s, u"value"_s}};
    const QByteArray blob = serialiseLegacyMap(props);

    Track loaded;
    loaded.storeExtraProperties(blob);

    EXPECT_EQ(loaded.serialiseExtraProperties(), blob);
    EXPECT_TRUE(loaded.hasExtraProperty(u"EmptyValue"_s));
    EXPECT_TRUE(loaded.hasExtraProperty(u"Present"_s));

    const auto extraProps = loaded.extraProperties();
    EXPECT_EQ(extraProps.size(), 2);
    EXPECT_EQ(extraProps.value(u"EmptyValue"_s), QString{});
    EXPECT_EQ(extraProps.value(u"Present"_s), u"value"_s);
}

TEST(TrackTest, SameIdentityAsUsesIdThenSegmentIdentity)
{
    Track dbTrackA{u"/music/a.flac"_s, 0};
    dbTrackA.setId(100);
    dbTrackA.setDuration(2000);
    dbTrackA.setOffset(0);
    dbTrackA.generateHash();

    Track dbTrackB{u"/music/b.flac"_s, 0};
    dbTrackB.setId(100);
    dbTrackB.setDuration(3000);
    dbTrackB.setOffset(1000);
    dbTrackB.generateHash();

    EXPECT_TRUE(dbTrackA.sameIdentityAs(dbTrackB));

    Track segmentTrackA{u"/music/cue.flac"_s, 0};
    segmentTrackA.setDuration(1500);
    segmentTrackA.setOffset(500);
    segmentTrackA.generateHash();

    Track segmentTrackB = segmentTrackA;
    EXPECT_TRUE(segmentTrackA.sameIdentityAs(segmentTrackB));

    segmentTrackB.setOffset(750);
    EXPECT_FALSE(segmentTrackA.sameIdentityAs(segmentTrackB));

    Track invalidTrack;
    EXPECT_FALSE(segmentTrackA.sameIdentityAs(invalidTrack));
}

TEST(TrackTest, ChapterSegmentsHaveExplicitIdentityAtOffsetZero)
{
    Track wholeFile{u"/music/book.m4b"_s, 0};
    wholeFile.setDuration(10000);

    Track chapter{u"/music/book.m4b"_s, 0};
    chapter.setIsChapter(true);
    chapter.setOffset(0);
    chapter.setDuration(5000);

    EXPECT_EQ(chapter.segmentType(), Track::SegmentType::Chapter);
    EXPECT_TRUE(chapter.isBoundedSegment());
    EXPECT_TRUE(chapter.isSameStreamSegment());
    EXPECT_NE(chapter.uniqueFilepath(), wholeFile.uniqueFilepath());
    EXPECT_EQ(chapter.uniqueFilepath(), u"/music/book.m4b#chapter=0:0"_s);
}

TEST(TrackTest, ContiguousSameFileSegmentRequiresSameStreamSegmentType)
{
    Track chapterOne{u"/music/book.m4b"_s, 0};
    chapterOne.setIsChapter(true);
    chapterOne.setOffset(0);
    chapterOne.setDuration(5000);

    Track chapterTwo{u"/music/book.m4b"_s, 1};
    chapterTwo.setIsChapter(true);
    chapterTwo.setOffset(5000);
    chapterTwo.setDuration(4000);

    EXPECT_TRUE(isContiguousSameFileSegment(chapterOne, chapterTwo));

    Track plainSubsong{u"/music/book.m4b"_s, 1};
    plainSubsong.setOffset(5000);
    plainSubsong.setDuration(4000);

    EXPECT_FALSE(isContiguousSameFileSegment(chapterOne, plainSubsong));

    Track cueTrack{u"/music/book.m4b"_s, 1};
    cueTrack.setCuePath(u"/music/book.cue"_s);
    cueTrack.setOffset(5000);
    cueTrack.setDuration(4000);

    EXPECT_FALSE(isContiguousSameFileSegment(chapterOne, cueTrack));
}

TEST(TrackTest, MigratesPooledMetadataToExplicitStore)
{
    Track track;
    track.setArtists({u"Artist A"_s, u"Artist B"_s});
    track.setAlbum(u"Shared Album"_s);
    track.setGenres({u"Ambient"_s});
    track.setComposers({u"Composer A"_s});
    track.setPerformers({u"Performer A"_s});
    track.addExtraTag(u"custom"_s, u"value"_s);

    auto sharedStore = std::make_shared<TrackMetadataStore>();
    track.setMetadataStore(sharedStore);

    EXPECT_EQ(track.metadataStore(), sharedStore);
    EXPECT_EQ(track.artists(), (QStringList{u"Artist A"_s, u"Artist B"_s}));
    EXPECT_EQ(track.album(), u"Shared Album"_s);
    EXPECT_EQ(track.genre(), u"Ambient"_s);
    EXPECT_EQ(track.composer(), u"Composer A"_s);
    EXPECT_EQ(track.performer(), u"Performer A"_s);
    EXPECT_EQ(track.extraTag(u"CUSTOM"_s), (QStringList{u"value"_s}));

    EXPECT_EQ(sharedStore->values(StringPool::Domain::Artist), (QStringList{u"Artist A"_s, u"Artist B"_s}));
    EXPECT_EQ(sharedStore->values(StringPool::Domain::Album), (QStringList{u"Shared Album"_s}));
    EXPECT_EQ(sharedStore->values(StringPool::Domain::Composer), (QStringList{u"Composer A"_s}));
    EXPECT_EQ(sharedStore->values(StringPool::Domain::Performer), (QStringList{u"Performer A"_s}));
    EXPECT_EQ(sharedStore->values(StringPool::Domain::ExtraTagKey), (QStringList{u"CUSTOM"_s}));
}

TEST(TrackTest, CalculatesEffectiveReplayGainForOpusHeaderGain)
{
    Track track;
    track.setRGTrackGain(5.0F);
    track.setRGAlbumGain(5.0F);
    track.setRGTrackPeak(0.255842F);
    track.setRGAlbumPeak(0.255842F);
    track.setExtraProperty(QString::fromLatin1(Constants::OpusHeaderGainQ78), u"-3205"_s);

    EXPECT_TRUE(track.hasOpusHeaderGain());
    EXPECT_NEAR(track.opusHeaderGainDb(), -12.5195F, 0.0002F);

    EXPECT_TRUE(track.hasEffectiveTrackGain());
    EXPECT_TRUE(track.hasEffectiveAlbumGain());
    EXPECT_TRUE(track.hasEffectiveTrackPeak());
    EXPECT_TRUE(track.hasEffectiveAlbumPeak());

    EXPECT_NEAR(track.effectiveRGTrackGain(), -7.5195F, 0.0002F);
    EXPECT_NEAR(track.effectiveRGAlbumGain(), -7.5195F, 0.0002F);

    const float expectedPeak = 0.255842F / std::pow(10.0F, track.opusHeaderGainDb() / 20.0F);
    EXPECT_NEAR(track.effectiveRGTrackPeak(), expectedPeak, 0.00001F);
    EXPECT_NEAR(track.effectiveRGAlbumPeak(), expectedPeak, 0.00001F);
    EXPECT_NEAR(track.techInfo(QString::fromLatin1(Constants::MetaData::OpusHeaderGain)).toDouble(), -12.5195, 0.0002);

    EXPECT_FLOAT_EQ(track.rgTrackGain(), 5.0F);
    EXPECT_FLOAT_EQ(track.rgAlbumGain(), 5.0F);
    EXPECT_FLOAT_EQ(track.rgTrackPeak(), 0.255842F);
    EXPECT_FLOAT_EQ(track.rgAlbumPeak(), 0.255842F);
}
} // namespace Fooyin::Testing
