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

#include "core/playlist/parsers/m3uparser.h"
#include "testutils.h"

#include <core/track.h>

#include <gtest/gtest.h>

#include <QBuffer>
#include <QDir>
#include <QFile>

namespace Fooyin::Testing {
using namespace Qt::StringLiterals;

class M3uParserTest : public ::testing::Test
{
public:
    M3uParserTest()
        : m_parser{std::make_unique<M3uParser>()}
    { }

protected:
    std::unique_ptr<PlaylistParser> m_parser;
};

TEST_F(M3uParserTest, StandardM3u)
{
    const QString filepath = u":/playlists/standardtest.m3u"_s;
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
        ASSERT_EQ(3, tracks.size());
    }
}

TEST_F(M3uParserTest, ExtendedM3u)
{
    const QString filepath = u":/playlists/extendedtest.m3u"_s;
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
        ASSERT_EQ(7, tracks.size());

        EXPECT_EQ(u"Rotten Apple", tracks.at(0).title());
        EXPECT_EQ(u"Alice in Chains", tracks.at(0).artist());
        EXPECT_EQ(u"Nutshell", tracks.at(1).title());
    }
}

TEST_F(M3uParserTest, RemoteEntriesStayRemote)
{
    QByteArray playlistData{"#EXTM3U\n"
                            "#EXTINF:-1,CVGM - 64Kb MP3 Stream - NL\n"
                            "https://slacker.cvgm.net/cvgm64\n"
                            "#EXTINF:-1,CVGM - 128Kb SHOUTcast Stream - USA\n"
                            "http://radio.cvgm.net:8080/\n"};
    QBuffer buffer{&playlistData};
    ASSERT_TRUE(buffer.open(QIODevice::ReadOnly | QIODevice::Text));

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [](const Track& track) {
        return track;
    };

    const auto tracks = m_parser->readPlaylist(&buffer, u"http://www.cvgm.net/static/media/misc/cvgm_streams.m3u"_s, {},
                                               readEntry, false);

    ASSERT_EQ(2, tracks.size());
    EXPECT_EQ(u"https://slacker.cvgm.net/cvgm64"_s, tracks.at(0).filepath());
    EXPECT_EQ(u"http://radio.cvgm.net:8080/"_s, tracks.at(1).filepath());
    EXPECT_TRUE(tracks.at(0).isRemote());
    EXPECT_TRUE(tracks.at(1).isRemote());
    EXPECT_EQ(u"CVGM"_s, tracks.at(0).artist());
    EXPECT_EQ(u"64Kb MP3 Stream"_s, tracks.at(0).title());
}

TEST_F(M3uParserTest, RelativeRemoteEntriesResolveAgainstPlaylistUrl)
{
    QByteArray playlistData{"#EXTM3U\n"
                            "streams/live.mp3\n"
                            "../backup/live.ogg\n"};
    QBuffer buffer{&playlistData};
    ASSERT_TRUE(buffer.open(QIODevice::ReadOnly | QIODevice::Text));

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [](const Track& track) {
        return track;
    };

    const auto tracks
        = m_parser->readPlaylist(&buffer, u"https://example.com/radio/listen/playlist.m3u"_s, {}, readEntry, false);

    ASSERT_EQ(2, tracks.size());
    EXPECT_EQ(u"https://example.com/radio/listen/streams/live.mp3"_s, tracks.at(0).filepath());
    EXPECT_EQ(u"https://example.com/radio/backup/live.ogg"_s, tracks.at(1).filepath());
    EXPECT_TRUE(tracks.at(0).isRemote());
    EXPECT_TRUE(tracks.at(1).isRemote());
}

