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

#pragma once

#include "core/engine/audiooutput.h"

extern "C"
{
#include <libavutil/samplefmt.h>
}
namespace Fy::Core::Engine::FFmpeg {
int bytesPerSample(AVSampleFormat format) noexcept
{
    return av_get_bytes_per_sample(format);
}

int bytesPerFrame(AVSampleFormat format, int channelCount)
{
    return bytesPerSample(format) * channelCount;
}

int framesForDuration(int sampleRate, uint64_t milliseconds)
{
    return int((milliseconds * sampleRate) / 1000000LL);
}

uint64_t durationForBytes(OutputContext context, uint32_t bytes)
{
    return uint64_t(1000LL * (bytes / bytesPerFrame(context.format, context.channelLayout.nb_channels)))
         / context.sampleRate;
}

int bytesForDuration(OutputContext context, uint64_t milliseconds)
{
    return bytesPerFrame(context.format, context.channelLayout.nb_channels)
         * framesForDuration(context.sampleRate, milliseconds);
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
} // namespace Fy::Core::Engine::FFmpeg
