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

#include <core/engine/audioconverter.h>

#include <core/engine/audiobuffer.h>
#include <utils/math.h>

#include <cfenv>

namespace {
using ChannelMap = std::array<int, 32>;

template <typename InputType, typename OutputType, typename Func>
void convert(const Fooyin::AudioFormat& inputFormat, const std::byte* input, const Fooyin::AudioFormat& outputFormat,
             std::byte* output, int sampleCount, const ChannelMap& channelMap, Func&& conversionFunc)
{
    const int inBps  = inputFormat.bytesPerSample();
    const int outBps = outputFormat.bytesPerSample();

    const int inChannels  = inputFormat.channelCount();
    const int outChannels = outputFormat.channelCount();

    for(int i{0}; i < sampleCount; ++i) {
        for(int ch{0}; ch < outChannels; ++ch) {
            if(channelMap.at(ch) < 0) {
                continue;
            }

            InputType inSample;
            const auto inOffset = (i * inChannels + channelMap.at(ch)) * inBps;
            std::memcpy(&inSample, input + inOffset, inBps);

            OutputType outSample = conversionFunc(inSample);
            const auto outOffset = (i * outChannels + channelMap.at(ch)) * outBps;
            std::memcpy(output + outOffset, &outSample, outBps);
        }
    }
}

uint8_t convertU8ToU8(const uint8_t inSample)
{
    return inSample;
}

int16_t convertU8ToS16(const uint8_t inSample)
{
    return static_cast<int16_t>((inSample ^ 0x80) << 8);
}

int32_t convertU8ToS32(const uint8_t inSample)
{
    return inSample ^ 0x80 << 24;
}

float convertU8ToFloat(const uint8_t inSample)
{
    return static_cast<float>(inSample) / 0x80 - 1.0F;
}

int16_t convertS16ToS16(const int16_t inSample)
{
    return inSample;
}

uint8_t convertS16ToU8(const int16_t inSample)
{
    return static_cast<uint8_t>(inSample >> 8 ^ 0x80);
}

int32_t convertS16ToS32(const int16_t inSample)
{
    return inSample << 16;
}

float convertS16ToFloat(const int16_t inSample)
{
    return static_cast<float>(inSample) / static_cast<float>(std::numeric_limits<int16_t>::max());
}

uint8_t convertS32ToU8(const int32_t inSample)
{
    return static_cast<int8_t>(inSample >> 24 ^ 0x80);
}

int16_t convertS32ToS16(const int32_t inSample)
{
    return static_cast<int16_t>(inSample >> 16);
}

int32_t convertS32ToS32(const int32_t inSample)
{
    return inSample;
}

float convertS32ToFloat(const int32_t inSample)
{
    return static_cast<float>(inSample) / static_cast<float>(std::numeric_limits<int32_t>::max());
}

uint8_t convertFloatToU8(const float inSample)
{
    const int prevRoundingMode = std::fegetround();
    std::fesetround(FE_TONEAREST);

    static constexpr auto minS8 = static_cast<int>(std::numeric_limits<int8_t>::min());
    static constexpr auto maxS8 = static_cast<int>(std::numeric_limits<int8_t>::max());

    int intSample = Fooyin::Math::fltToInt(inSample * 0x80);
    intSample     = std::clamp(intSample, minS8, maxS8);

    std::fesetround(prevRoundingMode);

    return static_cast<uint8_t>(intSample ^ 0x80);
}

int16_t convertFloatToS16(const float inSample)
{
    const int prevRoundingMode = std::fegetround();
    std::fesetround(FE_TONEAREST);

    static constexpr auto minS16 = static_cast<int>(std::numeric_limits<int16_t>::min());
    static constexpr auto maxS16 = static_cast<int>(std::numeric_limits<int16_t>::max());

    int intSample = Fooyin::Math::fltToInt(inSample * 0x8000);
    intSample     = std::clamp(intSample, minS16, maxS16);

    std::fesetround(prevRoundingMode);

    return static_cast<int16_t>(intSample);
}

int32_t convertFloatToS32(const float inSample)
{
    const int prevRoundingMode = std::fegetround();
    std::fesetround(FE_TONEAREST);

    static constexpr int minS32 = std::numeric_limits<int32_t>::min();
    static constexpr int maxS32 = std::numeric_limits<int32_t>::max();

    int intSample = Fooyin::Math::fltToInt(inSample * 0x80000000);
    intSample     = std::clamp(intSample, minS32, maxS32);

    std::fesetround(prevRoundingMode);

    return intSample;
}

float convertFloatToFloat(const float inSample)
{
    return inSample;
}

bool convertFormat(const Fooyin::AudioFormat& inFormat, const std::byte* input, const Fooyin::AudioFormat& outFormat,
                   std::byte* output, int samples)
{
    ChannelMap channels;
    std::iota(channels.begin(), channels.end(), -1);

    // TODO: Handle channel layout of output
    const int outputChannels = outFormat.channelCount();
    for(int i{0}; i < outputChannels; ++i) {
        channels.at(i) = i;
    }

    using SampleFormat = Fooyin::SampleFormat;

    switch(inFormat.sampleFormat()) {
        case(SampleFormat::U8): {
            switch(outFormat.sampleFormat()) {
                case(SampleFormat::U8):
                    convert<uint8_t, uint8_t>(inFormat, input, outFormat, output, samples, channels, convertU8ToU8);
                    return true;
                case(SampleFormat::S16):
                    convert<uint8_t, int16_t>(inFormat, input, outFormat, output, samples, channels, convertU8ToS16);
                    return true;
                case(SampleFormat::S24):
                case(SampleFormat::S32):
                    convert<uint8_t, int32_t>(inFormat, input, outFormat, output, samples, channels, convertU8ToS32);
                    return true;
                case(SampleFormat::Float):
                    convert<uint8_t, float>(inFormat, input, outFormat, output, samples, channels, convertU8ToFloat);
                    return true;
                default:
                    break;
            }
            break;
        }
        case(SampleFormat::S16): {
            switch(outFormat.sampleFormat()) {
                case(SampleFormat::U8):
                    convert<int16_t, uint8_t>(inFormat, input, outFormat, output, samples, channels, convertS16ToU8);
                    return true;
                case(SampleFormat::S16):
                    convert<int16_t, int16_t>(inFormat, input, outFormat, output, samples, channels, convertS16ToS16);
                    return true;
                case(SampleFormat::S24):
                case(SampleFormat::S32):
                    convert<int16_t, int32_t>(inFormat, input, outFormat, output, samples, channels, convertS16ToS32);
                    return true;
                case(SampleFormat::Float):
                    convert<int16_t, float>(inFormat, input, outFormat, output, samples, channels, convertS16ToFloat);
                    return true;
                default:
                    break;
            }
            break;
        }
        case(SampleFormat::S24):
        case(SampleFormat::S32): {
            switch(outFormat.sampleFormat()) {
                case(SampleFormat::U8):
                    convert<int32_t, uint8_t>(inFormat, input, outFormat, output, samples, channels, convertS32ToU8);
                    return true;
                case(SampleFormat::S16):
                    convert<int32_t, int16_t>(inFormat, input, outFormat, output, samples, channels, convertS32ToS16);
                    return true;
                case(SampleFormat::S24):
                case(SampleFormat::S32):
                    convert<int32_t, int32_t>(inFormat, input, outFormat, output, samples, channels, convertS32ToS32);
                    return true;
                case(SampleFormat::Float):
                    convert<int32_t, float>(inFormat, input, outFormat, output, samples, channels, convertS32ToFloat);
                    return true;
                default:
                    break;
            }
            break;
        }
        case(SampleFormat::Float): {
            switch(outFormat.sampleFormat()) {
                case(SampleFormat::U8):
                    convert<float, uint8_t>(inFormat, input, outFormat, output, samples, channels, convertFloatToU8);
                    return true;
                case(SampleFormat::S16):
                    convert<float, int16_t>(inFormat, input, outFormat, output, samples, channels, convertFloatToS16);
                    return true;
                case(SampleFormat::S24):
                case(SampleFormat::S32):
                    convert<float, int32_t>(inFormat, input, outFormat, output, samples, channels, convertFloatToS32);
                    return true;
                case(SampleFormat::Float):
                    convert<float, float>(inFormat, input, outFormat, output, samples, channels, convertFloatToFloat);
                    return true;
                default:
                    break;
            }
            break;
        }
        default:
            return false;
    }

    return false;
}
} // namespace

namespace Fooyin::Audio {
AudioBuffer convert(const AudioBuffer& buffer, const AudioFormat& outputFormat)
{
    if(!buffer.isValid() || !outputFormat.isValid()) {
        return {};
    }

    AudioBuffer output{outputFormat, buffer.startTime()};
    output.resize(outputFormat.bytesForFrames(buffer.frameCount()));

    if(convert(buffer.format(), buffer.constData().data(), outputFormat, output.data(), buffer.frameCount())) {
        return output;
    }

    return {};
}

bool convert(const AudioFormat& inputFormat, const std::byte* input, const AudioFormat& outputFormat, std::byte* output,
             int sampleCount)
{
    if(!inputFormat.isValid() || !outputFormat.isValid()) {
        return false;
    }

    convertFormat(inputFormat, input, outputFormat, output, sampleCount);

    return true;
}

} // namespace Fooyin::Audio
