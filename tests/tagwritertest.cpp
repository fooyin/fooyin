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
class TagWriterTest : public ::testing::Test
{
protected:
    TagLibReader m_parser;
};

TEST_F(TagWriterTest, AiffWrite)
{
    const TempResource file{QStringLiteral(":/audio/audiotest.aiff")};
    file.checkValid();

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(track));

        track.setId(0);
        track.setTitle(QStringLiteral("TestTitle"));
        track.setAlbum({QStringLiteral("TestAlbum")});
        track.setAlbumArtists({QStringLiteral("TestAArtist")});
        track.setArtists({QStringLiteral("TestArtist")});
        track.setDate(QStringLiteral("2023-12-12"));
        track.setTrackNumber(9);
        track.setTrackTotal(99);
        track.setDiscNumber(4);
        track.setDiscTotal(44);
        track.setGenres({QStringLiteral("TestGenre")});
        track.setPerformer(QStringLiteral("TestPerformer"));
        track.setComposer(QStringLiteral("testComposer"));
        track.setComment(QStringLiteral("TestComment"));
        track.addExtraTag(QStringLiteral("WRITETEST"), QStringLiteral("Success"));
        track.removeExtraTag(QStringLiteral("TEST"));

        ASSERT_TRUE(m_parser.writeTrack(track, {}));
    }

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(track));

        EXPECT_EQ(track.title(), QStringLiteral("TestTitle"));
        EXPECT_EQ(track.album(), QStringLiteral("TestAlbum"));
        EXPECT_EQ(track.albumArtist(), QStringLiteral("TestAArtist"));
        EXPECT_EQ(track.artist(), QStringLiteral("TestArtist"));
        EXPECT_EQ(track.date(), QStringLiteral("2023-12-12"));
        EXPECT_EQ(track.trackNumber(), 9);
        EXPECT_EQ(track.trackTotal(), 99);
        EXPECT_EQ(track.discNumber(), 4);
        EXPECT_EQ(track.discTotal(), 44);
        EXPECT_EQ(track.genre(), QStringLiteral("TestGenre"));
        EXPECT_EQ(track.performer(), QStringLiteral("TestPerformer"));
        EXPECT_EQ(track.composer(), QStringLiteral("testComposer"));
        EXPECT_EQ(track.comment(), QStringLiteral("TestComment"));

        const auto testTag = track.extraTag(QStringLiteral("TEST"));
        EXPECT_TRUE(testTag.empty());

        const auto writeTag = track.extraTag(QStringLiteral("WRITETEST"));
        ASSERT_TRUE(!writeTag.isEmpty());
        EXPECT_EQ(writeTag.front(), QStringLiteral("Success"));
    }
}

TEST_F(TagWriterTest, FlacWrite)
{
    const TempResource file{QStringLiteral(":/audio/audiotest.flac")};
    file.checkValid();

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(track));

        track.setId(0);
        track.setTitle(QStringLiteral("TestTitle"));
        track.setAlbum({QStringLiteral("TestAlbum")});
        track.setAlbumArtists({QStringLiteral("TestAArtist")});
        track.setArtists({QStringLiteral("TestArtist")});
        track.setDate(QStringLiteral("2023-12-12"));
        track.setTrackNumber(9);
        track.setTrackTotal(99);
        track.setDiscNumber(4);
        track.setDiscTotal(44);
        track.setGenres({QStringLiteral("TestGenre")});
        track.setPerformer(QStringLiteral("TestPerformer"));
        track.setComposer(QStringLiteral("testComposer"));
        track.setComment(QStringLiteral("TestComment"));
        track.addExtraTag(QStringLiteral("WRITETEST"), QStringLiteral("Success"));
        track.removeExtraTag(QStringLiteral("TEST"));

        ASSERT_TRUE(m_parser.writeTrack(track, {}));
    }

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(track));

        EXPECT_EQ(track.title(), QStringLiteral("TestTitle"));
        EXPECT_EQ(track.album(), QStringLiteral("TestAlbum"));
        EXPECT_EQ(track.albumArtist(), QStringLiteral("TestAArtist"));
        EXPECT_EQ(track.artist(), QStringLiteral("TestArtist"));
        EXPECT_EQ(track.date(), QStringLiteral("2023-12-12"));
        EXPECT_EQ(track.trackNumber(), 9);
        EXPECT_EQ(track.trackTotal(), 99);
        EXPECT_EQ(track.discNumber(), 4);
        EXPECT_EQ(track.discTotal(), 44);
        EXPECT_EQ(track.genre(), QStringLiteral("TestGenre"));
        EXPECT_EQ(track.performer(), QStringLiteral("TestPerformer"));
        EXPECT_EQ(track.composer(), QStringLiteral("testComposer"));
        EXPECT_EQ(track.comment(), QStringLiteral("TestComment"));

        const auto testTag = track.extraTag(QStringLiteral("TEST"));
        EXPECT_TRUE(testTag.empty());

        const auto writeTag = track.extraTag(QStringLiteral("WRITETEST"));
        ASSERT_TRUE(!writeTag.isEmpty());
        EXPECT_EQ(writeTag.front(), QStringLiteral("Success"));
    }
}

