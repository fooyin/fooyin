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

#include "core/tagging/tagreader.h"
#include "core/tagging/tagwriter.h"

#include <core/track.h>

#include <gtest/gtest.h>

// clazy:excludeall=returning-void-expression

namespace {
void fileValid(const Fooyin::Testing::TempResource& resource, const QString& file)
{
    QByteArray origFileData;
    QByteArray tmpFileData;
    {
        QFile origFile(file);
        origFile.open(QIODevice::ReadOnly);

        EXPECT_TRUE(origFile.isOpen());

        origFileData = origFile.readAll();
        origFile.close();
    }

    {
        QFile tmpFile(resource.fileName());
        tmpFile.open(QIODevice::ReadOnly);

        EXPECT_TRUE(tmpFile.isOpen());

        tmpFileData = tmpFile.readAll();
        tmpFile.close();
    }

    EXPECT_TRUE(!origFileData.isEmpty());
    EXPECT_TRUE(!tmpFileData.isEmpty());
    EXPECT_EQ(origFileData, tmpFileData);
}
} // namespace

namespace Fooyin::Testing {
class TagWriterTest : public ::testing::Test
{
protected:
};

TEST_F(TagWriterTest, AiffWrite)
{
    const TempResource file{QStringLiteral(":/audio/audiotest.aiff")};
    fileValid(file, QStringLiteral(":/audio/audiotest.aiff"));

    {
        Track track{file.fileName()};
        Tagging::readMetaData(track);

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

        Tagging::writeMetaData(track);
    }

    {
        Track track{file.fileName()};
        Tagging::readMetaData(track);

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
    fileValid(file, QStringLiteral(":/audio/audiotest.flac"));

    {
        Track track{file.fileName()};
        Tagging::readMetaData(track);

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

        Tagging::writeMetaData(track);
    }

    {
        Track track{file.fileName()};
        Tagging::readMetaData(track);

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
    fileValid(file, QStringLiteral(":/audio/audiotest.m4a"));

    {
        Track track{file.fileName()};
        Tagging::readMetaData(track);

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

        Tagging::writeMetaData(track);
    }

    {
        Track track{file.fileName()};
        Tagging::readMetaData(track);

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
    fileValid(file, QStringLiteral(":/audio/audiotest.mp3"));

    {
        Track track{file.fileName()};
        Tagging::readMetaData(track);

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

        Tagging::writeMetaData(track);
    }

    {
        Track track{file.fileName()};
        Tagging::readMetaData(track);

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
    fileValid(file, QStringLiteral(":/audio/audiotest.ogg"));

    {
        Track track{file.fileName()};
        Tagging::readMetaData(track);

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

        Tagging::writeMetaData(track);
    }

    {
        Track track{file.fileName()};
        Tagging::readMetaData(track);

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
    fileValid(file, QStringLiteral(":/audio/audiotest.opus"));

    {
        Track track{file.fileName()};
        Tagging::readMetaData(track);

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

        Tagging::writeMetaData(track);
    }

    {
        Track track{file.fileName()};
        Tagging::readMetaData(track);

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
    fileValid(file, QStringLiteral(":/audio/audiotest.wav"));

    {
        Track track{file.fileName()};
        Tagging::readMetaData(track);

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

        Tagging::writeMetaData(track);
    }

    {
        Track track{file.fileName()};
        Tagging::readMetaData(track);

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
