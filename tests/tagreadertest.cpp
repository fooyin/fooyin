/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "testutils.h"

#include <core/engine/taglibparser.h>
#include <core/track.h>

#include <gtest/gtest.h>

// clazy:excludeall=returning-void-expression
namespace Fooyin::Testing {
class TagReaderTest : public ::testing::Test
{
protected:
    TagLibReader m_parser;
};

TEST_F(TagReaderTest, AiffRead)
{
    const TempResource file{QStringLiteral(":/audio/audiotest.aiff")};
    file.checkValid();

    Track track{file.fileName()};
    ASSERT_TRUE(m_parser.readMetaData(track));

    EXPECT_EQ(track.codec(), QStringLiteral("AIFF"));
    EXPECT_EQ(track.title(), QStringLiteral("AIFF Test"));
    EXPECT_EQ(track.album(), QStringLiteral("Fooyin Audio Tests"));
    EXPECT_EQ(track.albumArtist(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.artist(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.date(), QStringLiteral("2023"));
    EXPECT_EQ(track.trackNumber(), 1);
    EXPECT_EQ(track.trackTotal(), 7);
    EXPECT_EQ(track.discNumber(), 1);
    EXPECT_EQ(track.discTotal(), 1);
    EXPECT_EQ(track.genre(), QStringLiteral("Testing"));
    EXPECT_EQ(track.performer(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.composer(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.comment(), QStringLiteral("A fooyin test"));
    EXPECT_GT(track.duration(), 0);

    const auto testTag = track.extraTag(QStringLiteral("TEST"));
    ASSERT_TRUE(!testTag.isEmpty());
    EXPECT_EQ(testTag.front(), QStringLiteral("A custom tag"));
}

TEST_F(TagReaderTest, FlacRead)
{
    const TempResource file{QStringLiteral(":/audio/audiotest.flac")};
    file.checkValid();

    Track track{file.fileName()};
    ASSERT_TRUE(m_parser.readMetaData(track));

    EXPECT_EQ(track.codec(), QStringLiteral("FLAC"));
    EXPECT_EQ(track.title(), QStringLiteral("FLAC Test"));
    EXPECT_EQ(track.album(), QStringLiteral("Fooyin Audio Tests"));
    EXPECT_EQ(track.albumArtist(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.artist(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.date(), QStringLiteral("2023"));
    EXPECT_EQ(track.trackNumber(), 2);
    EXPECT_EQ(track.trackTotal(), 7);
    EXPECT_EQ(track.discNumber(), 1);
    EXPECT_EQ(track.discTotal(), 1);
    EXPECT_EQ(track.genre(), QStringLiteral("Testing"));
    EXPECT_EQ(track.performer(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.composer(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.comment(), QStringLiteral("A fooyin test"));
    EXPECT_GT(track.duration(), 0);

    const auto testTag = track.extraTag(QStringLiteral("TEST"));
    ASSERT_TRUE(!testTag.isEmpty());
    EXPECT_EQ(testTag.front(), QStringLiteral("A custom tag"));
}

TEST_F(TagReaderTest, M4aRead)
{
    const TempResource file{QStringLiteral(":/audio/audiotest.m4a")};
    file.checkValid();

    Track track{file.fileName()};
    ASSERT_TRUE(m_parser.readMetaData(track));

    EXPECT_EQ(track.codec(), QStringLiteral("AAC"));
    EXPECT_EQ(track.title(), QStringLiteral("M4A Test"));
    EXPECT_EQ(track.album(), QStringLiteral("Fooyin Audio Tests"));
    EXPECT_EQ(track.albumArtist(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.artist(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.date(), QStringLiteral("2023"));
    EXPECT_EQ(track.trackNumber(), 3);
    EXPECT_EQ(track.trackTotal(), 7);
    EXPECT_EQ(track.discNumber(), 1);
    EXPECT_EQ(track.discTotal(), 1);
    EXPECT_EQ(track.genre(), QStringLiteral("Testing"));
    EXPECT_EQ(track.composer(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.comment(), QStringLiteral("A fooyin test"));
    EXPECT_GT(track.duration(), 0);

    const auto testTag = track.extraTag(QStringLiteral("TEST"));
    ASSERT_TRUE(!testTag.isEmpty());
    EXPECT_EQ(testTag.front(), QStringLiteral("A custom tag"));
}

TEST_F(TagReaderTest, Mp3Read)
{
    const TempResource file{QStringLiteral(":/audio/audiotest.mp3")};
    file.checkValid();

    Track track{file.fileName()};
    ASSERT_TRUE(m_parser.readMetaData(track));

    EXPECT_EQ(track.codec(), QStringLiteral("MP3"));
    EXPECT_EQ(track.title(), QStringLiteral("MP3 Test"));
    EXPECT_EQ(track.album(), QStringLiteral("Fooyin Audio Tests"));
    EXPECT_EQ(track.albumArtist(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.artist(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.date(), QStringLiteral("2023"));
    EXPECT_EQ(track.trackNumber(), 4);
    EXPECT_EQ(track.trackTotal(), 7);
    EXPECT_EQ(track.discNumber(), 1);
    EXPECT_EQ(track.discTotal(), 1);
    EXPECT_EQ(track.genre(), QStringLiteral("Testing"));
    EXPECT_EQ(track.performer(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.composer(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.comment(), QStringLiteral("A fooyin test"));
    EXPECT_GT(track.duration(), 0);

    const auto testTag = track.extraTag(QStringLiteral("TEST"));
    ASSERT_TRUE(!testTag.isEmpty());
    EXPECT_EQ(testTag.front(), QStringLiteral("A custom tag"));
}

TEST_F(TagReaderTest, OggRead)
{
    const TempResource file{QStringLiteral(":/audio/audiotest.ogg")};
    file.checkValid();

    Track track{file.fileName()};
    ASSERT_TRUE(m_parser.readMetaData(track));

    EXPECT_EQ(track.codec(), QStringLiteral("Vorbis"));
    EXPECT_EQ(track.title(), QStringLiteral("OGG Test"));
    EXPECT_EQ(track.album(), QStringLiteral("Fooyin Audio Tests"));
    EXPECT_EQ(track.albumArtist(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.artist(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.date(), QStringLiteral("2023"));
    EXPECT_EQ(track.trackNumber(), 5);
    EXPECT_EQ(track.trackTotal(), 7);
    EXPECT_EQ(track.discNumber(), 1);
    EXPECT_EQ(track.discTotal(), 1);
    EXPECT_EQ(track.genre(), QStringLiteral("Testing"));
    EXPECT_EQ(track.performer(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.composer(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.comment(), QStringLiteral("A fooyin test"));
    EXPECT_GT(track.duration(), 0);

    const auto testTag = track.extraTag(QStringLiteral("TEST"));
    ASSERT_TRUE(!testTag.isEmpty());
    EXPECT_EQ(testTag.front(), QStringLiteral("A custom tag"));
}

TEST_F(TagReaderTest, OpusRead)
{
    const TempResource file{QStringLiteral(":/audio/audiotest.opus")};
    file.checkValid();

    Track track{file.fileName()};
    ASSERT_TRUE(m_parser.readMetaData(track));

    EXPECT_EQ(track.codec(), QStringLiteral("Opus"));
    EXPECT_EQ(track.title(), QStringLiteral("OPUS Test"));
    EXPECT_EQ(track.album(), QStringLiteral("Fooyin Audio Tests"));
    EXPECT_EQ(track.albumArtist(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.artist(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.date(), QStringLiteral("2023"));
    EXPECT_EQ(track.trackNumber(), 6);
    EXPECT_EQ(track.trackTotal(), 7);
    EXPECT_EQ(track.discNumber(), 1);
    EXPECT_EQ(track.discTotal(), 1);
    EXPECT_EQ(track.genre(), QStringLiteral("Testing"));
    EXPECT_EQ(track.performer(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.composer(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.comment(), QStringLiteral("A fooyin test"));
    EXPECT_GT(track.duration(), 0);

    const auto testTag = track.extraTag(QStringLiteral("TEST"));
    ASSERT_TRUE(!testTag.isEmpty());
    EXPECT_EQ(testTag.front(), QStringLiteral("A custom tag"));
}

TEST_F(TagReaderTest, WavRead)
{
    const TempResource file{QStringLiteral(":/audio/audiotest.wav")};
    file.checkValid();

    Track track{file.fileName()};
    ASSERT_TRUE(m_parser.readMetaData(track));

    EXPECT_EQ(track.codec(), QStringLiteral("WAV"));
    EXPECT_EQ(track.title(), QStringLiteral("WAV Test"));
    EXPECT_EQ(track.album(), QStringLiteral("Fooyin Audio Tests"));
    EXPECT_EQ(track.albumArtist(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.artist(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.date(), QStringLiteral("2023"));
    EXPECT_EQ(track.trackNumber(), 7);
    EXPECT_EQ(track.trackTotal(), 7);
    EXPECT_EQ(track.discNumber(), 1);
    EXPECT_EQ(track.discTotal(), 1);
    EXPECT_EQ(track.genre(), QStringLiteral("Testing"));
    EXPECT_EQ(track.performer(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.composer(), QStringLiteral("Fooyin"));
    EXPECT_EQ(track.comment(), QStringLiteral("A fooyin test"));
    EXPECT_GT(track.duration(), 0);

    const auto testTag = track.extraTag(QStringLiteral("TEST"));
    ASSERT_TRUE(!testTag.isEmpty());
    EXPECT_EQ(testTag.front(), QStringLiteral("A custom tag"));
}
} // namespace Fooyin::Testing
