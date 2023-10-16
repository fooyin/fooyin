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

#include "ffmpegutils.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
}

#include <QDebug>

namespace Fy::Core::Engine::FFmpeg {
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

void skipSamples(AVFrame* frame, int samples)
{
    if(av_frame_make_writable(frame) < 0) {
        return;
    }

    uint8_t** fdata   = frame->data;
    const auto format = static_cast<AVSampleFormat>(frame->format);
    const int stride  = av_get_bytes_per_sample(format) * frame->ch_layout.nb_channels;
    const int size    = (frame->nb_samples - samples) * stride;

    memmove(fdata[0], fdata[0] + samples * stride, size);

    frame->nb_samples -= samples;
    if(frame->pts > 0) {
        frame->pts += samples / frame->sample_rate;
    }
}

void fillSilence(uint8_t* dst, int bytes, int format)
{
    const bool unsignedFormat = format == AV_SAMPLE_FMT_U8 || format == AV_SAMPLE_FMT_U8P;
    memset(dst, unsignedFormat ? 0x80 : 0, bytes);
}

void adjustVolumeOfSamples(uint8_t* data, AVSampleFormat format, int bytes, double volume)
{
    if(volume == 1.0) {
        return;
    }

    if(volume == 0.0) {
        fillSilence(data, bytes, format);
        return;
    }

    const int bps  = av_get_bytes_per_sample(format) * 8;
    const auto vol = static_cast<float>(volume);

    switch(bps) {
        case(8): {
            for(int i{0}; i < bytes; ++i) {
                const int8_t sample = data[i];
                data[i]             = static_cast<int8_t>(sample * vol);
            }
            break;
        }
        case(16): {
            auto* adjustedData = std::bit_cast<int16_t*>(data);
            const int count    = bytes / 2;
            for(int i{0}; i < count; ++i) {
                const int16_t sample = adjustedData[i];
                adjustedData[i]      = static_cast<int16_t>(sample * vol);
            }
            break;
        }
        case(24): {
            const int count = bytes / 3;
            for(int i{0}; i < count; ++i) {
                const int32_t sample = (static_cast<int8_t>(data[i * 3]) | static_cast<int8_t>(data[i * 3 + 1] << 8)
                                        | static_cast<int8_t>(data[i * 3 + 2] << 16));
                auto newSample       = static_cast<int32_t>(sample * vol);
                data[i * 3]          = static_cast<uint8_t>(newSample & 0x0000ff);
                data[i * 3 + 1]      = static_cast<uint8_t>((newSample & 0x00ff00) >> 8);
                data[i * 3 + 2]      = static_cast<uint8_t>((newSample & 0xff0000) >> 16);
            }
            break;
        }
        case(32): {
            const int count = bytes / 4;
            if(format == AV_SAMPLE_FMT_FLT) {
                auto* adjustedData = std::bit_cast<float*>(data);
                for(int i = 0; i < count; ++i) {
                    adjustedData[i] = adjustedData[i] * vol;
                }
            }
            else {
                auto* adjustedData = std::bit_cast<int32_t*>(data);
                for(int i = 0; i < count; ++i) {
                    const int32_t sample = adjustedData[i];
                    adjustedData[i]      = static_cast<int32_t>(sample * vol);
                }
            }
            break;
        }
        default:
            qDebug() << "Unable to adjust volume of unsupported bps: " << bps;
    }
}
} // namespace Fy::Core::Engine::FFmpeg
