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

extern "C"
{
#include <libavutil/frame.h>
}

#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(FFMPEG, "FFmpeg")

namespace {
Fooyin::SampleFormat sampleFormat(AVSampleFormat format, int bps)
{
    switch(format) {
        case(AV_SAMPLE_FMT_NONE):
        case(AV_SAMPLE_FMT_U8):
        case(AV_SAMPLE_FMT_U8P):
            return Fooyin::SampleFormat::U8;
        case(AV_SAMPLE_FMT_S16):
        case(AV_SAMPLE_FMT_S16P):
            return Fooyin::SampleFormat::S16;
        case(AV_SAMPLE_FMT_S32):
        case(AV_SAMPLE_FMT_S32P):
            return bps == 24 ? Fooyin::SampleFormat::S24 : Fooyin::SampleFormat::S32;
        case(AV_SAMPLE_FMT_FLT):
        case(AV_SAMPLE_FMT_FLTP):
            return Fooyin::SampleFormat::F32;
        default:
            return Fooyin::SampleFormat::Unknown;
    }
}
} // namespace

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

    const auto sampleFmt = sampleFormat(static_cast<AVSampleFormat>(codec->format), codec->bits_per_raw_sample);
    format.setSampleFormat(sampleFmt);
    format.setSampleRate(codec->sample_rate);
#if OLD_CHANNEL_LAYOUT
    format.setChannelCount(codec->channels);
#else
    format.setChannelCount(codec->ch_layout.nb_channels);
#endif

    return format;
}
} // namespace Fooyin::Utils
