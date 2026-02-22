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

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <random>
#include <utility>

namespace {
using ChannelMap = std::array<int, Fooyin::AudioFormat::MaxChannels>;

ChannelMap buildChannelMap(const Fooyin::AudioFormat& inFormat, const Fooyin::AudioFormat& outFormat)
{
    ChannelMap map;
    map.fill(-1);

    const int inCh  = inFormat.channelCount();
    const int outCh = outFormat.channelCount();

    if(!inFormat.hasChannelLayout() || !outFormat.hasChannelLayout()) {
        const int n = std::min(inCh, outCh);
        for(int i{0}; i < n; ++i) {
            map[i] = i;
        }
        return map;
    }

    using Pos = Fooyin::AudioFormat::ChannelPosition;

    std::array<int, 256> posToIn;
    posToIn.fill(-1);

    for(int i{0}; i < inCh; ++i) {
        const Pos p = inFormat.channelPosition(i);
        if(p != Pos::UnknownPosition) {
            posToIn[static_cast<uint8_t>(p)] = i;
        }
    }

    for(int o{0}; o < outCh; ++o) {
        const Pos p = outFormat.channelPosition(o);
        if(p == Pos::UnknownPosition) {
            if(o < inCh) {
                map[o] = o;
            }
            continue;
        }
        map[o] = posToIn[static_cast<uint8_t>(p)];
    }

    return map;
}

template <typename InputType, typename OutputType, typename Func>
void convert(const Fooyin::AudioFormat& inputFormat, const std::byte* input, const Fooyin::AudioFormat& outputFormat,
             std::byte* output, int frames, const ChannelMap& channelMap, const Func& conversionFunc)
{
    const int inBps  = inputFormat.bytesPerSample();
    const int outBps = outputFormat.bytesPerSample();

    const auto inChannels  = static_cast<size_t>(inputFormat.channelCount());
    const auto outChannels = static_cast<size_t>(outputFormat.channelCount());

    if(frames <= 0 || inChannels == 0 || outChannels == 0) {
        return;
    }

    std::array<ptrdiff_t, Fooyin::AudioFormat::MaxChannels> mappedInOffsets;
    mappedInOffsets.fill(-1);

    for(size_t outChannel{0}; outChannel < outChannels; ++outChannel) {
        const int mappedInChannel = channelMap[outChannel];
        if(mappedInChannel >= 0 && std::cmp_less(mappedInChannel, inChannels)) {
            mappedInOffsets[outChannel] = static_cast<ptrdiff_t>(mappedInChannel) * static_cast<ptrdiff_t>(inBps);
        }
    }

    const auto totalFrames      = static_cast<size_t>(frames);
    const size_t inFrameStride  = inChannels * inBps;
    const size_t outFrameStride = outChannels * outBps;

    for(size_t frame{0}; frame < totalFrames; ++frame) {
        const std::byte* inFrame = input + (frame * inFrameStride);
        std::byte* outFrame      = output + (frame * outFrameStride);

        for(size_t outCh{0}; outCh < outChannels; ++outCh) {
            const ptrdiff_t inOff = mappedInOffsets[outCh];
            if(inOff < 0) {
                continue;
            }

            InputType inSample{};
            std::memcpy(&inSample, inFrame + static_cast<size_t>(inOff), inBps);

            OutputType outSample = conversionFunc(inSample);
            std::memcpy(outFrame + (outCh * outBps), &outSample, outBps);
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
    return (static_cast<int32_t>(inSample ^ 0x80) << 24);
}

float convertU8ToFloat(const uint8_t inSample)
{
    return (static_cast<float>(inSample) * (1.0F / static_cast<float>(0x80))) - 1.0F;
}

double convertU8ToDouble(const uint8_t inSample)
{
    return (static_cast<double>(inSample) * (1.0 / static_cast<double>(0x80))) - 1.0;
}

uint8_t convertS16ToU8(const int16_t inSample)
{
    return static_cast<uint8_t>(inSample >> 8 ^ 0x80);
}

int16_t convertS16ToS16(const int16_t inSample)
{
    return inSample;
}

int32_t convertS16ToS32(const int16_t inSample)
{
    return inSample << 16;
}

float convertS16ToFloat(const int16_t inSample)
{
    return static_cast<float>(inSample) * (1.0F / static_cast<float>(0x8000));
}

double convertS16ToDouble(const int16_t inSample)
{
    return static_cast<double>(inSample) * (1.0 / static_cast<double>(0x8000));
}

uint8_t convertS32ToU8(const int32_t inSample)
{
    return static_cast<uint8_t>(inSample >> 24 ^ 0x80);
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
    return static_cast<float>(inSample) * (1.0F / static_cast<float>(0x80000000));
}

double convertS32ToDouble(const int32_t inSample)
{
    return static_cast<double>(inSample) * (1.0 / static_cast<double>(0x80000000));
}

template <typename T, typename R>
R convertToIntegral(T inSample, const T scale, const R minValue, const R maxValue)
{
    const T lo = static_cast<T>(minValue) / scale;
    const T hi = static_cast<T>(maxValue) / scale;

    inSample = std::clamp(inSample, lo, hi);

    const long long v = std::llrint(static_cast<double>(inSample) * static_cast<double>(scale));

    const auto minI = static_cast<long long>(minValue);
    const auto maxI = static_cast<long long>(maxValue);

    return static_cast<R>(std::clamp(v, minI, maxI));
}

uint8_t convertFloatToU8(const float inSample)
{
    const int16_t centered = convertToIntegral<float, int16_t>(inSample, 128.0F, int16_t{-128}, int16_t{127});
    return static_cast<uint8_t>(centered + 128);
}

int16_t convertFloatToS16(const float inSample)
{
    return convertToIntegral<float, int16_t>(inSample, 32768.0F, std::numeric_limits<int16_t>::min(),
                                             std::numeric_limits<int16_t>::max());
}

int16_t convertFloatToS16Dithered(const float inSample)
{
    constexpr float lsb = 1.0F / 32768.0F;
    thread_local std::mt19937 rng{std::random_device{}()};
    thread_local std::uniform_real_distribution<float> dist(-0.5F * lsb, 0.5F * lsb);
    return convertToIntegral<float, int16_t>(inSample + dist(rng) + dist(rng), 32768.0F,
                                             std::numeric_limits<int16_t>::min(), std::numeric_limits<int16_t>::max());
}

int32_t convertFloatToS32(const float inSample)
{
    return convertToIntegral<float, int32_t>(inSample, 2147483648.0F, std::numeric_limits<int32_t>::min(),
                                             std::numeric_limits<int32_t>::max());
}

float convertFloatToFloat(const float inSample)
{
    return inSample;
}

double convertFloatToDouble(const float inSample)
{
    return static_cast<double>(inSample);
}

uint8_t convertDoubleToU8(const double inSample)
{
    const int16_t centered = convertToIntegral<double, int16_t>(inSample, 128.0, int16_t{-128}, int16_t{127});
    return static_cast<uint8_t>(centered + 128);
}

int16_t convertDoubleToS16(const double inSample)
{
    return convertToIntegral<double, int16_t>(inSample, 32768.0, std::numeric_limits<int16_t>::min(),
                                              std::numeric_limits<int16_t>::max());
}

int16_t convertDoubleToS16Dithered(const double inSample)
{
    static constexpr double lsb = 1.0 / 32768.0;
    thread_local std::mt19937 rng{std::random_device{}()};
    thread_local std::uniform_real_distribution<double> dist{-0.5 * lsb, 0.5 * lsb};

    return convertToIntegral<double, int16_t>(inSample + dist(rng) + dist(rng), 32768.0,
                                              std::numeric_limits<int16_t>::min(), std::numeric_limits<int16_t>::max());
}

int32_t convertDoubleToS32(const double inSample)
{
    return convertToIntegral<double, int32_t>(inSample, 2147483648.0, std::numeric_limits<int32_t>::min(),
                                              std::numeric_limits<int32_t>::max());
}

float convertDoubleToFloat(const double inSample)
{
    return static_cast<float>(inSample);
}

double convertDoubleToDouble(const double inSample)
{
    return inSample;
}

template <typename In, typename Out, typename F>
bool doConvert(const Fooyin::AudioFormat& inFmt, const std::byte* in, const Fooyin::AudioFormat& outFmt, std::byte* out,
               int frames, const ChannelMap& ch, F&& f)
{
    convert<In, Out>(inFmt, in, outFmt, out, frames, ch, std::forward<F>(f));
    return true;
}

using DispatchFn = bool (*)(const Fooyin::AudioFormat&, const std::byte*, const Fooyin::AudioFormat&, std::byte*, int,
                            const ChannelMap&, bool);

template <typename In, typename Out, auto ConvFunc>
constexpr DispatchFn cell() noexcept
{
    return +[](const Fooyin::AudioFormat& inFmt, const std::byte* in, const Fooyin::AudioFormat& outFmt, std::byte* out,
               int frames, const ChannelMap& ch, bool /*dither*/) -> bool {
        convert<In, Out>(inFmt, in, outFmt, out, frames, ch, ConvFunc);
        return true;
    };
}

template <typename In, typename Out, auto NoDither, auto Dithered>
constexpr DispatchFn cellDither() noexcept
{
    return +[](const Fooyin::AudioFormat& inFmt, const std::byte* in, const Fooyin::AudioFormat& outFmt, std::byte* out,
               int frames, const ChannelMap& ch, bool dither) -> bool {
        convert<In, Out>(inFmt, in, outFmt, out, frames, ch, dither ? Dithered : NoDither);
        return true;
    };
}

// clang-format off
enum class Lane : uint8_t { U8, S16, S32, F32, F64, Count };
// clang-format on

constexpr int laneIndex(Fooyin::SampleFormat f) noexcept
{
    using SF = Fooyin::SampleFormat;
    switch(f) {
        case SF::U8:
            return 0;
        case SF::S16:
            return 1;
        case SF::S24In32:
        case SF::S32:
            return 2;
        case SF::F32:
            return 3;
        case SF::F64:
            return 4;
        default:
            return -1;
    }
}

bool convertFormat(const Fooyin::AudioFormat& inFormat, const std::byte* input, const Fooyin::AudioFormat& outFormat,
                   std::byte* output, int frames, bool dither)
{
    const ChannelMap channels = buildChannelMap(inFormat, outFormat);

    const int i = laneIndex(inFormat.sampleFormat());
    const int o = laneIndex(outFormat.sampleFormat());
    if(i < 0 || o < 0) {
        return false;
    }

    static constexpr int N = static_cast<int>(Lane::Count);
    // clang-format off
    static constexpr std::array<std::array<DispatchFn, N>, N> convertTable = {{
        // in: U8
        { cell<uint8_t, uint8_t,  convertU8ToU8>(),
          cell<uint8_t, int16_t,  convertU8ToS16>(),
          cell<uint8_t, int32_t,  convertU8ToS32>(),
          cell<uint8_t, float,    convertU8ToFloat>(),
          cell<uint8_t, double,   convertU8ToDouble>() },
        // in: S16
        { cell<int16_t, uint8_t,  convertS16ToU8>(),
          cell<int16_t, int16_t,  convertS16ToS16>(),
          cell<int16_t, int32_t,  convertS16ToS32>(),
          cell<int16_t, float,    convertS16ToFloat>(),
          cell<int16_t, double,   convertS16ToDouble>() },
        // in: S32 lane (S32/S24In32)
        { cell<int32_t, uint8_t,  convertS32ToU8>(),
          cell<int32_t, int16_t,  convertS32ToS16>(),
          cell<int32_t, int32_t,  convertS32ToS32>(),
          cell<int32_t, float,    convertS32ToFloat>(),
          cell<int32_t, double,   convertS32ToDouble>() },
        // in: F32
        { cell<float, uint8_t,    convertFloatToU8>(),
          cellDither<float, int16_t, convertFloatToS16, convertFloatToS16Dithered>(),
          cell<float, int32_t,    convertFloatToS32>(),
          cell<float, float,      convertFloatToFloat>(),
          cell<float, double,     convertFloatToDouble>() },
        // in: F64
        { cell<double, uint8_t,   convertDoubleToU8>(),
          cellDither<double, int16_t, convertDoubleToS16, convertDoubleToS16Dithered>(),
          cell<double, int32_t,   convertDoubleToS32>(),
          cell<double, float,     convertDoubleToFloat>(),
          cell<double, double,    convertDoubleToDouble>() },
    }};
    // clang-format on

    const DispatchFn fn = convertTable[i][o];
    return fn && fn(inFormat, input, outFormat, output, frames, channels, dither);
}
} // namespace

namespace Fooyin::Audio {

AudioBuffer convert(const AudioBuffer& buffer, const AudioFormat& outputFormat, const bool dither)
{
    if(!buffer.isValid() || !outputFormat.isValid()) {
        return {};
    }

    AudioBuffer output{outputFormat, buffer.startTime()};
    output.resize(outputFormat.bytesForFrames(buffer.frameCount()));

    if(convertFormat(buffer.format(), buffer.constData().data(), outputFormat, output.data(), buffer.frameCount(),
                     dither)) {
        return output;
    }

    return {};
}

bool convert(const AudioFormat& inputFormat, const std::byte* input, const AudioFormat& outputFormat, std::byte* output,
             const int frames, const bool dither)
{
    if(!inputFormat.isValid() || !outputFormat.isValid()) {
        return false;
    }

    return convertFormat(inputFormat, input, outputFormat, output, frames, dither);
}
} // namespace Fooyin::Audio
