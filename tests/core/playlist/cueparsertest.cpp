/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include "core/playlist/parsers/cueparser.h"
#include "testutils.h"

#include <core/track.h>

#include <gtest/gtest.h>

#include <QBuffer>
#include <QDir>
#include <QFile>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
class CueParserTest : public ::testing::Test
{
public:
    CueParserTest()
        : m_parser{std::make_unique<CueParser>()}
    { }

protected:
    std::unique_ptr<PlaylistParser> m_parser;
};

TEST_F(CueParserTest, SingleCue)
{
    const QString filepath = u":/playlists/singlefiletest.cue"_s;
    QFile file{filepath};
    if(file.open(QIODevice::ReadOnly)) {
        QDir dir{filepath};
        dir.cdUp();

        const auto readTrack = [](const Track& track) {
            return track;
        };

        PlaylistParser::ReadPlaylistEntry readEntry;
        readEntry.readTrack = readTrack;

        const auto tracks = m_parser->readPlaylist(&file, filepath, dir, readEntry, false);
        ASSERT_EQ(2, tracks.size());

        EXPECT_EQ(1991, tracks.at(0).year());
        EXPECT_EQ(u"Alternative", tracks.at(0).genre());
        EXPECT_EQ(u"Loveless", tracks.at(0).album());
        EXPECT_EQ(u"My Bloody Valentine"_s, tracks.at(0).artist());
        EXPECT_EQ(u"My Bloody Valentine"_s, tracks.at(0).albumArtist());
        EXPECT_EQ(u"Only Shallow", tracks.at(0).title());

        EXPECT_EQ(tracks.at(1).discNumber(), u"1"_s);
        EXPECT_EQ(tracks.at(1).trackNumber(), u"02"_s);
    }
}

TEST_F(CueParserTest, CueUsesGlobalPerformerAsArtistWhenTrackPerformersAreMissing)
{
    const QString cueSheet = uR"(PERFORMER "Global Artist"
TITLE "Global Album"
FILE "album.flac" FLAC
  TRACK 01 AUDIO
    TITLE "Track One"
    INDEX 01 00:00:00
  TRACK 02 AUDIO
    TITLE "Track Two"
    INDEX 01 01:00:00
)"_s;

    QByteArray cueData = cueSheet.toUtf8();
    QBuffer buffer{&cueData};
    ASSERT_TRUE(buffer.open(QIODevice::ReadOnly | QIODevice::Text));

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [](const Track& track) {
        Track loaded{track};
        loaded.setDuration(180000);
        return loaded;
    };

    const auto tracks = m_parser->readPlaylist(&buffer, u"/music/album.flac"_s, {}, readEntry, false);
    ASSERT_EQ(2, tracks.size());

    EXPECT_EQ(u"Global Artist"_s, tracks.at(0).artist());
    EXPECT_TRUE(tracks.at(0).albumArtist().isEmpty());
    EXPECT_EQ(u"Global Artist"_s, tracks.at(1).artist());
    EXPECT_TRUE(tracks.at(1).albumArtist().isEmpty());
}

TEST_F(CueParserTest, CueKeepsGlobalPerformerAsAlbumArtistWhenTrackPerformersExist)
{
    const QString cueSheet = uR"(PERFORMER "Album Artist"
TITLE "Global Album"
FILE "album.flac" FLAC
  TRACK 01 AUDIO
    TITLE "Track One"
    PERFORMER "Track Artist"
    INDEX 01 00:00:00
  TRACK 02 AUDIO
    TITLE "Track Two"
    INDEX 01 01:00:00
)"_s;

    QByteArray cueData = cueSheet.toUtf8();
    QBuffer buffer{&cueData};
    ASSERT_TRUE(buffer.open(QIODevice::ReadOnly | QIODevice::Text));

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [](const Track& track) {
        Track loaded{track};
        loaded.setDuration(180000);
        return loaded;
    };

    const auto tracks = m_parser->readPlaylist(&buffer, u"/music/album.flac"_s, {}, readEntry, false);
    ASSERT_EQ(2, tracks.size());

    EXPECT_EQ(u"Track Artist"_s, tracks.at(0).artist());
    EXPECT_EQ(u"Album Artist"_s, tracks.at(0).albumArtist());
    EXPECT_TRUE(tracks.at(1).artist().isEmpty());
    EXPECT_EQ(u"Album Artist"_s, tracks.at(1).albumArtist());
}

