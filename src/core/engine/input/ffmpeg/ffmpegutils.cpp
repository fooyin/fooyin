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
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
}

#include <QDebug>

Q_LOGGING_CATEGORY(FFMPEG, "fy.ffmpeg")

namespace {
#if !OLD_CHANNEL_LAYOUT
Fooyin::AudioFormat::ChannelPosition channelPositionFromAvChannel(AVChannel channel)
{
    using P = Fooyin::AudioFormat::ChannelPosition;
    switch(channel) {
        case AV_CHAN_FRONT_LEFT:
            return P::FrontLeft;
        case AV_CHAN_FRONT_RIGHT:
            return P::FrontRight;
        case AV_CHAN_FRONT_CENTER:
            return P::FrontCenter;
        case AV_CHAN_LOW_FREQUENCY:
            return P::LFE;
        case AV_CHAN_BACK_LEFT:
            return P::BackLeft;
        case AV_CHAN_BACK_RIGHT:
            return P::BackRight;
        case AV_CHAN_FRONT_LEFT_OF_CENTER:
            return P::FrontLeftOfCenter;
        case AV_CHAN_FRONT_RIGHT_OF_CENTER:
            return P::FrontRightOfCenter;
        case AV_CHAN_BACK_CENTER:
            return P::BackCenter;
        case AV_CHAN_SIDE_LEFT:
            return P::SideLeft;
        case AV_CHAN_SIDE_RIGHT:
            return P::SideRight;
        case AV_CHAN_TOP_CENTER:
            return P::TopCenter;
        case AV_CHAN_TOP_FRONT_LEFT:
            return P::TopFrontLeft;
        case AV_CHAN_TOP_FRONT_CENTER:
            return P::TopFrontCenter;
        case AV_CHAN_TOP_FRONT_RIGHT:
            return P::TopFrontRight;
        case AV_CHAN_TOP_BACK_LEFT:
            return P::TopBackLeft;
        case AV_CHAN_TOP_BACK_CENTER:
            return P::TopBackCenter;
        case AV_CHAN_TOP_BACK_RIGHT:
            return P::TopBackRight;
#ifdef AV_CHAN_LOW_FREQUENCY_2
        case AV_CHAN_LOW_FREQUENCY_2:
            return P::LFE2;
#endif
#ifdef AV_CHAN_TOP_SIDE_LEFT
        case AV_CHAN_TOP_SIDE_LEFT:
            return P::TopSideLeft;
#endif
#ifdef AV_CHAN_TOP_SIDE_RIGHT
        case AV_CHAN_TOP_SIDE_RIGHT:
            return P::TopSideRight;
#endif
#ifdef AV_CHAN_BOTTOM_FRONT_CENTER
        case AV_CHAN_BOTTOM_FRONT_CENTER:
            return P::BottomFrontCenter;
#endif
#ifdef AV_CHAN_BOTTOM_FRONT_LEFT
        case AV_CHAN_BOTTOM_FRONT_LEFT:
            return P::BottomFrontLeft;
#endif
#ifdef AV_CHAN_BOTTOM_FRONT_RIGHT
        case AV_CHAN_BOTTOM_FRONT_RIGHT:
            return P::BottomFrontRight;
#endif
        default:
            return P::UnknownPosition;
    }
}
#else
void pushLayoutIfSet(Fooyin::AudioFormat::ChannelLayout& layout, uint64_t mask, uint64_t bit,
                     Fooyin::AudioFormat::ChannelPosition pos)
{
    if((mask & bit) != 0) {
        layout.push_back(pos);
    }
}

Fooyin::AudioFormat::ChannelLayout channelLayoutFromMask(uint64_t mask, int expectedChannels)
{
    using P = Fooyin::AudioFormat::ChannelPosition;
    Fooyin::AudioFormat::ChannelLayout layout;
    if(mask == 0 || expectedChannels <= 0) {
        return layout;
    }

    layout.reserve(static_cast<size_t>(expectedChannels));

    uint64_t knownMask = 0;
#define FY_PUSH_LAYOUT(maskBit, pos)                                                                                   \
    do {                                                                                                               \
        knownMask |= static_cast<uint64_t>(maskBit);                                                                   \
        pushLayoutIfSet(layout, mask, static_cast<uint64_t>(maskBit), pos);                                            \
    } while(false)

    FY_PUSH_LAYOUT(AV_CH_FRONT_LEFT, P::FrontLeft);
    FY_PUSH_LAYOUT(AV_CH_FRONT_RIGHT, P::FrontRight);
    FY_PUSH_LAYOUT(AV_CH_FRONT_CENTER, P::FrontCenter);
    FY_PUSH_LAYOUT(AV_CH_LOW_FREQUENCY, P::LFE);
    FY_PUSH_LAYOUT(AV_CH_BACK_LEFT, P::BackLeft);
    FY_PUSH_LAYOUT(AV_CH_BACK_RIGHT, P::BackRight);
    FY_PUSH_LAYOUT(AV_CH_FRONT_LEFT_OF_CENTER, P::FrontLeftOfCenter);
    FY_PUSH_LAYOUT(AV_CH_FRONT_RIGHT_OF_CENTER, P::FrontRightOfCenter);
    FY_PUSH_LAYOUT(AV_CH_BACK_CENTER, P::BackCenter);
    FY_PUSH_LAYOUT(AV_CH_SIDE_LEFT, P::SideLeft);
    FY_PUSH_LAYOUT(AV_CH_SIDE_RIGHT, P::SideRight);
    FY_PUSH_LAYOUT(AV_CH_TOP_CENTER, P::TopCenter);
    FY_PUSH_LAYOUT(AV_CH_TOP_FRONT_LEFT, P::TopFrontLeft);
    FY_PUSH_LAYOUT(AV_CH_TOP_FRONT_CENTER, P::TopFrontCenter);
    FY_PUSH_LAYOUT(AV_CH_TOP_FRONT_RIGHT, P::TopFrontRight);
    FY_PUSH_LAYOUT(AV_CH_TOP_BACK_LEFT, P::TopBackLeft);
    FY_PUSH_LAYOUT(AV_CH_TOP_BACK_CENTER, P::TopBackCenter);
    FY_PUSH_LAYOUT(AV_CH_TOP_BACK_RIGHT, P::TopBackRight);
#ifdef AV_CH_LOW_FREQUENCY_2
    FY_PUSH_LAYOUT(AV_CH_LOW_FREQUENCY_2, P::LFE2);
#endif
#ifdef AV_CH_TOP_SIDE_LEFT
    FY_PUSH_LAYOUT(AV_CH_TOP_SIDE_LEFT, P::TopSideLeft);
#endif
#ifdef AV_CH_TOP_SIDE_RIGHT
    FY_PUSH_LAYOUT(AV_CH_TOP_SIDE_RIGHT, P::TopSideRight);
#endif
#ifdef AV_CH_BOTTOM_FRONT_CENTER
    FY_PUSH_LAYOUT(AV_CH_BOTTOM_FRONT_CENTER, P::BottomFrontCenter);
#endif
#ifdef AV_CH_BOTTOM_FRONT_LEFT
    FY_PUSH_LAYOUT(AV_CH_BOTTOM_FRONT_LEFT, P::BottomFrontLeft);
#endif
#ifdef AV_CH_BOTTOM_FRONT_RIGHT
    FY_PUSH_LAYOUT(AV_CH_BOTTOM_FRONT_RIGHT, P::BottomFrontRight);
#endif
#undef FY_PUSH_LAYOUT

    uint64_t unknownMask = mask & ~knownMask;
    while(unknownMask != 0) {
        layout.push_back(P::UnknownPosition);
        unknownMask &= (unknownMask - 1);
    }

    while(static_cast<int>(layout.size()) < expectedChannels) {
        layout.push_back(P::UnknownPosition);
    }
    if(static_cast<int>(layout.size()) > expectedChannels) {
        layout.resize(static_cast<size_t>(expectedChannels));
    }

    return layout;
}
#endif
} // namespace

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