TEST_F(M3uParserTest, RejectsHlsMediaPlaylist)
{
    QByteArray playlistData{"#EXTM3U\n"
                            "#EXT-X-TARGETDURATION:10\n"
                            "#EXTINF:10,\n"
                            "segment-001.ts\n"
                            "#EXTINF:10,\n"
                            "segment-002.ts\n"
                            "#EXT-X-ENDLIST\n"};
    QBuffer buffer{&playlistData};
    ASSERT_TRUE(buffer.open(QIODevice::ReadOnly | QIODevice::Text));

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [](const Track& track) {
        return track;
    };

    EXPECT_EQ(0, m_parser->countEntries(&buffer, u"https://example.com/live/playlist.m3u8"_s, {}));
    ASSERT_TRUE(buffer.seek(0));
    EXPECT_TRUE(
        m_parser->readPlaylist(&buffer, u"https://example.com/live/playlist.m3u8"_s, {}, readEntry, false).empty());
}

TEST_F(M3uParserTest, RejectsHlsMasterPlaylist)
{
    QByteArray playlistData{"#EXTM3U\n"
                            "#EXT-X-STREAM-INF:BANDWIDTH=1280000,RESOLUTION=640x360\n"
                            "low/playlist.m3u8\n"
                            "#EXT-X-STREAM-INF:BANDWIDTH=2560000,RESOLUTION=1280x720\n"
                            "high/playlist.m3u8\n"};
    QBuffer buffer{&playlistData};
    ASSERT_TRUE(buffer.open(QIODevice::ReadOnly | QIODevice::Text));

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [](const Track& track) {
        return track;
    };

    EXPECT_EQ(0, m_parser->countEntries(&buffer, u"https://example.com/master.m3u8"_s, {}));
    ASSERT_TRUE(buffer.seek(0));
    EXPECT_TRUE(m_parser->readPlaylist(&buffer, u"https://example.com/master.m3u8"_s, {}, readEntry, false).empty());
}

TEST_F(M3uParserTest, SavesExternalCueTracksAsSingleCueEntry)
{
    Track track1{u"/music/album.flac"_s};
    track1.setCuePath(u"/music/album.cue"_s);
    track1.setOffset(0);

    Track track2{u"/music/album.flac"_s};
    track2.setCuePath(u"/music/album.cue"_s);
    track2.setOffset(120000);

    QByteArray output;
    QBuffer buffer{&output};
    ASSERT_TRUE(buffer.open(QIODevice::WriteOnly | QIODevice::Text));

    m_parser->savePlaylist(&buffer, u"m3u"_s, {track1, track2}, QDir{u"/music"_s}, PlaylistParser::PathType::Absolute,
                           false);

    EXPECT_EQ(u"/music/album.cue\n"_s, QString::fromUtf8(output));
}

TEST_F(M3uParserTest, SavesEmbeddedCueTracksAsSingleFileEntry)
{
    Track track1{u"/music/album.flac"_s};
    track1.setCuePath(u"Embedded"_s);
    track1.setOffset(0);

    Track track2{u"/music/album.flac"_s};
    track2.setCuePath(u"Embedded"_s);
    track2.setOffset(120000);

    QByteArray output;
    QBuffer buffer{&output};
    ASSERT_TRUE(buffer.open(QIODevice::WriteOnly | QIODevice::Text));

    m_parser->savePlaylist(&buffer, u"m3u"_s, {track1, track2}, QDir{u"/music"_s}, PlaylistParser::PathType::Absolute,
                           false);

    EXPECT_EQ(u"/music/album.flac\n"_s, QString::fromUtf8(output));
}

