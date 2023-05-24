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
#include <libavutil/frame.h>
}

namespace Fy::Core::Engine::FFmpeg {
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
} // namespace Fy::Core::Engine::FFmpeg