AudioFormat audioFormatFromCodec(AVCodecParameters* codec, AVSampleFormat ctxFormat)
{
    AudioFormat format;

    const auto sampleFmt = sampleFormat(ctxFormat, codec->bits_per_raw_sample);
    format.setSampleFormat(sampleFmt);
    format.setSampleRate(codec->sample_rate);
#if OLD_CHANNEL_LAYOUT
    format.setChannelCount(codec->channels);
    const auto layout = channelLayoutFromMask(static_cast<uint64_t>(codec->channel_layout), codec->channels);
    if(static_cast<int>(layout.size()) == codec->channels) {
        format.setChannelLayout(layout);
    }
#else
    format.setChannelCount(codec->ch_layout.nb_channels);
    AudioFormat::ChannelLayout layout;
    const unsigned channelCount = static_cast<unsigned>(codec->ch_layout.nb_channels);
    layout.reserve(static_cast<size_t>(channelCount));
    for(unsigned i = 0; i < channelCount; ++i) {
        const auto channel = av_channel_layout_channel_from_index(&codec->ch_layout, i);
        layout.push_back(channelPositionFromAvChannel(channel));
    }
    if(!layout.empty()) {
        format.setChannelLayout(layout);
    }
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
            return bps == 24 ? SampleFormat::S24In32 : SampleFormat::S32;
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
            case SampleFormat::S24In32:
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
            case(SampleFormat::S24In32):
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