TEST_F(TagWriterTest, M4aWrite)
{
    const TempResource file{QStringLiteral(":/audio/audiotest.m4a")};
    file.checkValid();

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(track));

        track.setId(0);
        track.setTitle(QStringLiteral("TestTitle"));
        track.setAlbum({QStringLiteral("TestAlbum")});
        track.setAlbumArtists({QStringLiteral("TestAArtist")});
        track.setArtists({QStringLiteral("TestArtist")});
        track.setDate(QStringLiteral("2023-12-12"));
        track.setTrackNumber(9);
        track.setTrackTotal(99);
        track.setDiscNumber(4);
        track.setDiscTotal(44);
        track.setGenres({QStringLiteral("TestGenre")});
        track.setPerformer(QStringLiteral("TestPerformer"));
        track.setComposer(QStringLiteral("testComposer"));
        track.setComment(QStringLiteral("TestComment"));
        track.addExtraTag(QStringLiteral("WRITETEST"), QStringLiteral("Success"));
        track.removeExtraTag(QStringLiteral("TEST"));

        ASSERT_TRUE(m_parser.writeTrack(track, {}));
    }

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(track));

        EXPECT_EQ(track.title(), QStringLiteral("TestTitle"));
        EXPECT_EQ(track.album(), QStringLiteral("TestAlbum"));
        EXPECT_EQ(track.albumArtist(), QStringLiteral("TestAArtist"));
        EXPECT_EQ(track.artist(), QStringLiteral("TestArtist"));
        EXPECT_EQ(track.date(), QStringLiteral("2023-12-12"));
        EXPECT_EQ(track.trackNumber(), 9);
        EXPECT_EQ(track.trackTotal(), 99);
        EXPECT_EQ(track.discNumber(), 4);
        EXPECT_EQ(track.discTotal(), 44);
        EXPECT_EQ(track.genre(), QStringLiteral("TestGenre"));
        EXPECT_EQ(track.performer(), QStringLiteral("TestPerformer"));
        EXPECT_EQ(track.composer(), QStringLiteral("testComposer"));
        EXPECT_EQ(track.comment(), QStringLiteral("TestComment"));

        const auto testTag = track.extraTag(QStringLiteral("TEST"));
        EXPECT_TRUE(testTag.empty());

        const auto writeTag = track.extraTag(QStringLiteral("WRITETEST"));
        ASSERT_TRUE(!writeTag.isEmpty());
        EXPECT_EQ(writeTag.front(), QStringLiteral("Success"));
    }
}

