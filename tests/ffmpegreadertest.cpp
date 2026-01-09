/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "core/engine/ffmpeg/ffmpeginput.h"

#include <core/track.h>

#include <QBuffer>

#include <gtest/gtest.h>

namespace Fooyin::Testing {
class FFmpegReaderTest : public ::testing::Test
{
protected:
    FFmpegReader m_reader;
};

TEST_F(FFmpegReaderTest, MpcApev2TagRead)
{
    const QString filepath = QStringLiteral(":/audio/audiotest.mpc");
    TempResource file{filepath};
    file.checkValid();

    Track track{filepath};
    ASSERT_TRUE(m_reader.readTrack({filepath, &file, nullptr}, track));

    EXPECT_EQ(track.title(), QStringLiteral("APEv2 Test"));
    EXPECT_EQ(track.album(), QStringLiteral("Fooyin Audio Tests"));
    EXPECT_EQ(track.albumArtist(), QStringLiteral("Test Album Artist"));
    EXPECT_EQ(track.trackNumber(), QStringLiteral("1"));
    EXPECT_EQ(track.trackTotal(), QStringLiteral("10"));
    EXPECT_EQ(track.discNumber(), QStringLiteral("1"));
    EXPECT_EQ(track.discTotal(), QStringLiteral("2"));
    EXPECT_EQ(track.date(), QStringLiteral("2024"));
    EXPECT_EQ(track.comment(), QStringLiteral("A fooyin APEv2 test"));
}

TEST_F(FFmpegReaderTest, MpcApev2MultiValueTags)
{
    const QString filepath = QStringLiteral(":/audio/audiotest.mpc");
    TempResource file{filepath};
    file.checkValid();

    Track track{filepath};
    ASSERT_TRUE(m_reader.readTrack({filepath, &file, nullptr}, track));

    // Multi-value artist field (null-separated in APEv2)
    const QStringList artists = track.artists();
    ASSERT_EQ(artists.size(), 2);
    EXPECT_EQ(artists.at(0), QStringLiteral("Artist One"));
    EXPECT_EQ(artists.at(1), QStringLiteral("Artist Two"));

    // Multi-value genre field
    const QStringList genres = track.genres();
    ASSERT_EQ(genres.size(), 2);
    EXPECT_EQ(genres.at(0), QStringLiteral("Classical"));
    EXPECT_EQ(genres.at(1), QStringLiteral("Testing"));

    // Multi-value composer field
    const QStringList composers = track.composers();
    ASSERT_EQ(composers.size(), 2);
    EXPECT_EQ(composers.at(0), QStringLiteral("Composer One"));
    EXPECT_EQ(composers.at(1), QStringLiteral("Composer Two"));
}

TEST_F(FFmpegReaderTest, MpcApev2ExtraTags)
{
    const QString filepath = QStringLiteral(":/audio/audiotest.mpc");
    TempResource file{filepath};
    file.checkValid();

    Track track{filepath};
    ASSERT_TRUE(m_reader.readTrack({filepath, &file, nullptr}, track));

    // Conductor is stored as extra tag
    const auto conductor = track.extraTag(QStringLiteral("CONDUCTOR"));
    ASSERT_FALSE(conductor.isEmpty());
    EXPECT_EQ(conductor.front(), QStringLiteral("Test Conductor"));
}

TEST_F(FFmpegReaderTest, MpcApev2CoverArtType2)
{
    const QString filepath = QStringLiteral(":/audio/audiotest.mpc");
    TempResource file{filepath};
    file.checkValid();

    Track track{filepath};
    ASSERT_TRUE(m_reader.readTrack({filepath, &file, nullptr}, track));

    // Test front cover (APEv2 type 2 item - some taggers use this instead of type 1)
    const QByteArray frontCover = m_reader.readCover({filepath, &file, nullptr}, track, Track::Cover::Front);
    ASSERT_FALSE(frontCover.isEmpty());
    // Check JPEG magic bytes
    EXPECT_EQ(static_cast<uint8_t>(frontCover.at(0)), 0xFF);
    EXPECT_EQ(static_cast<uint8_t>(frontCover.at(1)), 0xD8);
    EXPECT_TRUE(frontCover.contains("FRONT"));

    // Reset file position
    file.seek(0);

    // Test back cover
    const QByteArray backCover = m_reader.readCover({filepath, &file, nullptr}, track, Track::Cover::Back);
    ASSERT_FALSE(backCover.isEmpty());
    EXPECT_EQ(static_cast<uint8_t>(backCover.at(0)), 0xFF);
    EXPECT_EQ(static_cast<uint8_t>(backCover.at(1)), 0xD8);
    EXPECT_TRUE(backCover.contains("BACK"));

    // Verify they are different covers
    EXPECT_NE(frontCover, backCover);
}

TEST_F(FFmpegReaderTest, MalformedApev2TruncatedFooter)
{
    // Create a file that's too small to contain APE footer (< 32 bytes)
    QByteArray smallData(20, '\0');
    QBuffer buffer(&smallData);
    buffer.open(QIODevice::ReadOnly);

    Track track{QStringLiteral("test.mpc")};
    // Should not crash and should fail gracefully
    EXPECT_FALSE(m_reader.readTrack({QStringLiteral("test.mpc"), &buffer, nullptr}, track));
}

TEST_F(FFmpegReaderTest, MalformedApev2InvalidPreamble)
{
    // Create a file with invalid APE preamble (garbage at footer position)
    QByteArray data(100, '\0');
    // Write garbage where APE footer would be (last 32 bytes)
    data.replace(data.size() - 32, 8, "NOTAPETG");

    QBuffer buffer(&data);
    buffer.open(QIODevice::ReadOnly);

    Track track{QStringLiteral("test.mpc")};
    // Should not crash - just return with no APE tags parsed
    // FFmpeg may still try to read the file, which will fail, but that's expected
    [[maybe_unused]] bool result = m_reader.readTrack({QStringLiteral("test.mpc"), &buffer, nullptr}, track);

    // Track should have empty metadata (no crash)
    EXPECT_TRUE(track.title().isEmpty());
    EXPECT_TRUE(track.artists().isEmpty());
}

TEST_F(FFmpegReaderTest, MalformedApev2InvalidTagSize)
{
    // Create APE footer with valid preamble but absurd tag size
    QByteArray data(100, '\0');
    const int footerStart = data.size() - 32;

    // APE preamble "APETAGEX"
    data.replace(footerStart, 8, "APETAGEX");
    // Version 2000 (little-endian)
    data[footerStart + 8]  = static_cast<char>(0xD0);
    data[footerStart + 9]  = static_cast<char>(0x07);
    data[footerStart + 10] = 0;
    data[footerStart + 11] = 0;
    // Tag size: 0xFFFFFFFF (impossibly large, little-endian)
    data[footerStart + 12] = static_cast<char>(0xFF);
    data[footerStart + 13] = static_cast<char>(0xFF);
    data[footerStart + 14] = static_cast<char>(0xFF);
    data[footerStart + 15] = static_cast<char>(0xFF);

    QBuffer buffer(&data);
    buffer.open(QIODevice::ReadOnly);

    Track track{QStringLiteral("test.mpc")};
    // Should not crash - APE reader should reject invalid tag size
    [[maybe_unused]] bool result = m_reader.readTrack({QStringLiteral("test.mpc"), &buffer, nullptr}, track);

    // Track should have empty metadata (no crash)
    EXPECT_TRUE(track.title().isEmpty());
}
} // namespace Fooyin::Testing
