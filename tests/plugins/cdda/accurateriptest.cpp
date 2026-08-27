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
 */

#include "accuraterip.h"

#include <QByteArray>
#include <QtEndian>

#include <gtest/gtest.h>

#include <cstring>

using namespace Qt::StringLiterals;

namespace Fooyin::Cdda {
namespace {
CdToc simpleToc()
{
    return {.firstTrackNumber = 1,
            .lastTrackNumber  = 3,
            .leadoutSector    = 3,
            .tracks           = {{.number = 1, .firstSector = 0, .endSectorExclusive = 1, .isAudio = true},
                                 {.number = 2, .firstSector = 1, .endSectorExclusive = 2, .isAudio = true},
                                 {.number = 3, .firstSector = 2, .endSectorExclusive = 3, .isAudio = true}}};
}

void appendLe32(QByteArray& data, uint32_t value)
{
    const uint32_t little = qToLittleEndian(value);
    data.append(reinterpret_cast<const char*>(&little), sizeof(little));
}
} // namespace

TEST(AccurateRipTest, CalculatesDiscIdAndUrl)
{
    const CdToc toc{.firstTrackNumber = 1,
                    .lastTrackNumber  = 3,
                    .leadoutSector    = 300,
                    .tracks = {{.number = 1, .firstSector = 0, .endSectorExclusive = 100, .isAudio = true},
                               {.number = 2, .firstSector = 100, .endSectorExclusive = 200, .isAudio = true},
                               {.number = 3, .firstSector = 200, .endSectorExclusive = 300, .isAudio = true}}};

    const auto id = accurateRipDiscId(toc);
    ASSERT_TRUE(id.has_value()) << id.error().toStdString();
    EXPECT_EQ(600, id->id1);
    EXPECT_EQ(2001, id->id2);
    EXPECT_EQ(0x09000403, id->cddbId);
    EXPECT_EQ(u"https://www.accuraterip.com/accuraterip/8/5/2/dBAR-003-00000258-000007d1-09000403.bin"_s,
              accurateRipDiscUrl(*id).toString());
}

TEST(AccurateRipTest, CalculatesPublishedDiscRecordAddress)
{
    const std::vector<int> offsets{0,      22617,  41737,  58167,  71952,  91225,
                                   104502, 115230, 132015, 143782, 159720, 174447};
    CdToc toc{.firstTrackNumber = 1, .lastTrackNumber = 12, .leadoutSector = 267107, .tracks = {}};
    for(int index{0}; std::cmp_less(index, offsets.size()); ++index) {
        toc.tracks.push_back(
            {.number             = index + 1,
             .firstSector        = offsets.at(index),
             .endSectorExclusive = std::cmp_less(index + 1, offsets.size()) ? offsets.at(index + 1) : 267107,
             .isAudio            = true});
    }

    const auto id = accurateRipDiscId(toc);
    ASSERT_TRUE(id.has_value()) << id.error().toStdString();
    EXPECT_EQ(0x00151865, id->id1);
    EXPECT_EQ(0x00c50650, id->id2);
    EXPECT_EQ(0xa70de90c, id->cddbId);
    EXPECT_EQ(u"https://www.accuraterip.com/accuraterip/5/6/8/dBAR-012-00151865-00c50650-a70de90c.bin"_s,
              accurateRipDiscUrl(*id).toString());
}

TEST(AccurateRipTest, ParsesMatchingBinaryRecords)
{
    const AccurateRipDiscId id{.id1 = 1, .id2 = 2, .cddbId = 3, .trackCount = 2};
    QByteArray data;
    data.append(char{2});
    appendLe32(data, 1);
    appendLe32(data, 2);
    appendLe32(data, 3);
    data.append(char{7});
    appendLe32(data, 0x11223344);
    appendLe32(data, 0x55667788);
    data.append(char{9});
    appendLe32(data, 0xaabbccdd);
    appendLe32(data, 0x01020304);

    const auto records = parseAccurateRipResponse(data, id);
    ASSERT_TRUE(records.has_value()) << records.error().toStdString();
    ASSERT_EQ(1, records->size());
    EXPECT_EQ(7, records->front().front().confidence);
    EXPECT_EQ(0x11223344, records->front().front().crc);
    EXPECT_EQ(0x01020304, records->front().back().crc2);
}

TEST(AccurateRipTest, ParsesAndMatchesDriveOffsetsExactly)
{
    const QByteArray html
        = R"(<table><tr><td>LG Electronics - DVDRAM GH24NSD5</td><td>+6</td><td>42</td><td>100%</td></tr></table>)";
    const auto offsets = parseAccurateRipDriveOffsets(html);
    ASSERT_EQ(1, offsets.size());
    const auto match = findAccurateRipDriveOffset(offsets, u"HL-DT-ST"_s, u"DVDRAM GH24NSD5"_s);
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(6, match->correctionSampleFrames);
    EXPECT_EQ(42, match->submissions);
}

TEST(AccurateRipTest, VerifiesOffsetCorrectedSourcePcm)
{
    AccurateRipPressing pressing(3);
    pressing[1] = {.confidence = 5, .crc = 14, .crc2 = 0};
    AccurateRipVerifier verifier{simpleToc(), {pressing}};
    Track track{u"cdda:///abcdefghijklmnopqrstuvwx1234"_s, 1};
    track.setTrackNumber(u"2"_s);

    QByteArray pcm(FramesPerSector * 4LL, '\0');
    const uint32_t values[]{qToLittleEndian(1), qToLittleEndian(2), qToLittleEndian(3)};
    std::memcpy(pcm.data(), values, sizeof(values));
    const AudioFormat format{SampleFormat::S16, 44100, 2};
    const AudioBuffer buffer{reinterpret_cast<const uint8_t*>(pcm.constData()), static_cast<size_t>(pcm.size()), format,
                             0};

    verifier.trackStarted(track, format);
    verifier.sourceAudio(track, buffer);
    verifier.trackFinished(track, true);

    const auto results = verifier.results();
    ASSERT_EQ(1, results.size());
    EXPECT_EQ(AccurateRipVerifyStatus::Verified, results.front().status);
    EXPECT_EQ(14U, results.front().crcV1);
    EXPECT_EQ(5, results.front().confidence);
    EXPECT_EQ((std::vector<uint32_t>{14U}), results.front().databaseCrcs);
}
} // namespace Fooyin::Cdda