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

#include <core/engine/audiobuffer.h>
#include <core/engine/audioconverter.h>
#include <core/engine/audioformat.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace Fooyin::Testing {
namespace {
constexpr int SampleRate = 48000;
constexpr int Channels   = 2;
constexpr int Frames     = 9;

constexpr std::array<SampleFormat, 6> Formats = {SampleFormat::U8,  SampleFormat::S16, SampleFormat::S24In32,
                                                 SampleFormat::S32, SampleFormat::F32, SampleFormat::F64};

constexpr std::array<double, Frames> NormalizedPattern = {-1.0, -0.75, -0.5, -0.125, 0.0, 0.125, 0.5, 0.75, 0.9921875};

template <typename T>
T quantizeToSigned(const double sample, const double scale, const T minValue, const T maxValue)
{
    const double lo      = static_cast<double>(minValue) / scale;
    const double hi      = static_cast<double>(maxValue) / scale;
    const double x       = std::clamp(sample, lo, hi);
    const long long v    = std::llrint(x * scale);
    const long long minI = static_cast<long long>(minValue);
    const long long maxI = static_cast<long long>(maxValue);
    return static_cast<T>(std::clamp(v, minI, maxI));
}

void writeSample(std::byte* dst, SampleFormat format, const double normalized)
{
    switch(format) {
        case SampleFormat::U8: {
            const int16_t centered = quantizeToSigned<int16_t>(normalized, 128.0, int16_t{-128}, int16_t{127});
            const auto value       = static_cast<uint8_t>(centered + 128);
            std::memcpy(dst, &value, sizeof(value));
            break;
        }
        case SampleFormat::S16: {
            const auto value = quantizeToSigned<int16_t>(normalized, 32768.0, std::numeric_limits<int16_t>::min(),
                                                         std::numeric_limits<int16_t>::max());
            std::memcpy(dst, &value, sizeof(value));
            break;
        }
        case SampleFormat::S24In32: {
            const auto value = quantizeToSigned<int32_t>(normalized, 8388608.0, int32_t{-8388608}, int32_t{8388607});
            std::memcpy(dst, &value, sizeof(value));
            break;
        }
        case SampleFormat::S32: {
            const auto value = quantizeToSigned<int32_t>(normalized, 2147483648.0, std::numeric_limits<int32_t>::min(),
                                                         std::numeric_limits<int32_t>::max());
            std::memcpy(dst, &value, sizeof(value));
            break;
        }
        case SampleFormat::F32: {
            const auto value = static_cast<float>(normalized);
            std::memcpy(dst, &value, sizeof(value));
            break;
        }
        case SampleFormat::F64: {
            const auto value = normalized;
            std::memcpy(dst, &value, sizeof(value));
            break;
        }
        case SampleFormat::Unknown:
            break;
    }
}

AudioBuffer makeBuffer(SampleFormat format, int channels = Channels, int frames = Frames)
{
    const AudioFormat audioFormat{format, SampleRate, channels};
    std::vector<std::byte> raw(static_cast<size_t>(audioFormat.bytesForFrames(frames)));

    const int bps = audioFormat.bytesPerSample();
    for(int frame{0}; frame < frames; ++frame) {
        for(int channel{0}; channel < channels; ++channel) {
            const auto sampleIndex
                = static_cast<size_t>(frame * channels + channel) % static_cast<size_t>(NormalizedPattern.size());
            const double sample = NormalizedPattern[sampleIndex];
            const auto offset   = static_cast<size_t>((frame * channels + channel) * bps);
            writeSample(raw.data() + offset, format, sample);
        }
    }

    return AudioBuffer{std::span<const std::byte>{raw.data(), raw.size()}, audioFormat, 0};
}

bool equalBytes(const AudioBuffer& lhs, const AudioBuffer& rhs)
{
    if(!lhs.isValid() || !rhs.isValid()) {
        return false;
    }
    if(lhs.byteCount() != rhs.byteCount()) {
        return false;
    }
    return std::equal(lhs.constData().begin(), lhs.constData().end(), rhs.constData().begin(), rhs.constData().end());
}
} // namespace

