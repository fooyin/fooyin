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
class TagReaderTest : public ::testing::Test
{
protected:
    Core::Tagging::TagReader m_tagReader;
};

TEST_F(TagReaderTest, AiffRead)
{
    const TempResource file{u":/audio/audiotest.aiff"_s};
    fileValid(file, u":/audio/audiotest.aiff"_s);

    Core::Track track{file.fileName()};
    m_tagReader.readMetaData(track);

    EXPECT_EQ(track.type(), Core::Track::Type::AIFF);
    EXPECT_EQ(track.title(), "AIFF Test");
    EXPECT_EQ(track.album(), "Fooyin Audio Tests");
    EXPECT_EQ(track.albumArtist(), "Fooyin");
    EXPECT_EQ(track.artist(), "Fooyin");
    EXPECT_EQ(track.date(), "2023");
    EXPECT_EQ(track.trackNumber(), 1);
    EXPECT_EQ(track.trackTotal(), 7);
    EXPECT_EQ(track.discNumber(), 1);
    EXPECT_EQ(track.discTotal(), 1);
    EXPECT_EQ(track.genre(), "Testing");
    EXPECT_EQ(track.performer(), "Fooyin");
    EXPECT_EQ(track.composer(), "Fooyin");
    EXPECT_EQ(track.comment(), "A fooyin test");

    const auto testTag = track.extraTag(u"TEST"_s);
    ASSERT_TRUE(!testTag.isEmpty());
    EXPECT_EQ(testTag.front(), "A custom tag");
}

TEST_F(TagReaderTest, FlacRead)
{
    const TempResource file{u":/audio/audiotest.flac"_s};
    fileValid(file, u":/audio/audiotest.flac"_s);

    Core::Track track{file.fileName()};
    m_tagReader.readMetaData(track);

    EXPECT_EQ(track.type(), Core::Track::Type::FLAC);
    EXPECT_EQ(track.title(), "FLAC Test");
    EXPECT_EQ(track.album(), "Fooyin Audio Tests");
    EXPECT_EQ(track.albumArtist(), "Fooyin");
    EXPECT_EQ(track.artist(), "Fooyin");
    EXPECT_EQ(track.date(), "2023");
    EXPECT_EQ(track.trackNumber(), 2);
    EXPECT_EQ(track.trackTotal(), 7);
    EXPECT_EQ(track.discNumber(), 1);
    EXPECT_EQ(track.discTotal(), 1);
    EXPECT_EQ(track.genre(), "Testing");
    EXPECT_EQ(track.performer(), "Fooyin");
    EXPECT_EQ(track.composer(), "Fooyin");
    EXPECT_EQ(track.comment(), "A fooyin test");

    const auto testTag = track.extraTag(u"TEST"_s);
    ASSERT_TRUE(!testTag.isEmpty());
    EXPECT_EQ(testTag.front(), "A custom tag");
}

TEST_F(TagReaderTest, M4aRead)
{
    const TempResource file{u":/audio/audiotest.m4a"_s};
    fileValid(file, u":/audio/audiotest.m4a"_s);

    Core::Track track{file.fileName()};
    m_tagReader.readMetaData(track);

    EXPECT_EQ(track.type(), Core::Track::Type::MP4);
    EXPECT_EQ(track.title(), "M4A Test");
    EXPECT_EQ(track.album(), "Fooyin Audio Tests");
    EXPECT_EQ(track.albumArtist(), "Fooyin");
    EXPECT_EQ(track.artist(), "Fooyin");
    EXPECT_EQ(track.date(), "2023");
    EXPECT_EQ(track.trackNumber(), 3);
    EXPECT_EQ(track.trackTotal(), 7);
    EXPECT_EQ(track.discNumber(), 1);
    EXPECT_EQ(track.discTotal(), 1);
    EXPECT_EQ(track.genre(), "Testing");
    EXPECT_EQ(track.composer(), "Fooyin");
    EXPECT_EQ(track.comment(), "A fooyin test");
}

TEST_F(TagReaderTest, Mp3Read)
{
    const TempResource file{u":/audio/audiotest.mp3"_s};
    fileValid(file, u":/audio/audiotest.mp3"_s);

    Core::Track track{file.fileName()};
    m_tagReader.readMetaData(track);

    EXPECT_EQ(track.type(), Core::Track::Type::MPEG);
    EXPECT_EQ(track.title(), "MP3 Test");
    EXPECT_EQ(track.album(), "Fooyin Audio Tests");
    EXPECT_EQ(track.albumArtist(), "Fooyin");
    EXPECT_EQ(track.artist(), "Fooyin");
    EXPECT_EQ(track.date(), "2023");
    EXPECT_EQ(track.trackNumber(), 4);
    EXPECT_EQ(track.trackTotal(), 7);
    EXPECT_EQ(track.discNumber(), 1);
    EXPECT_EQ(track.discTotal(), 1);
    EXPECT_EQ(track.genre(), "Testing");
    EXPECT_EQ(track.performer(), "Fooyin");
    EXPECT_EQ(track.composer(), "Fooyin");
    EXPECT_EQ(track.comment(), "A fooyin test");

    const auto testTag = track.extraTag(u"TEST"_s);
    ASSERT_TRUE(!testTag.isEmpty());
    EXPECT_EQ(testTag.front(), "A custom tag");
}

