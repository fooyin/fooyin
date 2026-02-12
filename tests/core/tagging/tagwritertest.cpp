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

#include "../../testutils.h"

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
    const QString filepath = QStringLiteral(":/audio/audiotest.aiff");
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(source, track));

        track.setId(0);
        track.setTitle(QStringLiteral("TestTitle"));
        track.setAlbum({QStringLiteral("TestAlbum")});
        track.setAlbumArtists({QStringLiteral("TestAArtist")});
        track.setArtists({QStringLiteral("TestArtist")});
        track.setDate(QStringLiteral("2023-12-12"));
        track.setTrackNumber(QStringLiteral("9"));
        track.setTrackTotal(QStringLiteral("99"));
        track.setDiscNumber(QStringLiteral("4"));
        track.setDiscTotal(QStringLiteral("44"));
        track.setGenres({QStringLiteral("TestGenre")});
        track.setPerformers({QStringLiteral("TestPerformer")});
        track.setComposers({QStringLiteral("testComposer")});
        track.setComment(QStringLiteral("TestComment"));
        track.addExtraTag(QStringLiteral("WRITETEST"), QStringLiteral("Success"));
        track.removeExtraTag(QStringLiteral("TEST"));

        ASSERT_TRUE(m_parser.writeTrack(source, track, {}));
    }

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(source, track));

        EXPECT_EQ(track.title(), QStringLiteral("TestTitle"));
        EXPECT_EQ(track.album(), QStringLiteral("TestAlbum"));
        EXPECT_EQ(track.albumArtist(), QStringLiteral("TestAArtist"));
        EXPECT_EQ(track.artist(), QStringLiteral("TestArtist"));
        EXPECT_EQ(track.date(), QStringLiteral("2023-12-12"));
        EXPECT_EQ(track.trackNumber(), QStringLiteral("9"));
        EXPECT_EQ(track.trackTotal(), QStringLiteral("99"));
        EXPECT_EQ(track.discNumber(), QStringLiteral("4"));
        EXPECT_EQ(track.discTotal(), QStringLiteral("44"));
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
    const QString filepath = QStringLiteral(":/audio/audiotest.flac");
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack({filepath, &file, nullptr}, track));

        track.setId(0);
        track.setTitle(QStringLiteral("TestTitle"));
        track.setAlbum({QStringLiteral("TestAlbum")});
        track.setAlbumArtists({QStringLiteral("TestAArtist")});
        track.setArtists({QStringLiteral("TestArtist")});
        track.setDate(QStringLiteral("2023-12-12"));
        track.setTrackNumber(QStringLiteral("9"));
        track.setTrackTotal(QStringLiteral("99"));
        track.setDiscNumber(QStringLiteral("4"));
        track.setDiscTotal(QStringLiteral("44"));
        track.setGenres({QStringLiteral("TestGenre")});
        track.setPerformers({QStringLiteral("TestPerformer")});
        track.setComposers({QStringLiteral("testComposer")});
        track.setComment(QStringLiteral("TestComment"));
        track.addExtraTag(QStringLiteral("WRITETEST"), QStringLiteral("Success"));
        track.removeExtraTag(QStringLiteral("TEST"));

        ASSERT_TRUE(m_parser.writeTrack(source, track, {}));
    }

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack({filepath, &file, nullptr}, track));

        EXPECT_EQ(track.title(), QStringLiteral("TestTitle"));
        EXPECT_EQ(track.album(), QStringLiteral("TestAlbum"));
        EXPECT_EQ(track.albumArtist(), QStringLiteral("TestAArtist"));
        EXPECT_EQ(track.artist(), QStringLiteral("TestArtist"));
        EXPECT_EQ(track.date(), QStringLiteral("2023-12-12"));
        EXPECT_EQ(track.trackNumber(), QStringLiteral("9"));
        EXPECT_EQ(track.trackTotal(), QStringLiteral("99"));
        EXPECT_EQ(track.discNumber(), QStringLiteral("4"));
        EXPECT_EQ(track.discTotal(), QStringLiteral("44"));
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
    const QString filepath = QStringLiteral(":/audio/audiotest.m4a");
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(source, track));

        track.setId(0);
        track.setTitle(QStringLiteral("TestTitle"));
        track.setAlbum({QStringLiteral("TestAlbum")});
        track.setAlbumArtists({QStringLiteral("TestAArtist")});
        track.setArtists({QStringLiteral("TestArtist")});
        track.setDate(QStringLiteral("2023-12-12"));
        track.setTrackNumber(QStringLiteral("9"));
        track.setTrackTotal(QStringLiteral("99"));
        track.setDiscNumber(QStringLiteral("4"));
        track.setDiscTotal(QStringLiteral("44"));
        track.setGenres({QStringLiteral("TestGenre")});
        track.setPerformers({QStringLiteral("TestPerformer")});
        track.setComposers({QStringLiteral("testComposer")});
        track.setComment(QStringLiteral("TestComment"));
        track.addExtraTag(QStringLiteral("WRITETEST"), QStringLiteral("Success"));
        track.removeExtraTag(QStringLiteral("TEST"));

        ASSERT_TRUE(m_parser.writeTrack(source, track, {}));
    }

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack({filepath, &file, nullptr}, track));

        EXPECT_EQ(track.title(), QStringLiteral("TestTitle"));
        EXPECT_EQ(track.album(), QStringLiteral("TestAlbum"));
        EXPECT_EQ(track.albumArtist(), QStringLiteral("TestAArtist"));
        EXPECT_EQ(track.artist(), QStringLiteral("TestArtist"));
        EXPECT_EQ(track.date(), QStringLiteral("2023-12-12"));
        EXPECT_EQ(track.trackNumber(), QStringLiteral("9"));
        EXPECT_EQ(track.trackTotal(), QStringLiteral("99"));
        EXPECT_EQ(track.discNumber(), QStringLiteral("4"));
        EXPECT_EQ(track.discTotal(), QStringLiteral("44"));
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
    const QString filepath = QStringLiteral(":/audio/audiotest.mp3");
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(source, track));

        track.setId(0);
        track.setTitle(QStringLiteral("TestTitle"));
        track.setAlbum({QStringLiteral("TestAlbum")});
        track.setAlbumArtists({QStringLiteral("TestAArtist")});
        track.setArtists({QStringLiteral("TestArtist")});
        track.setDate(QStringLiteral("2023-12-12"));
        track.setTrackNumber(QStringLiteral("9"));
        track.setTrackTotal(QStringLiteral("99"));
        track.setDiscNumber(QStringLiteral("4"));
        track.setDiscTotal(QStringLiteral("44"));
        track.setGenres({QStringLiteral("TestGenre")});
        track.setPerformers({QStringLiteral("TestPerformer")});
        track.setComposers({QStringLiteral("testComposer")});
        track.setComment(QStringLiteral("TestComment"));
        track.addExtraTag(QStringLiteral("WRITETEST"), QStringLiteral("Success"));
        track.removeExtraTag(QStringLiteral("TEST"));

        ASSERT_TRUE(m_parser.writeTrack(source, track, {}));
    }

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack({filepath, &file, nullptr}, track));

        EXPECT_EQ(track.title(), QStringLiteral("TestTitle"));
        EXPECT_EQ(track.album(), QStringLiteral("TestAlbum"));
        EXPECT_EQ(track.albumArtist(), QStringLiteral("TestAArtist"));
        EXPECT_EQ(track.artist(), QStringLiteral("TestArtist"));
        EXPECT_EQ(track.date(), QStringLiteral("2023-12-12"));
        EXPECT_EQ(track.trackNumber(), QStringLiteral("9"));
        EXPECT_EQ(track.trackTotal(), QStringLiteral("99"));
        EXPECT_EQ(track.discNumber(), QStringLiteral("4"));
        EXPECT_EQ(track.discTotal(), QStringLiteral("44"));
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
    const QString filepath = QStringLiteral(":/audio/audiotest.ogg");
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(source, track));

        track.setId(0);
        track.setTitle(QStringLiteral("TestTitle"));
        track.setAlbum({QStringLiteral("TestAlbum")});
        track.setAlbumArtists({QStringLiteral("TestAArtist")});
        track.setArtists({QStringLiteral("TestArtist")});
        track.setDate(QStringLiteral("2023-12-12"));
        track.setTrackNumber(QStringLiteral("9"));
        track.setTrackTotal(QStringLiteral("99"));
        track.setDiscNumber(QStringLiteral("4"));
        track.setDiscTotal(QStringLiteral("44"));
        track.setGenres({QStringLiteral("TestGenre")});
        track.setPerformers({QStringLiteral("TestPerformer")});
        track.setComposers({QStringLiteral("testComposer")});
        track.setComment(QStringLiteral("TestComment"));
        track.addExtraTag(QStringLiteral("WRITETEST"), QStringLiteral("Success"));
        track.removeExtraTag(QStringLiteral("TEST"));

        ASSERT_TRUE(m_parser.writeTrack(source, track, {}));
    }

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack({filepath, &file, nullptr}, track));

        EXPECT_EQ(track.title(), QStringLiteral("TestTitle"));
        EXPECT_EQ(track.album(), QStringLiteral("TestAlbum"));
        EXPECT_EQ(track.albumArtist(), QStringLiteral("TestAArtist"));
        EXPECT_EQ(track.artist(), QStringLiteral("TestArtist"));
        EXPECT_EQ(track.date(), QStringLiteral("2023-12-12"));
        EXPECT_EQ(track.trackNumber(), QStringLiteral("9"));
        EXPECT_EQ(track.trackTotal(), QStringLiteral("99"));
        EXPECT_EQ(track.discNumber(), QStringLiteral("4"));
        EXPECT_EQ(track.discTotal(), QStringLiteral("44"));
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
    const QString filepath = QStringLiteral(":/audio/audiotest.opus");
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(source, track));

        track.setId(0);
        track.setTitle(QStringLiteral("TestTitle"));
        track.setAlbum({QStringLiteral("TestAlbum")});
        track.setAlbumArtists({QStringLiteral("TestAArtist")});
        track.setArtists({QStringLiteral("TestArtist")});
        track.setDate(QStringLiteral("2023-12-12"));
        track.setTrackNumber(QStringLiteral("9"));
        track.setTrackTotal(QStringLiteral("99"));
        track.setDiscNumber(QStringLiteral("4"));
        track.setDiscTotal(QStringLiteral("44"));
        track.setGenres({QStringLiteral("TestGenre")});
        track.setPerformers({QStringLiteral("TestPerformer")});
        track.setComposers({QStringLiteral("testComposer")});
        track.setComment(QStringLiteral("TestComment"));
        track.addExtraTag(QStringLiteral("WRITETEST"), QStringLiteral("Success"));
        track.removeExtraTag(QStringLiteral("TEST"));

        ASSERT_TRUE(m_parser.writeTrack(source, track, {}));
    }

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack({filepath, &file, nullptr}, track));

        EXPECT_EQ(track.title(), QStringLiteral("TestTitle"));
        EXPECT_EQ(track.album(), QStringLiteral("TestAlbum"));
        EXPECT_EQ(track.albumArtist(), QStringLiteral("TestAArtist"));
        EXPECT_EQ(track.artist(), QStringLiteral("TestArtist"));
        EXPECT_EQ(track.date(), QStringLiteral("2023-12-12"));
        EXPECT_EQ(track.trackNumber(), QStringLiteral("9"));
        EXPECT_EQ(track.trackTotal(), QStringLiteral("99"));
        EXPECT_EQ(track.discNumber(), QStringLiteral("4"));
        EXPECT_EQ(track.discTotal(), QStringLiteral("44"));
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
    const QString filepath = QStringLiteral(":/audio/audiotest.wav");
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(source, track));

        track.setId(0);
        track.setTitle(QStringLiteral("TestTitle"));
        track.setAlbum({QStringLiteral("TestAlbum")});
        track.setAlbumArtists({QStringLiteral("TestAArtist")});
        track.setArtists({QStringLiteral("TestArtist")});
        track.setDate(QStringLiteral("2023-12-12"));
        track.setTrackNumber(QStringLiteral("9"));
        track.setTrackTotal(QStringLiteral("99"));
        track.setDiscNumber(QStringLiteral("4"));
        track.setDiscTotal(QStringLiteral("44"));
        track.setGenres({QStringLiteral("TestGenre")});
        track.setPerformers({QStringLiteral("TestPerformer")});
        track.setComposers({QStringLiteral("testComposer")});
        track.setComment(QStringLiteral("TestComment"));
        track.addExtraTag(QStringLiteral("WRITETEST"), QStringLiteral("Success"));
        track.removeExtraTag(QStringLiteral("TEST"));

        ASSERT_TRUE(m_parser.writeTrack(source, track, {}));
    }

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(source, track));

        EXPECT_EQ(track.title(), QStringLiteral("TestTitle"));
        EXPECT_EQ(track.album(), QStringLiteral("TestAlbum"));
        EXPECT_EQ(track.albumArtist(), QStringLiteral("TestAArtist"));
        EXPECT_EQ(track.artist(), QStringLiteral("TestArtist"));
        EXPECT_EQ(track.date(), QStringLiteral("2023-12-12"));
        EXPECT_EQ(track.trackNumber(), QStringLiteral("9"));
        EXPECT_EQ(track.trackTotal(), QStringLiteral("99"));
        EXPECT_EQ(track.discNumber(), QStringLiteral("4"));
        EXPECT_EQ(track.discTotal(), QStringLiteral("44"));
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
