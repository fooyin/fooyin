/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include "ffmpegutils.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
}

#include <QDebug>

namespace {
Fooyin::AudioFormat::SampleFormat sampleFormat(AVSampleFormat format)
{
    switch(format) {
        case(AV_SAMPLE_FMT_NONE):
        case(AV_SAMPLE_FMT_U8):
        case(AV_SAMPLE_FMT_U8P):
            return Fooyin::AudioFormat::UInt8;
        case(AV_SAMPLE_FMT_S16):
        case(AV_SAMPLE_FMT_S16P):
            return Fooyin::AudioFormat::Int16;
        case(AV_SAMPLE_FMT_S32):
        case(AV_SAMPLE_FMT_S32P):
            return Fooyin::AudioFormat::Int32;
        case(AV_SAMPLE_FMT_FLT):
        case(AV_SAMPLE_FMT_FLTP):
            return Fooyin::AudioFormat::Float;
        case(AV_SAMPLE_FMT_DBL):
        case(AV_SAMPLE_FMT_DBLP):
        case(AV_SAMPLE_FMT_S64):
        case(AV_SAMPLE_FMT_S64P):
            return Fooyin::AudioFormat::Double;
        default:
            return Fooyin::AudioFormat::Unknown;
    }
}
} // namespace

namespace Fooyin::Utils {
void printError(int error)
{
    char errStr[1024];
    av_strerror(error, errStr, 1024);
    qDebug() << "[FFmpeg] " << errStr;
}

void printError(const QString& error)
{
    qDebug() << "[FFmpeg] " << error;
}

AVSampleFormat interleaveFormat(AVSampleFormat planarFormat)
{
    switch(planarFormat) {
        case(AV_SAMPLE_FMT_U8P):
            return AV_SAMPLE_FMT_U8;
        case(AV_SAMPLE_FMT_S16P):
            return AV_SAMPLE_FMT_S16;
        case(AV_SAMPLE_FMT_S32P):
            return AV_SAMPLE_FMT_S32;
        case(AV_SAMPLE_FMT_FLTP):
            return AV_SAMPLE_FMT_FLT;
        case(AV_SAMPLE_FMT_DBLP):
            return AV_SAMPLE_FMT_DBL;
        case(AV_SAMPLE_FMT_S64P):
            return AV_SAMPLE_FMT_S64;
        case(AV_SAMPLE_FMT_NONE):
        case(AV_SAMPLE_FMT_U8):
        case(AV_SAMPLE_FMT_S16):
        case(AV_SAMPLE_FMT_S32):
        case(AV_SAMPLE_FMT_FLT):
        case(AV_SAMPLE_FMT_DBL):
        case(AV_SAMPLE_FMT_S64):
        case(AV_SAMPLE_FMT_NB):
        default:
            return planarFormat;
    }
}

AudioFormat audioFormatFromCodec(AVCodecParameters* codec)
{
    AudioFormat format;

    const auto sampleFmt = sampleFormat(static_cast<AVSampleFormat>(codec->format));
    format.setSampleFormat(sampleFmt);
    format.setSampleRate(codec->sample_rate);
    format.setChannelCount(codec->ch_layout.nb_channels);

    return format;
}

void fillSilence(uint8_t* dst, int bytes, const AudioFormat& format)
{
    const bool unsignedFormat = format.sampleFormat() == AudioFormat::UInt8;
    memset(dst, unsignedFormat ? 0x80 : 0, bytes);
}

void adjustVolumeOfSamples(uint8_t* data, const AudioFormat& format, int bytes, double volume)
{
    if(volume == 1.0) {
        return;
    }

    if(volume == 0.0) {
        fillSilence(data, bytes, format);
        return;
    }

    const int bps  = format.bytesPerSample() * 8;
    const auto vol = static_cast<float>(volume);

    switch(bps) {
        case(8): {
            for(int i{0}; i < bytes; ++i) {
                const auto sample = std::bit_cast<int8_t>(data[i]);
                data[i]           = static_cast<int8_t>(static_cast<float>(sample) * vol);
            }
            break;
        }
        case(16): {
            auto* adjustedData = std::bit_cast<int16_t*>(data);
            const int count    = bytes / 2;
            for(int i{0}; i < count; ++i) {
                const int16_t sample = adjustedData[i];
                adjustedData[i]      = static_cast<int16_t>(static_cast<float>(sample) * vol);
            }
            break;
        }
        case(24): {
            const int count = bytes / 3;
            for(int i{0}; i < count; ++i) {
                const int32_t sample              = (static_cast<int8_t>(data[static_cast<int>(i * 3)])
                                        | static_cast<int8_t>(data[static_cast<int>(i * 3 + 1)] << 8)
                                        | static_cast<int8_t>(data[static_cast<int>(i * 3 + 2)] << 16));
                auto newSample                    = static_cast<int32_t>(static_cast<float>(sample) * vol);
                data[static_cast<int>(i * 3)]     = static_cast<uint8_t>(newSample & 0x0000FF);
                data[static_cast<int>(i * 3 + 1)] = static_cast<uint8_t>((newSample & 0x00FF00) >> 8);
                data[static_cast<int>(i * 3 + 2)] = static_cast<uint8_t>((newSample & 0xFF0000) >> 16);
            }
            break;
        }
        case(32): {
            const int count = bytes / 4;
            if(format.sampleFormat() == AudioFormat::Float) {
                auto* adjustedData = std::bit_cast<float*>(data);
                for(int i = 0; i < count; ++i) {
                    adjustedData[i] = adjustedData[i] * vol;
                }
            }
            else {
                auto* adjustedData = std::bit_cast<int32_t*>(data);
                for(int i = 0; i < count; ++i) {
                    const int32_t sample = adjustedData[i];
                    adjustedData[i]      = static_cast<int32_t>(static_cast<float>(sample) * vol);
                }
            }
            break;
        }
        default:
            qDebug() << "Unable to adjust volume of unsupported bps: " << bps;
    }
}
} // namespace Fooyin::Utils