TEST_F(TagWriterTest, Mp3Write)
{
    const TempResource file{QStringLiteral(":/audio/audiotest.mp3")};
    file.checkValid();

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(track));

        track.setId(0);
        track.setTitle(QStringLiteral("TestTitle"));
        track.setAlbum({QStringLiteral("TestAlbum")});
        track.setAlbumArtists({QStringLiteral("TestAArtist")});
        track.setArtists({QStringLiteral("TestArtist")});
        track.setDate(QStringLiteral("2023-12-12"));
        track.setTrackNumber(9);
        track.setTrackTotal(99);
        track.setDiscNumber(4);
        track.setDiscTotal(44);
        track.setGenres({QStringLiteral("TestGenre")});
        track.setPerformer(QStringLiteral("TestPerformer"));
        track.setComposer(QStringLiteral("testComposer"));
        track.setComment(QStringLiteral("TestComment"));
        track.addExtraTag(QStringLiteral("WRITETEST"), QStringLiteral("Success"));
        track.removeExtraTag(QStringLiteral("TEST"));

        ASSERT_TRUE(m_parser.writeTrack(track, {}));
    }

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(track));

        EXPECT_EQ(track.title(), QStringLiteral("TestTitle"));
        EXPECT_EQ(track.album(), QStringLiteral("TestAlbum"));
        EXPECT_EQ(track.albumArtist(), QStringLiteral("TestAArtist"));
        EXPECT_EQ(track.artist(), QStringLiteral("TestArtist"));
        EXPECT_EQ(track.date(), QStringLiteral("2023-12-12"));
        EXPECT_EQ(track.trackNumber(), 9);
        EXPECT_EQ(track.trackTotal(), 99);
        EXPECT_EQ(track.discNumber(), 4);
        EXPECT_EQ(track.discTotal(), 44);
        EXPECT_EQ(track.genre(), QStringLiteral("TestGenre"));
        EXPECT_EQ(track.performer(), QStringLiteral("TestPerformer"));
        EXPECT_EQ(track.composer(), QStringLiteral("testComposer"));
        EXPECT_EQ(track.comment(), QStringLiteral("TestComment"));

        const auto testTag = track.extraTag(QStringLiteral("TEST"));
        EXPECT_TRUE(testTag.empty());

        const auto writeTag = track.extraTag(QStringLiteral("WRITETEST"));
        ASSERT_TRUE(!writeTag.isEmpty());
        EXPECT_EQ(writeTag.front(), QStringLiteral("Success"));
    }
}

TEST_F(TagWriterTest, OggWrite)
{
    const TempResource file{QStringLiteral(":/audio/audiotest.ogg")};
    file.checkValid();

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(track));

        track.setId(0);
        track.setTitle(QStringLiteral("TestTitle"));
        track.setAlbum({QStringLiteral("TestAlbum")});
        track.setAlbumArtists({QStringLiteral("TestAArtist")});
        track.setArtists({QStringLiteral("TestArtist")});
        track.setDate(QStringLiteral("2023-12-12"));
        track.setTrackNumber(9);
        track.setTrackTotal(99);
        track.setDiscNumber(4);
        track.setDiscTotal(44);
        track.setGenres({QStringLiteral("TestGenre")});
        track.setPerformer(QStringLiteral("TestPerformer"));
        track.setComposer(QStringLiteral("testComposer"));
        track.setComment(QStringLiteral("TestComment"));
        track.addExtraTag(QStringLiteral("WRITETEST"), QStringLiteral("Success"));
        track.removeExtraTag(QStringLiteral("TEST"));

        ASSERT_TRUE(m_parser.writeTrack(track, {}));
    }

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(track));

        EXPECT_EQ(track.title(), QStringLiteral("TestTitle"));
        EXPECT_EQ(track.album(), QStringLiteral("TestAlbum"));
        EXPECT_EQ(track.albumArtist(), QStringLiteral("TestAArtist"));
        EXPECT_EQ(track.artist(), QStringLiteral("TestArtist"));
        EXPECT_EQ(track.date(), QStringLiteral("2023-12-12"));
        EXPECT_EQ(track.trackNumber(), 9);
        EXPECT_EQ(track.trackTotal(), 99);
        EXPECT_EQ(track.discNumber(), 4);
        EXPECT_EQ(track.discTotal(), 44);
        EXPECT_EQ(track.genre(), QStringLiteral("TestGenre"));
        EXPECT_EQ(track.performer(), QStringLiteral("TestPerformer"));
        EXPECT_EQ(track.composer(), QStringLiteral("testComposer"));
        EXPECT_EQ(track.comment(), QStringLiteral("TestComment"));

        const auto testTag = track.extraTag(QStringLiteral("TEST"));
        EXPECT_TRUE(testTag.empty());

        const auto writeTag = track.extraTag(QStringLiteral("WRITETEST"));
        ASSERT_TRUE(!writeTag.isEmpty());
        EXPECT_EQ(writeTag.front(), QStringLiteral("Success"));
    }
}