TEST_F(M3uParserTest, ReadsExternalCueEntriesAsCueTracks)
{
    QByteArray playlistData{":/playlists/singlefiletest.cue\n"};
    QBuffer buffer{&playlistData};
    ASSERT_TRUE(buffer.open(QIODevice::ReadOnly | QIODevice::Text));

    const auto readTrack = [](const Track& track) {
        Track loaded{track};
        loaded.setDuration(600000);
        return loaded;
    };

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = readTrack;

    const auto tracks = m_parser->readPlaylist(&buffer, u"/tmp/test.m3u"_s, QDir{u"/tmp"_s}, readEntry, false);

    ASSERT_EQ(2, tracks.size());
    EXPECT_EQ(u":/playlists/My Bloody Valentine - Loveless.wav"_s, tracks.at(0).filepath());
    EXPECT_EQ(u":/playlists/singlefiletest.cue"_s, tracks.at(0).cuePath());
    EXPECT_EQ(u"01"_s, tracks.at(0).trackNumber());
    EXPECT_EQ(u"02"_s, tracks.at(1).trackNumber());
}

TEST_F(M3uParserTest, RoundTripsExternalCueTracks)
{
    const QString cuePath   = testFilePath(u"data/playlists/unreadableimage.cue"_s);
    const QString imagePath = testFilePath(u"data/playlists/unreadableimage.flac"_s);

    Track track1{imagePath};
    track1.setCuePath(cuePath);
    track1.setOffset(0);
    track1.setDuration(256000);
    track1.setTrackNumber(u"01"_s);

    Track track2{imagePath};
    track2.setCuePath(cuePath);
    track2.setOffset(256000);
    track2.setDuration(344000);
    track2.setTrackNumber(u"02"_s);

    QByteArray output;
    QBuffer writeBuffer{&output};
    ASSERT_TRUE(writeBuffer.open(QIODevice::WriteOnly | QIODevice::Text));

    m_parser->savePlaylist(&writeBuffer, u"m3u"_s, {track1, track2}, QDir{u"/music"_s},
                           PlaylistParser::PathType::Absolute, false);

    const auto readTrack = [](const Track& track) {
        Track loaded{track};
        loaded.setDuration(600000);
        return loaded;
    };

    QBuffer readBuffer{&output};
    ASSERT_TRUE(readBuffer.open(QIODevice::ReadOnly | QIODevice::Text));

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = readTrack;

    const auto tracks = m_parser->readPlaylist(&readBuffer, u"/tmp/test.m3u"_s, QDir{u"/tmp"_s}, readEntry, false);

    ASSERT_EQ(1, output.count('\n'));
    ASSERT_EQ(2, tracks.size());
    EXPECT_EQ(cuePath, QString::fromUtf8(output).trimmed());
    EXPECT_EQ(imagePath, tracks.at(0).filepath());
    EXPECT_EQ(0U, tracks.at(0).offset());
    EXPECT_EQ(192000U, tracks.at(0).duration());
    EXPECT_EQ(192000U, tracks.at(1).offset());
    EXPECT_EQ(408000U, tracks.at(1).duration());
}

TEST_F(M3uParserTest, ReadsEmbeddedCueEntriesAsCueTracks)
{
    QByteArray playlistData{"/music/album.flac\n"};
    QBuffer buffer{&playlistData};
    ASSERT_TRUE(buffer.open(QIODevice::ReadOnly | QIODevice::Text));

    const auto readTrack = [](const Track& track) {
        Track loaded{track};
        loaded.setDuration(600000);
        QFile cueFile{testFilePath(u"data/playlists/singlefiletest.cue"_s)};
        if(cueFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            loaded.addExtraTag(u"CUESHEET"_s, QString::fromUtf8(cueFile.readAll()));
        }
        return loaded;
    };

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = readTrack;

    const auto tracks
        = m_parser->readPlaylist(&buffer, u"/playlists/test.m3u"_s, QDir{u"/playlists"_s}, readEntry, false);

    ASSERT_EQ(2, tracks.size());
    EXPECT_EQ(u"/music/album.flac"_s, tracks.at(0).filepath());
    EXPECT_EQ(u"Embedded"_s, tracks.at(0).cuePath());
    EXPECT_EQ(u"01"_s, tracks.at(0).trackNumber());
    EXPECT_EQ(u"02"_s, tracks.at(1).trackNumber());
}
} // namespace Fooyin::Testing
