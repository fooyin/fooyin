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

#include <core/engine/verification/accuraterip.h>

#include <QtEndian>

#include <gtest/gtest.h>

#include <cstring>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
namespace {
Track albumTrack(int number, int total)
{
    Track track{u"/music/album/track%1.flac"_s.arg(number)};
    track.setTrackNumber(QString::number(number));
    track.setTrackTotal(QString::number(total));
    return track;
}

AudioBuffer makeBuffer(const std::vector<uint32_t>& samples, const AudioFormat& format)
{
    QByteArray pcm(static_cast<qsizetype>(samples.size() * sizeof(uint32_t)), '\0');
    for(size_t index{0}; index < samples.size(); ++index) {
        const uint32_t value = qToLittleEndian(samples.at(index));
        std::memcpy(pcm.data() + static_cast<qsizetype>(index * sizeof(value)), &value, sizeof(value));
    }
    return {reinterpret_cast<const uint8_t*>(pcm.constData()), static_cast<size_t>(pcm.size()), format, 0};
}

TEST(AccurateRipCoreTest, CalculatesDiscIdAndAddressFromLayout)
{
    const AccurateRip::DiscLayout layout{.tracks        = {{.firstSector = 0, .endSectorExclusive = 15750},
                                                           {.firstSector = 15750, .endSectorExclusive = 31500},
                                                           {.firstSector = 31500, .endSectorExclusive = 47250}},
                                         .leadoutSector = 47250};

    const auto id = AccurateRip::discId(layout);

    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(id->trackCount, 3);
    EXPECT_EQ(id->id1, 94500U);
    EXPECT_EQ(id->id2, 315001);
    EXPECT_EQ(AccurateRip::discUrl(*id).scheme(), u"https"_s);
    EXPECT_TRUE(AccurateRip::discUrl(*id).path().endsWith(u".bin"_s));
}

TEST(AccurateRipCoreTest, OrdersAndValidatesACompleteAlbumSelection)
{
    const Track first  = albumTrack(1, 3);
    const Track second = albumTrack(2, 3);
    const Track third  = albumTrack(3, 3);

    const auto prepared = AccurateRip::prepareAlbumTracks({third, first, second});

    ASSERT_TRUE(prepared.has_value());
    ASSERT_EQ(prepared->size(), 3);
    EXPECT_EQ(prepared->at(0).trackNumber(), u"1"_s);
    EXPECT_EQ(prepared->at(1).trackNumber(), u"2"_s);
    EXPECT_EQ(prepared->at(2).trackNumber(), u"3"_s);

    EXPECT_FALSE(AccurateRip::prepareAlbumTracks({first, third}).has_value());
}

TEST(AccurateRipCoreTest, RejectsKnownNonCdAudioBeforeDecoding)
{
    Track highResolution = albumTrack(1, 1);
    highResolution.setSampleRate(48000);
    highResolution.setChannels(2);
    highResolution.setBitDepth(24);
    highResolution.setEncoding(u"Lossless"_s);

    const auto prepared = AccurateRip::prepareAlbumTracks({highResolution});

    ASSERT_FALSE(prepared.has_value());
    EXPECT_TRUE(prepared.error().contains(u"16-bit, 44.1 kHz stereo"_s));
}

TEST(AccurateRipCoreTest, RejectsMalformedDatabaseRecords)
{
    static constexpr AccurateRip::DiscId expected{.id1 = 1, .id2 = 2, .cddbId = 3, .trackCount = 1};
    EXPECT_FALSE(AccurateRip::parseResponse(QByteArray{12, '\0'}, expected).has_value());
}

TEST(AccurateRipCoreTest, DetectsTheHighestConfidencePressingOffset)
{
    static constexpr int Sectors = 500;
    static constexpr int Offset  = 102;

    Track track = albumTrack(1, 1);
    const AudioFormat format{SampleFormat::S16, 44100, 2};
    std::vector<uint32_t> samples(Sectors * AccurateRip::FramesPerSector);
    for(size_t index{0}; index < samples.size(); ++index) {
        samples.at(index) = static_cast<uint32_t>(index * 2654435761);
    }

    uint32_t crc450{0};
    static constexpr size_t start = 450ULL * AccurateRip::FramesPerSector + Offset;
    for(int index{0}; index < AccurateRip::FramesPerSector; ++index) {
        crc450 += samples.at(start + static_cast<size_t>(index)) * static_cast<uint32_t>(index + 1);
    }

    AccurateRip::Pressing lowerConfidence(1);
    lowerConfidence.front() = {.confidence = 2, .crc = 1, .offsetChecksum = crc450};
    AccurateRip::Pressing higherConfidence(1);
    higherConfidence.front() = {.confidence = 9, .crc = 2, .offsetChecksum = crc450};

    AccurateRip::AlbumVerifier verifier{{track}};
    verifier.trackStarted(track, format);
    verifier.sourceAudio(track, makeBuffer(samples, format));
    verifier.trackFinished(track, true);

    const auto match = verifier.bestOffset({lowerConfidence, higherConfidence});
    EXPECT_EQ(match.sampleOffset, Offset);
    EXPECT_EQ(match.matchingTracks, 1);
    EXPECT_EQ(match.confidence, 11);
}

TEST(AccurateRipCoreTest, CalculatesVersionTwoChecksumsAcrossTrackBoundariesAtOffset)
{
    static constexpr int Offset       = 102;
    static constexpr int TrackSectors = 12;

    static constexpr size_t TrackFrames = TrackSectors * AccurateRip::FramesPerSector;

    const Track first  = albumTrack(1, 2);
    const Track second = albumTrack(2, 2);
    const TrackList tracks{first, second};

    const AccurateRip::DiscLayout layout{.tracks
                                         = {{.firstSector = 0, .endSectorExclusive = TrackSectors},
                                            {.firstSector = TrackSectors, .endSectorExclusive = 2 * TrackSectors}},
                                         .leadoutSector = 2 * TrackSectors};
    std::vector<uint32_t> discSamples(2 * TrackFrames);
    for(size_t index{0}; index < discSamples.size(); ++index) {
        discSamples.at(index) = static_cast<uint32_t>((index + 17) * 2246822519U);
    }

    AccurateRip::Pressing pressing(2);
    for(size_t trackIndex{0}; trackIndex < tracks.size(); ++trackIndex) {
        uint32_t crcV2{0};
        for(uint64_t position{1}; position <= TrackFrames; ++position) {
            if((trackIndex == 0 && position < 5ULL * AccurateRip::FramesPerSector)
               || (trackIndex == 1 && position > TrackFrames - 5ULL * AccurateRip::FramesPerSector)) {
                continue;
            }
            const int64_t source   = static_cast<int64_t>(trackIndex * TrackFrames + position - 1) + Offset;
            const uint64_t product = static_cast<uint64_t>(discSamples.at(static_cast<size_t>(source))) * position;
            crcV2 += static_cast<uint32_t>(product) + static_cast<uint32_t>(product >> 32);
        }
        pressing.at(trackIndex) = {.confidence = 7, .crc = crcV2, .offsetChecksum = 0};
    }

    const AudioFormat format{SampleFormat::S16, 44100, 2};
    AccurateRip::OffsetVerifier verifier{layout, {pressing}, tracks, Offset};
    for(size_t index{0}; index < tracks.size(); ++index) {
        const auto begin = discSamples.cbegin() + static_cast<std::ptrdiff_t>(index * TrackFrames);
        const std::vector<uint32_t> trackSamples{begin, begin + TrackFrames};
        verifier.trackStarted(tracks.at(index), format);
        verifier.sourceAudio(tracks.at(index), makeBuffer(trackSamples, format));
        verifier.trackFinished(tracks.at(index), true);
    }

    const auto results = verifier.results();
    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results.at(0).status, AccurateRip::VerifyStatus::Verified);
    EXPECT_EQ(results.at(1).status, AccurateRip::VerifyStatus::Verified);
    EXPECT_EQ(results.at(0).confidence, 7);
    EXPECT_EQ(results.at(0).sampleOffset, Offset);
}
} // namespace
} // namespace Fooyin::Testing