TEST_F(CueParserTest, UnreadableCueImageTracksAreDisabled)
{
    const QString cuePath = testFilePath(u"data/playlists/unreadableimage.cue"_s);
    QFile cueFile{cuePath};
    ASSERT_TRUE(cueFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QDir dir{cuePath};
    dir.cdUp();

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [](const Track& track) {
        return track;
    };
    readEntry.canLoadTrack = [](const Track&) {
        return false;
    };

    const auto tracks = m_parser->readPlaylist(&cueFile, cuePath, dir, readEntry, true);
    ASSERT_EQ(2, tracks.size());
    EXPECT_FALSE(tracks.at(0).isEnabled());
    EXPECT_FALSE(tracks.at(1).isEnabled());
    EXPECT_EQ(testFilePath(u"data/playlists/unreadableimage.flac"_s), tracks.at(0).filepath());
    EXPECT_EQ(u"Track 1"_s, tracks.at(0).title());
    EXPECT_EQ(u"Track 2"_s, tracks.at(1).title());
}

TEST_F(CueParserTest, MissingCueImageIsSkippedWhenConfigured)
{
    const QString cuePath = testFilePath(u"data/playlists/missingimage.cue"_s);
    QFile cueFile{cuePath};
    ASSERT_TRUE(cueFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QDir dir{cuePath};
    dir.cdUp();

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [](const Track& track) {
        return track;
    };
    readEntry.canLoadTrack = [](const Track&) {
        return false;
    };

    const auto tracks = m_parser->readPlaylist(&cueFile, cuePath, dir, readEntry, true);
    EXPECT_TRUE(tracks.empty());
}

TEST_F(CueParserTest, EmbeddedCuePreservesDiscNumberFromSourceTrackWhenCueOmitsIt)
{
    QFile cueFile{testFilePath(u"data/playlists/singlefiletest.cue"_s)};
    ASSERT_TRUE(cueFile.open(QIODevice::ReadOnly | QIODevice::Text));

    QString cueSheet = QString::fromUtf8(cueFile.readAll());
    cueSheet.remove(u"REM COMMENT \"ExactAudioCopy v0.95b4\"\n"_s);
    cueSheet.remove(u"REM DATE 1991\n"_s);
    cueSheet.remove(u"REM DISCNUMBER 1\n"_s);
    cueSheet.remove(u"REM GENRE Alternative\n"_s);

    QByteArray cueData = cueSheet.toUtf8();
    QBuffer buffer{&cueData};
    ASSERT_TRUE(buffer.open(QIODevice::ReadOnly | QIODevice::Text));

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [](const Track& track) {
        Track loaded{track};
        loaded.setDuration(600000);
        loaded.setComment(u"Source Comment"_s);
        loaded.setComposers({u"Source Composer"_s});
        loaded.setDate(u"2001"_s);
        loaded.setDiscNumber(u"1"_s);
        loaded.setDiscTotal(u"2"_s);
        loaded.setGenres({u"Source Genre"_s});
        loaded.setRGAlbumGain(-7.25F);
        loaded.setRGAlbumPeak(0.91F);
        return loaded;
    };

    const auto tracks = m_parser->readPlaylist(&buffer, u"/music/album.flac"_s, {}, readEntry, false);
    ASSERT_EQ(2, tracks.size());

    EXPECT_EQ(u"Source Comment"_s, tracks.at(0).comment());
    EXPECT_EQ(u"Source Composer"_s, tracks.at(0).composer());
    EXPECT_EQ(u"2001"_s, tracks.at(0).date());
    EXPECT_EQ(u"1"_s, tracks.at(0).discNumber());
    EXPECT_EQ(u"2"_s, tracks.at(0).discTotal());
    EXPECT_EQ(u"Source Genre"_s, tracks.at(0).genre());
    EXPECT_FLOAT_EQ(-7.25F, tracks.at(0).rgAlbumGain());
    EXPECT_FLOAT_EQ(0.91F, tracks.at(0).rgAlbumPeak());

    EXPECT_EQ(u"Source Comment"_s, tracks.at(1).comment());
    EXPECT_EQ(u"Source Composer"_s, tracks.at(1).composer());
    EXPECT_EQ(u"2001"_s, tracks.at(1).date());
    EXPECT_EQ(u"1"_s, tracks.at(1).discNumber());
    EXPECT_EQ(u"2"_s, tracks.at(1).discTotal());
    EXPECT_EQ(u"Source Genre"_s, tracks.at(1).genre());
    EXPECT_FLOAT_EQ(-7.25F, tracks.at(1).rgAlbumGain());
    EXPECT_FLOAT_EQ(0.91F, tracks.at(1).rgAlbumPeak());
}

TEST_F(CueParserTest, EmbeddedCuePreservesTrackLevelComposerWhenAlbumComposerIsMissing)
{
    QFile cueFile{testFilePath(u"data/playlists/singlefiletest.cue"_s)};
    ASSERT_TRUE(cueFile.open(QIODevice::ReadOnly | QIODevice::Text));

    QString cueSheet = QString::fromUtf8(cueFile.readAll());
    cueSheet.replace(u"    TITLE \"Only Shallow\"\n"_s,
                     u"    TITLE \"Only Shallow\"\n    COMPOSER \"Track Writer\"\n"_s);

    QByteArray cueData = cueSheet.toUtf8();
    QBuffer buffer{&cueData};
    ASSERT_TRUE(buffer.open(QIODevice::ReadOnly | QIODevice::Text));

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [](const Track& track) {
        Track loaded{track};
        loaded.setDuration(600000);
        loaded.setComposers({u"Source Composer"_s});
        return loaded;
    };

    const auto tracks = m_parser->readPlaylist(&buffer, u"/music/album.flac"_s, {}, readEntry, false);
    ASSERT_EQ(2, tracks.size());

    EXPECT_EQ(u"Track Writer"_s, tracks.at(0).composer());
    EXPECT_EQ(u"Source Composer"_s, tracks.at(1).composer());
}

TEST_F(CueParserTest, CueAppliesTrailingAlbumMetadataToAllTracks)
{
    const QString cuePath = testFilePath(u"data/playlists/trailingalbummetadata.cue"_s);
    QFile cueFile{cuePath};
    ASSERT_TRUE(cueFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QDir dir{cuePath};
    dir.cdUp();

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [](const Track& track) {
        Track loaded{track};
        loaded.setDuration(600000);
        return loaded;
    };

    const auto tracks = m_parser->readPlaylist(&cueFile, cuePath, dir, readEntry, false);
    ASSERT_EQ(2, tracks.size());

    EXPECT_EQ(u"Album Name"_s, tracks.at(0).album());
    EXPECT_EQ(u"Album Name"_s, tracks.at(1).album());
    EXPECT_EQ(u"Fear Factory"_s, tracks.at(0).albumArtist());
    EXPECT_EQ(u"Fear Factory"_s, tracks.at(1).albumArtist());
    EXPECT_EQ(u"Rock"_s, tracks.at(0).genre());
    EXPECT_EQ(u"Rock"_s, tracks.at(1).genre());
    EXPECT_EQ(u"1995"_s, tracks.at(0).date());
    EXPECT_EQ(u"1995"_s, tracks.at(1).date());
    EXPECT_EQ(u"Track One"_s, tracks.at(0).title());
    EXPECT_EQ(u"Track Two"_s, tracks.at(1).title());
}

TEST_F(CueParserTest, CueParsesMultipleFileSections)
{
    const QString cuePath = testFilePath(u"data/playlists/multifilealbum.cue"_s);
    const QString sideA   = testFilePath(u"data/playlists/multifile-side-a.bin"_s);
    const QString sideB   = testFilePath(u"data/playlists/multifile-side-b.flac"_s);
    QFile cueFile{cuePath};
    ASSERT_TRUE(cueFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QDir dir{cuePath};
    dir.cdUp();

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [&sideA, &sideB](const Track& track) {
        Track loaded{track};
        if(track.filepath() == sideA) {
            loaded.setDuration(420000);
        }
        else if(track.filepath() == sideB) {
            loaded.setDuration(540000);
        }
        return loaded;
    };

    const auto tracks = m_parser->readPlaylist(&cueFile, cuePath, dir, readEntry, false);
    ASSERT_EQ(4, tracks.size());

    EXPECT_EQ(sideA, tracks.at(0).filepath());
    EXPECT_EQ(sideA, tracks.at(1).filepath());
    EXPECT_EQ(sideB, tracks.at(2).filepath());
    EXPECT_EQ(sideB, tracks.at(3).filepath());

    EXPECT_EQ(u"01"_s, tracks.at(0).trackNumber());
    EXPECT_EQ(u"02"_s, tracks.at(1).trackNumber());
    EXPECT_EQ(u"03"_s, tracks.at(2).trackNumber());
    EXPECT_EQ(u"04"_s, tracks.at(3).trackNumber());

    EXPECT_EQ(u"Split Horizon"_s, tracks.at(0).album());
    EXPECT_EQ(u"Split Horizon"_s, tracks.at(3).album());
    EXPECT_EQ(u"Northbound"_s, tracks.at(0).albumArtist());
    EXPECT_EQ(u"Ambient"_s, tracks.at(2).genre());
    EXPECT_EQ(u"2007"_s, tracks.at(3).date());

    EXPECT_EQ(0U, tracks.at(0).offset());
    EXPECT_EQ(195000U, tracks.at(1).offset());
    EXPECT_EQ(0U, tracks.at(2).offset());
    EXPECT_EQ(240000U, tracks.at(3).offset());

    EXPECT_EQ(195000U, tracks.at(0).duration());
    EXPECT_EQ(225000U, tracks.at(1).duration());
    EXPECT_EQ(240000U, tracks.at(2).duration());
    EXPECT_EQ(300000U, tracks.at(3).duration());
}

TEST_F(CueParserTest, CueIgnoresPregapIndexesWhenIndex01IsPresent)
{
    const QString cuePath = testFilePath(u"data/playlists/pregapindexes.cue"_s);
    QFile cueFile{cuePath};
    ASSERT_TRUE(cueFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QDir dir{cuePath};
    dir.cdUp();

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [](const Track& track) {
        Track loaded{track};
        loaded.setDuration(600000);
        return loaded;
    };

    const auto tracks = m_parser->readPlaylist(&cueFile, cuePath, dir, readEntry, false);
    ASSERT_EQ(3, tracks.size());

    EXPECT_EQ(1000U, tracks.at(0).offset());
    EXPECT_EQ(182000U, tracks.at(1).offset());
    EXPECT_EQ(365000U, tracks.at(2).offset());

    EXPECT_EQ(181000U, tracks.at(0).duration());
    EXPECT_EQ(183000U, tracks.at(1).duration());
    EXPECT_EQ(235000U, tracks.at(2).duration());
}

TEST_F(CueParserTest, CueParsesTrackReplayGainWithoutLeakingAcrossTracks)
{
    const QString cuePath = testFilePath(u"data/playlists/trackreplaygain.cue"_s);
    QFile cueFile{cuePath};
    ASSERT_TRUE(cueFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QDir dir{cuePath};
    dir.cdUp();

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [](const Track& track) {
        Track loaded{track};
        loaded.setDuration(360000);
        return loaded;
    };

    const auto tracks = m_parser->readPlaylist(&cueFile, cuePath, dir, readEntry, false);
    ASSERT_EQ(3, tracks.size());

    EXPECT_FLOAT_EQ(-8.5F, tracks.at(0).rgAlbumGain());
    EXPECT_FLOAT_EQ(0.987F, tracks.at(1).rgAlbumPeak());

    EXPECT_TRUE(tracks.at(0).hasTrackGain());
    EXPECT_TRUE(tracks.at(0).hasTrackPeak());
    EXPECT_FLOAT_EQ(-7.0F, tracks.at(0).rgTrackGain());
    EXPECT_FLOAT_EQ(0.91F, tracks.at(0).rgTrackPeak());

    EXPECT_TRUE(tracks.at(1).hasTrackGain());
    EXPECT_TRUE(tracks.at(1).hasTrackPeak());
    EXPECT_FLOAT_EQ(-6.0F, tracks.at(1).rgTrackGain());
    EXPECT_FLOAT_EQ(0.92F, tracks.at(1).rgTrackPeak());

    EXPECT_FALSE(tracks.at(2).hasTrackGain());
    EXPECT_FALSE(tracks.at(2).hasTrackPeak());
}

TEST_F(CueParserTest, CueAppliesTrailingAlbumMetadataAcrossMultipleFileSections)
{
    const QString cuePath = testFilePath(u"data/playlists/multifiletrailingmetadata.cue"_s);
    const QString sideA   = testFilePath(u"data/playlists/multifile-side-a.bin"_s);
    const QString sideB   = testFilePath(u"data/playlists/multifile-side-b.flac"_s);
    QFile cueFile{cuePath};
    ASSERT_TRUE(cueFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QDir dir{cuePath};
    dir.cdUp();

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [&sideA, &sideB](const Track& track) {
        Track loaded{track};
        if(track.filepath() == sideA) {
            loaded.setDuration(300000);
        }
        else if(track.filepath() == sideB) {
            loaded.setDuration(480000);
        }
        return loaded;
    };

    const auto tracks = m_parser->readPlaylist(&cueFile, cuePath, dir, readEntry, false);
    ASSERT_EQ(4, tracks.size());

    EXPECT_EQ(u"Late Signals"_s, tracks.at(0).album());
    EXPECT_EQ(u"Late Signals"_s, tracks.at(3).album());
    EXPECT_EQ(u"Northbound"_s, tracks.at(0).albumArtist());
    EXPECT_EQ(u"Northbound"_s, tracks.at(3).albumArtist());
    EXPECT_EQ(u"Ambient"_s, tracks.at(1).genre());
    EXPECT_EQ(u"Ambient"_s, tracks.at(2).genre());
    EXPECT_EQ(u"2008"_s, tracks.at(0).date());
    EXPECT_EQ(u"2008"_s, tracks.at(3).date());
}

TEST_F(CueParserTest, CueParsesLastLineWithoutTrailingNewline)
{
    const QString cuePath = testFilePath(u"data/playlists/notrailingnewline.cue"_s);
    QFile cueFile{cuePath};
    ASSERT_TRUE(cueFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QDir dir{cuePath};
    dir.cdUp();

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [](const Track& track) {
        Track loaded{track};
        loaded.setDuration(300000);
        return loaded;
    };

    const auto tracks = m_parser->readPlaylist(&cueFile, cuePath, dir, readEntry, false);
    ASSERT_EQ(2, tracks.size());

    EXPECT_EQ(u"First"_s, tracks.at(0).title());
    EXPECT_EQ(u"Second"_s, tracks.at(1).title());
    EXPECT_EQ(150000U, tracks.at(0).duration());
    EXPECT_EQ(150000U, tracks.at(1).duration());
}

TEST_F(CueParserTest, CueRetargetsReferencedWavToMatchingFlacImage)
{
    const QString cuePath  = testFilePath(u"data/playlists/wavreferenceflac.cue"_s);
    const QString flacPath = testFilePath(u"data/playlists/wavreferenceflac.flac"_s);
    QFile cueFile{cuePath};
    ASSERT_TRUE(cueFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QDir dir{cuePath};
    dir.cdUp();

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [&flacPath](const Track& track) {
        Track loaded{track};
        EXPECT_EQ(flacPath, track.filepath());
        loaded.setDuration(360000);
        return loaded;
    };

    const auto tracks = m_parser->readPlaylist(&cueFile, cuePath, dir, readEntry, false);
    ASSERT_EQ(2, tracks.size());

    EXPECT_EQ(flacPath, tracks.at(0).filepath());
    EXPECT_EQ(flacPath, tracks.at(1).filepath());
    EXPECT_EQ(180000U, tracks.at(0).duration());
    EXPECT_EQ(180000U, tracks.at(1).duration());
}
} // namespace Fooyin::Testing
