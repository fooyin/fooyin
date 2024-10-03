/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/engine/audiobuffer.h>

#if defined(__GNUG__)
#pragma GCC diagnostic ignored "-Wold-style-cast"
#elif defined(__clang__)
#pragma clang diagnostic ignored "-Wold-style-cast"
#endif

extern "C"
{
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/frame.h>
}

#include <QDebug>

Q_LOGGING_CATEGORY(FFMPEG, "fy.ffmpeg")

void IOContextDeleter::operator()(AVIOContext* context) const
{
    if(context) {
        if(context->buffer) {
            av_freep(static_cast<void*>(&context->buffer));
        }
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 80, 100)
        avio_context_free(&context);
#else
        av_free(context);
#endif
    }
}

void FormatContextDeleter::operator()(AVFormatContext* context) const
{
    if(context) {
        avformat_close_input(&context);
        avformat_free_context(context);
    }
}

void FrameDeleter::operator()(AVFrame* frame) const
{
    if(frame != nullptr) {
        av_frame_free(&frame);
    }
}

void PacketDeleter::operator()(AVPacket* packet) const
{
    if(packet) {
        av_packet_free(&packet);
    }
}

namespace Fooyin::Utils {
void printError(int error)
{
    char errStr[1024];
    av_strerror(error, errStr, 1024);
    qCWarning(FFMPEG) << errStr;
}

void printError(const QString& error)
{
    qCWarning(FFMPEG) << error;
}

AudioFormat audioFormatFromCodec(AVCodecParameters* codec)
{
    AudioFormat format;

    const auto avFmt     = static_cast<AVSampleFormat>(codec->format);
    const auto sampleFmt = sampleFormat(avFmt, codec->bits_per_raw_sample);
    format.setSampleFormat(sampleFmt);
    format.setSampleFormatIsPlanar(av_sample_fmt_is_planar(avFmt) == 1);
    format.setSampleRate(codec->sample_rate);
#if OLD_CHANNEL_LAYOUT
    format.setChannelCount(codec->channels);
#else
    format.setChannelCount(codec->ch_layout.nb_channels);
#endif

    return format;
}

SampleFormat sampleFormat(AVSampleFormat format, int bps)
{
    switch(format) {
        case(AV_SAMPLE_FMT_NONE):
        case(AV_SAMPLE_FMT_U8):
        case(AV_SAMPLE_FMT_U8P):
            return SampleFormat::U8;
        case(AV_SAMPLE_FMT_S16):
        case(AV_SAMPLE_FMT_S16P):
            return SampleFormat::S16;
        case(AV_SAMPLE_FMT_S32):
        case(AV_SAMPLE_FMT_S32P):
            return bps == 24 ? SampleFormat::S24 : SampleFormat::S32;
        case(AV_SAMPLE_FMT_FLT):
        case(AV_SAMPLE_FMT_FLTP):
            return SampleFormat::F32;
        case(AV_SAMPLE_FMT_DBL):
        case(AV_SAMPLE_FMT_DBLP):
            return SampleFormat::F64;
        default:
            return SampleFormat::Unknown;
    }
}

AVSampleFormat sampleFormat(SampleFormat format, bool planar)
{
    if(planar) {
        switch(format) {
            case SampleFormat::U8:
                return AV_SAMPLE_FMT_U8P;
            case SampleFormat::S16:
                return AV_SAMPLE_FMT_S16P;
            case SampleFormat::S24:
            case SampleFormat::S32:
                return AV_SAMPLE_FMT_S32P;
            case SampleFormat::F32:
                return AV_SAMPLE_FMT_FLTP;
            case SampleFormat::F64:
                return AV_SAMPLE_FMT_DBLP;
            case SampleFormat::Unknown:
                break;
        }
    }
    else {
        switch(format) {
            case(SampleFormat::U8):
                return AV_SAMPLE_FMT_U8;
            case(SampleFormat::S16):
                return AV_SAMPLE_FMT_S16;
            case(SampleFormat::S24):
            case(SampleFormat::S32):
                return AV_SAMPLE_FMT_S32;
            case(SampleFormat::F32):
                return AV_SAMPLE_FMT_FLT;
            case(SampleFormat::F64):
                return AV_SAMPLE_FMT_DBL;
            case(SampleFormat::Unknown):
                break;
        }
    }
    return AV_SAMPLE_FMT_NONE;
}

Stream findAudioStream(AVFormatContext* context)
{
    const auto count = static_cast<int>(context->nb_streams);

    for(int i{0}; i < count; ++i) {
        AVStream* avStream = context->streams[i];
        const auto type    = avStream->codecpar->codec_type;

        if(type == AVMEDIA_TYPE_AUDIO) {
            return Stream{avStream};
        }
    }

    return {};
}
} // namespace Fooyin::Utils