TEST_F(TagWriterTest, OpusWrite)
{
    const TempResource file{QStringLiteral(":/audio/audiotest.opus")};
    file.checkValid();

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(track));

        track.setId(0);
        track.setTitle(QStringLiteral("TestTitle"));
        track.setAlbum({QStringLiteral("TestAlbum")});
        track.setAlbumArtists({QStringLiteral("TestAArtist")});
        track.setArtists({QStringLiteral("TestArtist")});
        track.setDate(QStringLiteral("2023-12-12"));
        track.setTrackNumber(9);
        track.setTrackTotal(99);
        track.setDiscNumber(4);
        track.setDiscTotal(44);
        track.setGenres({QStringLiteral("TestGenre")});
        track.setPerformer(QStringLiteral("TestPerformer"));
        track.setComposer(QStringLiteral("testComposer"));
        track.setComment(QStringLiteral("TestComment"));
        track.addExtraTag(QStringLiteral("WRITETEST"), QStringLiteral("Success"));
        track.removeExtraTag(QStringLiteral("TEST"));

        ASSERT_TRUE(m_parser.writeTrack(track, {}));
    }

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(track));

        EXPECT_EQ(track.title(), QStringLiteral("TestTitle"));
        EXPECT_EQ(track.album(), QStringLiteral("TestAlbum"));
        EXPECT_EQ(track.albumArtist(), QStringLiteral("TestAArtist"));
        EXPECT_EQ(track.artist(), QStringLiteral("TestArtist"));
        EXPECT_EQ(track.date(), QStringLiteral("2023-12-12"));
        EXPECT_EQ(track.trackNumber(), 9);
        EXPECT_EQ(track.trackTotal(), 99);
        EXPECT_EQ(track.discNumber(), 4);
        EXPECT_EQ(track.discTotal(), 44);
        EXPECT_EQ(track.genre(), QStringLiteral("TestGenre"));
        EXPECT_EQ(track.performer(), QStringLiteral("TestPerformer"));
        EXPECT_EQ(track.composer(), QStringLiteral("testComposer"));
        EXPECT_EQ(track.comment(), QStringLiteral("TestComment"));

        const auto testTag = track.extraTag(QStringLiteral("TEST"));
        EXPECT_TRUE(testTag.empty());

        const auto writeTag = track.extraTag(QStringLiteral("WRITETEST"));
        ASSERT_TRUE(!writeTag.isEmpty());
        EXPECT_EQ(writeTag.front(), QStringLiteral("Success"));
    }
}

TEST_F(TagWriterTest, WavWrite)
{
    const TempResource file{QStringLiteral(":/audio/audiotest.wav")};
    file.checkValid();

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(track));

        track.setId(0);
        track.setTitle(QStringLiteral("TestTitle"));
        track.setAlbum({QStringLiteral("TestAlbum")});
        track.setAlbumArtists({QStringLiteral("TestAArtist")});
        track.setArtists({QStringLiteral("TestArtist")});
        track.setDate(QStringLiteral("2023-12-12"));
        track.setTrackNumber(9);
        track.setTrackTotal(99);
        track.setDiscNumber(4);
        track.setDiscTotal(44);
        track.setGenres({QStringLiteral("TestGenre")});
        track.setPerformer(QStringLiteral("TestPerformer"));
        track.setComposer(QStringLiteral("testComposer"));
        track.setComment(QStringLiteral("TestComment"));
        track.addExtraTag(QStringLiteral("WRITETEST"), QStringLiteral("Success"));
        track.removeExtraTag(QStringLiteral("TEST"));

        ASSERT_TRUE(m_parser.writeTrack(track, {}));
    }

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(track));

        EXPECT_EQ(track.title(), QStringLiteral("TestTitle"));
        EXPECT_EQ(track.album(), QStringLiteral("TestAlbum"));
        EXPECT_EQ(track.albumArtist(), QStringLiteral("TestAArtist"));
        EXPECT_EQ(track.artist(), QStringLiteral("TestArtist"));
        EXPECT_EQ(track.date(), QStringLiteral("2023-12-12"));
        EXPECT_EQ(track.trackNumber(), 9);
        EXPECT_EQ(track.trackTotal(), 99);
        EXPECT_EQ(track.discNumber(), 4);
        EXPECT_EQ(track.discTotal(), 44);
        EXPECT_EQ(track.genre(), QStringLiteral("TestGenre"));
        EXPECT_EQ(track.performer(), QStringLiteral("TestPerformer"));
        EXPECT_EQ(track.composer(), QStringLiteral("testComposer"));
        EXPECT_EQ(track.comment(), QStringLiteral("TestComment"));

        const auto testTag = track.extraTag(QStringLiteral("TEST"));
        EXPECT_TRUE(testTag.empty());

        const auto writeTag = track.extraTag(QStringLiteral("WRITETEST"));
        ASSERT_TRUE(!writeTag.isEmpty());
        EXPECT_EQ(writeTag.front(), QStringLiteral("Success"));
    }
}
} // namespace Fooyin::Testing