TEST_F(TagReaderTest, OggRead)
{
    const TempResource file{u":/audio/audiotest.ogg"_s};
    fileValid(file, u":/audio/audiotest.ogg"_s);

    Core::Track track{file.fileName()};
    m_tagReader.readMetaData(track);

    EXPECT_EQ(track.type(), Core::Track::Type::OggVorbis);
    EXPECT_EQ(track.title(), "OGG Test");
    EXPECT_EQ(track.album(), "Fooyin Audio Tests");
    EXPECT_EQ(track.albumArtist(), "Fooyin");
    EXPECT_EQ(track.artist(), "Fooyin");
    EXPECT_EQ(track.date(), "2023");
    EXPECT_EQ(track.trackNumber(), 5);
    EXPECT_EQ(track.trackTotal(), 7);
    EXPECT_EQ(track.discNumber(), 1);
    EXPECT_EQ(track.discTotal(), 1);
    EXPECT_EQ(track.genre(), "Testing");
    EXPECT_EQ(track.performer(), "Fooyin");
    EXPECT_EQ(track.composer(), "Fooyin");
    EXPECT_EQ(track.comment(), "A fooyin test");

    const auto testTag = track.extraTag(u"TEST"_s);
    ASSERT_TRUE(!testTag.isEmpty());
    EXPECT_EQ(testTag.front(), "A custom tag");
}

TEST_F(TagReaderTest, OpusRead)
{
    const TempResource file{u":/audio/audiotest.opus"_s};
    fileValid(file, u":/audio/audiotest.opus"_s);

    Core::Track track{file.fileName()};
    m_tagReader.readMetaData(track);

    EXPECT_EQ(track.type(), Core::Track::Type::OggOpus);
    EXPECT_EQ(track.title(), "OPUS Test");
    EXPECT_EQ(track.album(), "Fooyin Audio Tests");
    EXPECT_EQ(track.albumArtist(), "Fooyin");
    EXPECT_EQ(track.artist(), "Fooyin");
    EXPECT_EQ(track.date(), "2023");
    EXPECT_EQ(track.trackNumber(), 6);
    EXPECT_EQ(track.trackTotal(), 7);
    EXPECT_EQ(track.discNumber(), 1);
    EXPECT_EQ(track.discTotal(), 1);
    EXPECT_EQ(track.genre(), "Testing");
    EXPECT_EQ(track.performer(), "Fooyin");
    EXPECT_EQ(track.composer(), "Fooyin");
    EXPECT_EQ(track.comment(), "A fooyin test");

    const auto testTag = track.extraTag(u"TEST"_s);
    ASSERT_TRUE(!testTag.isEmpty());
    EXPECT_EQ(testTag.front(), "A custom tag");
}

TEST_F(TagReaderTest, WavRead)
{
    const TempResource file{u":/audio/audiotest.wav"_s};
    fileValid(file, u":/audio/audiotest.wav"_s);

    Core::Track track{file.fileName()};
    m_tagReader.readMetaData(track);

    EXPECT_EQ(track.type(), Core::Track::Type::WAV);
    EXPECT_EQ(track.title(), "WAV Test");
    EXPECT_EQ(track.album(), "Fooyin Audio Tests");
    EXPECT_EQ(track.albumArtist(), "Fooyin");
    EXPECT_EQ(track.artist(), "Fooyin");
    EXPECT_EQ(track.date(), "2023");
    EXPECT_EQ(track.trackNumber(), 7);
    EXPECT_EQ(track.trackTotal(), 7);
    EXPECT_EQ(track.discNumber(), 1);
    EXPECT_EQ(track.discTotal(), 1);
    EXPECT_EQ(track.genre(), "Testing");
    EXPECT_EQ(track.performer(), "Fooyin");
    EXPECT_EQ(track.composer(), "Fooyin");
    EXPECT_EQ(track.comment(), "A fooyin test");

    const auto testTag = track.extraTag(u"TEST"_s);
    ASSERT_TRUE(!testTag.isEmpty());
    EXPECT_EQ(testTag.front(), "A custom tag");
}
} // namespace Fy::Testing
