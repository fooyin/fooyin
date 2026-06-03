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

#include "core/playlist/parsers/plsparser.h"

#include <core/track.h>

#include <gtest/gtest.h>

#include <QBuffer>
#include <QDir>
#include <QFile>

#include <algorithm>
#include <memory>

namespace Fooyin::Testing {
using namespace Qt::StringLiterals;

class PlsParserTest : public ::testing::Test
{
public:
    PlsParserTest()
        : m_parser{std::make_unique<PlsParser>()}
    { }

protected:
    std::unique_ptr<PlaylistParser> m_parser;
};

TEST_F(PlsParserTest, ParsesRemoteEntriesAndMetadata)
{
    QByteArray playlistData{"[playlist]\n"
                            "File1=https://stream.example.com/main\n"
                            "Title1=Example Radio - Main Stream\n"
                            "Length1=-1\n"
                            "File2=http://backup.example.com/live.mp3\n"
                            "Title2=Backup Stream\n"
                            "Length2=123\n"
                            "NumberOfEntries=2\n"
                            "Version=2\n"};
    QBuffer buffer{&playlistData};
    ASSERT_TRUE(buffer.open(QIODevice::ReadOnly | QIODevice::Text));

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [](const Track& track) {
        return track;
    };

    EXPECT_EQ(2, m_parser->countEntries(&buffer, {}, {}));
    ASSERT_TRUE(buffer.seek(0));

    const auto tracks
        = m_parser->readPlaylist(&buffer, u"https://example.com/stations/list.pls"_s, {}, readEntry, false);

    ASSERT_EQ(2, tracks.size());
    EXPECT_EQ(u"https://stream.example.com/main"_s, tracks.at(0).filepath());
    EXPECT_TRUE(tracks.at(0).artist().isEmpty());
    EXPECT_EQ(u"Example Radio - Main Stream"_s, tracks.at(0).title());
    EXPECT_EQ(0, tracks.at(0).duration());
    EXPECT_EQ(u"http://backup.example.com/live.mp3"_s, tracks.at(1).filepath());
    EXPECT_EQ(u"Backup Stream"_s, tracks.at(1).title());
    EXPECT_EQ(123000, tracks.at(1).duration());
    EXPECT_TRUE(tracks.at(0).isRemote());
    EXPECT_TRUE(tracks.at(1).isRemote());
}

TEST_F(PlsParserTest, ParsesLongRemotePlaylistFixture)
{
    const QString filepath = u":/playlists/longremote.pls"_s;
    QFile file{filepath};
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [](const Track& track) {
        return track;
    };

    EXPECT_EQ(12, m_parser->countEntries(&file, filepath, {}));
    ASSERT_TRUE(file.seek(0));

    const auto tracks = m_parser->readPlaylist(&file, filepath, {}, readEntry, false);

    ASSERT_EQ(12, tracks.size());
    EXPECT_EQ(u"https://streams.example.org/jazz/main-128.mp3"_s, tracks.at(0).filepath());
    EXPECT_TRUE(tracks.at(0).artist().isEmpty());
    EXPECT_EQ(u"Example Jazz - Main 128k"_s, tracks.at(0).title());
    EXPECT_EQ(u"https://cdn.example.org/archive/show-001.mp3"_s, tracks.at(8).filepath());
    EXPECT_EQ(5400000, tracks.at(8).duration());
    EXPECT_EQ(u"https://relay.example.net/fallback/live"_s, tracks.at(11).filepath());
    EXPECT_TRUE(std::ranges::all_of(tracks, [](const Track& track) { return track.isRemote(); }));
}

TEST_F(PlsParserTest, RelativeRemoteEntriesResolveAgainstPlaylistUrl)
{
    QByteArray playlistData{"[playlist]\n"
                            "File1=streams/live.mp3\n"
                            "File2=../backup/live.ogg\n"};
    QBuffer buffer{&playlistData};
    ASSERT_TRUE(buffer.open(QIODevice::ReadOnly | QIODevice::Text));

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [](const Track& track) {
        return track;
    };

    const auto tracks
        = m_parser->readPlaylist(&buffer, u"https://example.com/radio/listen/playlist.pls"_s, {}, readEntry, false);

    ASSERT_EQ(2, tracks.size());
    EXPECT_EQ(u"https://example.com/radio/listen/streams/live.mp3"_s, tracks.at(0).filepath());
    EXPECT_EQ(u"https://example.com/radio/backup/live.ogg"_s, tracks.at(1).filepath());
}

TEST_F(PlsParserTest, ParsesMixedRelativeFixtureAgainstRemoteBase)
{
    const QString filepath = u":/playlists/mixedrelative.pls"_s;
    QFile file{filepath};
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [](const Track& track) {
        return track;
    };

    const auto tracks
        = m_parser->readPlaylist(&file, u"https://example.com/radio/listen/mixedrelative.pls"_s, {}, readEntry, false);

    ASSERT_EQ(8, tracks.size());
    EXPECT_EQ(u"https://example.com/radio/listen/streams/main.mp3"_s, tracks.at(0).filepath());
    EXPECT_EQ(u"https://example.com/radio/listen/streams/high.aac"_s, tracks.at(1).filepath());
    EXPECT_EQ(u"https://example.com/radio/backup/low.ogg"_s, tracks.at(2).filepath());
    EXPECT_EQ(u"https://example.com/radio/listen/subdir/deep/live.opus"_s, tracks.at(3).filepath());
    EXPECT_EQ(u"https://example.com/absolute/path/local-file.flac"_s, tracks.at(4).filepath());
    EXPECT_EQ(u"https://example.com/radio/listen/relative/local-file.mp3"_s, tracks.at(5).filepath());
    EXPECT_EQ(u"https://external.example.com/direct/live"_s, tracks.at(6).filepath());
    EXPECT_EQ(u"http://external.example.com:8000/relay"_s, tracks.at(7).filepath());
}

TEST_F(PlsParserTest, SavesPlaylist)
{
    Track first{u"https://stream.example.com/main"_s};
    first.setArtists({u"Example Radio"_s});
    first.setTitle(u"Main Stream"_s);

    Track second{u"/music/local.mp3"_s};
    second.setTitle(u"Local Track"_s);
    second.setDuration(42000);

    QByteArray output;
    QBuffer buffer{&output};
    ASSERT_TRUE(buffer.open(QIODevice::WriteOnly | QIODevice::Text));

    m_parser->savePlaylist(&buffer, u"pls"_s, {first, second}, QDir{u"/music"_s}, PlaylistParser::PathType::Auto, true);

    const QString saved = QString::fromUtf8(output);
    const QString expected{u"[playlist]\n"
                           "File1=https://stream.example.com/main\n"
                           "Title1=Main Stream\n"
                           "File2=local.mp3\n"
                           "Title2=Local Track\n"
                           "Length2=42\n"
                           "NumberOfEntries=2\n"
                           "Version=2\n"_s};
    EXPECT_EQ(expected, saved);
}
} // namespace Fooyin::Testing