TEST(AudioConverterTest, ConvertsEverySupportedFormatPair)
{
    for(const auto inputFormat : Formats) {
        SCOPED_TRACE(static_cast<int>(inputFormat));
        const AudioBuffer input = makeBuffer(inputFormat);
        ASSERT_TRUE(input.isValid());

        for(const auto outputFormat : Formats) {
            SCOPED_TRACE(static_cast<int>(outputFormat));
            const AudioFormat outFmt{outputFormat, SampleRate, Channels};
            const auto converted = Audio::convert(input, outFmt);

            ASSERT_TRUE(converted.isValid());
            EXPECT_EQ(converted.format(), outFmt);
            EXPECT_EQ(converted.frameCount(), input.frameCount());
            EXPECT_EQ(converted.byteCount(), outFmt.bytesForFrames(input.frameCount()));

            if(inputFormat == outputFormat) {
                EXPECT_TRUE(equalBytes(input, converted));
            }
        }
    }
}

TEST(AudioConverterTest, RoundTripThroughFloat64PreservesOriginalSamples)
{
    const AudioFormat float64Format{SampleFormat::F64, SampleRate, Channels};

    for(const auto format : Formats) {
        SCOPED_TRACE(static_cast<int>(format));
        const AudioBuffer input = makeBuffer(format);
        ASSERT_TRUE(input.isValid());

        const auto toFloat64 = Audio::convert(input, float64Format);
        ASSERT_TRUE(toFloat64.isValid());

        const AudioFormat originalFormat{format, SampleRate, Channels};
        const auto roundTrip = Audio::convert(toFloat64, originalFormat);
        ASSERT_TRUE(roundTrip.isValid());

        EXPECT_TRUE(equalBytes(input, roundTrip));
    }
}

TEST(AudioConverterTest, RemapsChannelsUsingExplicitLayout)
{
    const std::array<double, 6> inputSamples = {1.0, 10.0, 2.0, 20.0, 3.0, 30.0};
    std::array<std::byte, inputSamples.size() * sizeof(double)> raw{};
    std::memcpy(raw.data(), inputSamples.data(), raw.size());

    AudioFormat inFormat{SampleFormat::F64, SampleRate, 2};
    inFormat.setChannelLayout({AudioFormat::ChannelPosition::FrontLeft, AudioFormat::ChannelPosition::FrontRight});

    const AudioBuffer input{std::span<const std::byte>{raw.data(), raw.size()}, inFormat, 0};
    ASSERT_TRUE(input.isValid());

    AudioFormat outFormat{SampleFormat::F64, SampleRate, 2};
    outFormat.setChannelLayout({AudioFormat::ChannelPosition::FrontRight, AudioFormat::ChannelPosition::FrontLeft});

    const auto output = Audio::convert(input, outFormat);
    ASSERT_TRUE(output.isValid());
    ASSERT_EQ(output.byteCount(), static_cast<int>(raw.size()));

    std::array<double, inputSamples.size()> mapped{};
    std::memcpy(mapped.data(), output.constData().data(), output.constData().size());

    const std::array<double, 6> expected = {10.0, 1.0, 20.0, 2.0, 30.0, 3.0};
    EXPECT_EQ(mapped, expected);
}

TEST(AudioConverterTest, ConvertsFloatToS16WithDitherEnabled)
{
    const AudioFormat outFormat{SampleFormat::S16, SampleRate, Channels};

    for(const auto inputFormat : {SampleFormat::F32, SampleFormat::F64}) {
        SCOPED_TRACE(static_cast<int>(inputFormat));
        const AudioBuffer input = makeBuffer(inputFormat);
        ASSERT_TRUE(input.isValid());

        const auto converted = Audio::convert(input, outFormat, true);
        ASSERT_TRUE(converted.isValid());
        EXPECT_EQ(converted.format(), outFormat);
        EXPECT_EQ(converted.frameCount(), input.frameCount());
    }
}
} // namespace Fooyin::Testing
