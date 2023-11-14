/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/tagging/tagreader.h>
#include <core/tagging/tagwriter.h>
#include <core/track.h>

#include <gtest/gtest.h>

// clazy:excludeall=returning-void-expression

using namespace Qt::Literals::StringLiterals;

namespace {
void fileValid(const Fy::Testing::TempResource& resource, const QString& file)
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

namespace Fy::Testing {
class TagWriterTest : public ::testing::Test
{
protected:
    Core::Tagging::TagReader m_tagReader;
    Core::Tagging::TagWriter m_tagWriter;
};

TEST_F(TagWriterTest, AiffWrite)
{
    const TempResource file{u":/audio/audiotest.aiff"_s};
    fileValid(file, u":/audio/audiotest.aiff"_s);

    {
        Core::Track track{file.fileName()};
        m_tagReader.readMetaData(track);

        track.setId(0);
        track.setType(Core::Track::Type::FLAC);
        track.setTitle(u"TestTitle"_s);
        track.setAlbum({u"TestAlbum"_s});
        track.setAlbumArtist(u"TestAArtist"_s);
        track.setArtists({u"TestArtist"_s});
        track.setDate(u"2023-12-12"_s);
        track.setTrackNumber(9);
        track.setTrackTotal(99);
        track.setDiscNumber(4);
        track.setDiscTotal(44);
        track.setGenres({u"TestGenre"_s});
        track.setPerformer(u"TestPerformer"_s);
        track.setComposer(u"testComposer"_s);
        track.setComment(u"TestComment"_s);
        track.addExtraTag(u"WRITETEST"_s, u"Success"_s);
        track.removeExtraTag(u"TEST"_s);

        m_tagWriter.writeMetaData(track);
    }

    {
        Core::Track track{file.fileName()};
        m_tagReader.readMetaData(track);

        EXPECT_EQ(track.title(), "TestTitle");
        EXPECT_EQ(track.album(), "TestAlbum");
        EXPECT_EQ(track.albumArtist(), "TestAArtist");
        EXPECT_EQ(track.artist(), "TestArtist");
        EXPECT_EQ(track.date(), "2023-12-12");
        EXPECT_EQ(track.trackNumber(), 9);
        EXPECT_EQ(track.trackTotal(), 99);
        EXPECT_EQ(track.discNumber(), 4);
        EXPECT_EQ(track.discTotal(), 44);
        EXPECT_EQ(track.genre(), "TestGenre");
        EXPECT_EQ(track.performer(), "TestPerformer");
        EXPECT_EQ(track.composer(), "testComposer");
        EXPECT_EQ(track.comment(), "TestComment");

        const auto testTag = track.extraTag(u"TEST"_s);
        EXPECT_TRUE(testTag.empty());

        const auto writeTag = track.extraTag(u"WRITETEST"_s);
        ASSERT_TRUE(!writeTag.isEmpty());
        EXPECT_EQ(writeTag.front(), "Success");
    }
}

TEST_F(TagWriterTest, FlacWrite)
{
    const TempResource file{u":/audio/audiotest.flac"_s};
    fileValid(file, u":/audio/audiotest.flac"_s);

    {
        Core::Track track{file.fileName()};
        m_tagReader.readMetaData(track);

        track.setId(0);
        track.setType(Core::Track::Type::FLAC);
        track.setTitle(u"TestTitle"_s);
        track.setAlbum({u"TestAlbum"_s});
        track.setAlbumArtist(u"TestAArtist"_s);
        track.setArtists({u"TestArtist"_s});
        track.setDate(u"2023-12-12"_s);
        track.setTrackNumber(9);
        track.setTrackTotal(99);
        track.setDiscNumber(4);
        track.setDiscTotal(44);
        track.setGenres({u"TestGenre"_s});
        track.setPerformer(u"TestPerformer"_s);
        track.setComposer(u"testComposer"_s);
        track.setComment(u"TestComment"_s);
        track.addExtraTag(u"WRITETEST"_s, u"Success"_s);
        track.removeExtraTag(u"TEST"_s);

        m_tagWriter.writeMetaData(track);
    }

    {
        Core::Track track{file.fileName()};
        m_tagReader.readMetaData(track);

        EXPECT_EQ(track.title(), "TestTitle");
        EXPECT_EQ(track.album(), "TestAlbum");
        EXPECT_EQ(track.albumArtist(), "TestAArtist");
        EXPECT_EQ(track.artist(), "TestArtist");
        EXPECT_EQ(track.date(), "2023-12-12");
        EXPECT_EQ(track.trackNumber(), 9);
        EXPECT_EQ(track.trackTotal(), 99);
        EXPECT_EQ(track.discNumber(), 4);
        EXPECT_EQ(track.discTotal(), 44);
        EXPECT_EQ(track.genre(), "TestGenre");
        EXPECT_EQ(track.performer(), "TestPerformer");
        EXPECT_EQ(track.composer(), "testComposer");
        EXPECT_EQ(track.comment(), "TestComment");

        const auto testTag = track.extraTag(u"TEST"_s);
        EXPECT_TRUE(testTag.empty());

        const auto writeTag = track.extraTag(u"WRITETEST"_s);
        ASSERT_TRUE(!writeTag.isEmpty());
        EXPECT_EQ(writeTag.front(), "Success");
    }
}

TEST_F(TagWriterTest, M4aWrite)
{
    const TempResource file{u":/audio/audiotest.m4a"_s};
    fileValid(file, u":/audio/audiotest.m4a"_s);

    {
        Core::Track track{file.fileName()};
        m_tagReader.readMetaData(track);

        track.setId(0);
        track.setType(Core::Track::Type::FLAC);
        track.setTitle(u"TestTitle"_s);
        track.setAlbum({u"TestAlbum"_s});
        track.setAlbumArtist(u"TestAArtist"_s);
        track.setArtists({u"TestArtist"_s});
        track.setDate(u"2023-12-12"_s);
        track.setTrackNumber(9);
        track.setTrackTotal(99);
        track.setDiscNumber(4);
        track.setDiscTotal(44);
        track.setGenres({u"TestGenre"_s});
        track.setPerformer(u"TestPerformer"_s);
        track.setComposer(u"testComposer"_s);
        track.setComment(u"TestComment"_s);
        track.addExtraTag(u"WRITETEST"_s, u"Success"_s);
        track.removeExtraTag(u"TEST"_s);

        m_tagWriter.writeMetaData(track);
    }

    {
        Core::Track track{file.fileName()};
        m_tagReader.readMetaData(track);

        EXPECT_EQ(track.title(), "TestTitle");
        EXPECT_EQ(track.album(), "TestAlbum");
        EXPECT_EQ(track.albumArtist(), "TestAArtist");
        EXPECT_EQ(track.artist(), "TestArtist");
        EXPECT_EQ(track.date(), "2023-12-12");
        EXPECT_EQ(track.trackNumber(), 9);
        EXPECT_EQ(track.trackTotal(), 99);
        EXPECT_EQ(track.discNumber(), 4);
        EXPECT_EQ(track.discTotal(), 44);
        EXPECT_EQ(track.genre(), "TestGenre");
        EXPECT_EQ(track.performer(), "TestPerformer");
        EXPECT_EQ(track.composer(), "testComposer");
        EXPECT_EQ(track.comment(), "TestComment");

        const auto testTag = track.extraTag(u"TEST"_s);
        EXPECT_TRUE(testTag.empty());

        const auto writeTag = track.extraTag(u"WRITETEST"_s);
        ASSERT_TRUE(!writeTag.isEmpty());
        EXPECT_EQ(writeTag.front(), "Success");
    }
}

TEST_F(TagWriterTest, Mp3Write)
{
    const TempResource file{u":/audio/audiotest.mp3"_s};
    fileValid(file, u":/audio/audiotest.mp3"_s);

    {
        Core::Track track{file.fileName()};
        m_tagReader.readMetaData(track);

        track.setId(0);
        track.setType(Core::Track::Type::FLAC);
        track.setTitle(u"TestTitle"_s);
        track.setAlbum({u"TestAlbum"_s});
        track.setAlbumArtist(u"TestAArtist"_s);
        track.setArtists({u"TestArtist"_s});
        track.setDate(u"2023-12-12"_s);
        track.setTrackNumber(9);
        track.setTrackTotal(99);
        track.setDiscNumber(4);
        track.setDiscTotal(44);
        track.setGenres({u"TestGenre"_s});
        track.setPerformer(u"TestPerformer"_s);
        track.setComposer(u"testComposer"_s);
        track.setComment(u"TestComment"_s);
        track.addExtraTag(u"WRITETEST"_s, u"Success"_s);
        track.removeExtraTag(u"TEST"_s);

        m_tagWriter.writeMetaData(track);
    }

    {
        Core::Track track{file.fileName()};
        m_tagReader.readMetaData(track);

        EXPECT_EQ(track.title(), "TestTitle");
        EXPECT_EQ(track.album(), "TestAlbum");
        EXPECT_EQ(track.albumArtist(), "TestAArtist");
        EXPECT_EQ(track.artist(), "TestArtist");
        EXPECT_EQ(track.date(), "2023-12-12");
        EXPECT_EQ(track.trackNumber(), 9);
        EXPECT_EQ(track.trackTotal(), 99);
        EXPECT_EQ(track.discNumber(), 4);
        EXPECT_EQ(track.discTotal(), 44);
        EXPECT_EQ(track.genre(), "TestGenre");
        EXPECT_EQ(track.performer(), "TestPerformer");
        EXPECT_EQ(track.composer(), "testComposer");
        EXPECT_EQ(track.comment(), "TestComment");

        const auto testTag = track.extraTag(u"TEST"_s);
        EXPECT_TRUE(testTag.empty());

        const auto writeTag = track.extraTag(u"WRITETEST"_s);
        ASSERT_TRUE(!writeTag.isEmpty());
        EXPECT_EQ(writeTag.front(), "Success");
    }
}

TEST_F(TagWriterTest, OggWrite)
{
    const TempResource file{u":/audio/audiotest.ogg"_s};
    fileValid(file, u":/audio/audiotest.ogg"_s);

    {
        Core::Track track{file.fileName()};
        m_tagReader.readMetaData(track);

        track.setId(0);
        track.setType(Core::Track::Type::FLAC);
        track.setTitle(u"TestTitle"_s);
        track.setAlbum({u"TestAlbum"_s});
        track.setAlbumArtist(u"TestAArtist"_s);
        track.setArtists({u"TestArtist"_s});
        track.setDate(u"2023-12-12"_s);
        track.setTrackNumber(9);
        track.setTrackTotal(99);
        track.setDiscNumber(4);
        track.setDiscTotal(44);
        track.setGenres({u"TestGenre"_s});
        track.setPerformer(u"TestPerformer"_s);
        track.setComposer(u"testComposer"_s);
        track.setComment(u"TestComment"_s);
        track.addExtraTag(u"WRITETEST"_s, u"Success"_s);
        track.removeExtraTag(u"TEST"_s);

        m_tagWriter.writeMetaData(track);
    }

    {
        Core::Track track{file.fileName()};
        m_tagReader.readMetaData(track);

        EXPECT_EQ(track.title(), "TestTitle");
        EXPECT_EQ(track.album(), "TestAlbum");
        EXPECT_EQ(track.albumArtist(), "TestAArtist");
        EXPECT_EQ(track.artist(), "TestArtist");
        EXPECT_EQ(track.date(), "2023-12-12");
        EXPECT_EQ(track.trackNumber(), 9);
        EXPECT_EQ(track.trackTotal(), 99);
        EXPECT_EQ(track.discNumber(), 4);
        EXPECT_EQ(track.discTotal(), 44);
        EXPECT_EQ(track.genre(), "TestGenre");
        EXPECT_EQ(track.performer(), "TestPerformer");
        EXPECT_EQ(track.composer(), "testComposer");
        EXPECT_EQ(track.comment(), "TestComment");

        const auto testTag = track.extraTag(u"TEST"_s);
        EXPECT_TRUE(testTag.empty());

        const auto writeTag = track.extraTag(u"WRITETEST"_s);
        ASSERT_TRUE(!writeTag.isEmpty());
        EXPECT_EQ(writeTag.front(), "Success");
    }
}

TEST_F(TagWriterTest, OpusWrite)
{
    const TempResource file{u":/audio/audiotest.opus"_s};
    fileValid(file, u":/audio/audiotest.opus"_s);

    {
        Core::Track track{file.fileName()};
        m_tagReader.readMetaData(track);

        track.setId(0);
        track.setType(Core::Track::Type::FLAC);
        track.setTitle(u"TestTitle"_s);
        track.setAlbum({u"TestAlbum"_s});
        track.setAlbumArtist(u"TestAArtist"_s);
        track.setArtists({u"TestArtist"_s});
        track.setDate(u"2023-12-12"_s);
        track.setTrackNumber(9);
        track.setTrackTotal(99);
        track.setDiscNumber(4);
        track.setDiscTotal(44);
        track.setGenres({u"TestGenre"_s});
        track.setPerformer(u"TestPerformer"_s);
        track.setComposer(u"testComposer"_s);
        track.setComment(u"TestComment"_s);
        track.addExtraTag(u"WRITETEST"_s, u"Success"_s);
        track.removeExtraTag(u"TEST"_s);

        m_tagWriter.writeMetaData(track);
    }

    {
        Core::Track track{file.fileName()};
        m_tagReader.readMetaData(track);

        EXPECT_EQ(track.title(), "TestTitle");
        EXPECT_EQ(track.album(), "TestAlbum");
        EXPECT_EQ(track.albumArtist(), "TestAArtist");
        EXPECT_EQ(track.artist(), "TestArtist");
        EXPECT_EQ(track.date(), "2023-12-12");
        EXPECT_EQ(track.trackNumber(), 9);
        EXPECT_EQ(track.trackTotal(), 99);
        EXPECT_EQ(track.discNumber(), 4);
        EXPECT_EQ(track.discTotal(), 44);
        EXPECT_EQ(track.genre(), "TestGenre");
        EXPECT_EQ(track.performer(), "TestPerformer");
        EXPECT_EQ(track.composer(), "testComposer");
        EXPECT_EQ(track.comment(), "TestComment");

        const auto testTag = track.extraTag(u"TEST"_s);
        EXPECT_TRUE(testTag.empty());

        const auto writeTag = track.extraTag(u"WRITETEST"_s);
        ASSERT_TRUE(!writeTag.isEmpty());
        EXPECT_EQ(writeTag.front(), "Success");
    }
}

TEST_F(TagWriterTest, WavWrite)
{
    const TempResource file{u":/audio/audiotest.wav"_s};
    fileValid(file, u":/audio/audiotest.wav"_s);

    {
        Core::Track track{file.fileName()};
        m_tagReader.readMetaData(track);

        track.setId(0);
        track.setType(Core::Track::Type::FLAC);
        track.setTitle(u"TestTitle"_s);
        track.setAlbum({u"TestAlbum"_s});
        track.setAlbumArtist(u"TestAArtist"_s);
        track.setArtists({u"TestArtist"_s});
        track.setDate(u"2023-12-12"_s);
        track.setTrackNumber(9);
        track.setTrackTotal(99);
        track.setDiscNumber(4);
        track.setDiscTotal(44);
        track.setGenres({u"TestGenre"_s});
        track.setPerformer(u"TestPerformer"_s);
        track.setComposer(u"testComposer"_s);
        track.setComment(u"TestComment"_s);
        track.addExtraTag(u"WRITETEST"_s, u"Success"_s);
        track.removeExtraTag(u"TEST"_s);

        m_tagWriter.writeMetaData(track);
    }

    {
        Core::Track track{file.fileName()};
        m_tagReader.readMetaData(track);

        EXPECT_EQ(track.title(), "TestTitle");
        EXPECT_EQ(track.album(), "TestAlbum");
        EXPECT_EQ(track.albumArtist(), "TestAArtist");
        EXPECT_EQ(track.artist(), "TestArtist");
        EXPECT_EQ(track.date(), "2023-12-12");
        EXPECT_EQ(track.trackNumber(), 9);
        EXPECT_EQ(track.trackTotal(), 99);
        EXPECT_EQ(track.discNumber(), 4);
        EXPECT_EQ(track.discTotal(), 44);
        EXPECT_EQ(track.genre(), "TestGenre");
        EXPECT_EQ(track.performer(), "TestPerformer");
        EXPECT_EQ(track.composer(), "testComposer");
        EXPECT_EQ(track.comment(), "TestComment");

        const auto testTag = track.extraTag(u"TEST"_s);
        EXPECT_TRUE(testTag.empty());

        const auto writeTag = track.extraTag(u"WRITETEST"_s);
        ASSERT_TRUE(!writeTag.isEmpty());
        EXPECT_EQ(writeTag.front(), "Success");
    }
}
} // namespace Fy::Testing
