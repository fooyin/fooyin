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

#include "ffmpegresampler.h"

#include "ffmpegutils.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

namespace Fooyin {
FFmpegResampler::FFmpegResampler(const AudioFormat& inFormat, const AudioFormat& outFormat, uint64_t startTime)
    : m_inFormat{inFormat}
    , m_outFormat{outFormat}
    , m_startTime{startTime}
    , m_samplesConverted{0}
{
    if(!inFormat.isValid() || !outFormat.isValid()) {
        return;
    }

    SwrContext* context{nullptr};
#if OLD_CHANNEL_LAYOUT
    context = swr_alloc_set_opts(nullptr, av_get_default_channel_layout(outFormat.channelCount()),
                                 Utils::sampleFormat(outFormat.sampleFormat()), outFormat.sampleRate(),
                                 av_get_default_channel_layout(inFormat.channelCount()),
                                 Utils::sampleFormat(inFormat.sampleFormat()), inFormat.sampleRate(), 0, nullptr);
#else
    AVChannelLayout inLayout;
    av_channel_layout_default(&inLayout, inFormat.channelCount());
    AVChannelLayout outLayout;
    av_channel_layout_default(&outLayout, outFormat.channelCount());

    swr_alloc_set_opts2(&context, &outLayout, Utils::sampleFormat(outFormat.sampleFormat()), outFormat.sampleRate(),
                        &inLayout, Utils::sampleFormat(inFormat.sampleFormat()), inFormat.sampleRate(), 0, nullptr);
#endif
    if(swr_init(context) < 0) {
        return;
    }
    m_context = {context, SwrContextDeleter()};
}

bool FFmpegResampler::canResample() const
{
    return m_context != nullptr;
}

AudioBuffer FFmpegResampler::resample(const AudioBuffer& buffer)
{
    AudioBuffer outBuffer{m_outFormat, buffer.startTime()};

    const AudioFormat outFormat = outBuffer.format();

    const int outCount = swr_get_out_samples(m_context.get(), buffer.frameCount());
    outBuffer.resize(outFormat.bytesForFrames(outCount));

    const auto* in = std::bit_cast<const uint8_t*>(buffer.data());
    auto* out      = std::bit_cast<uint8_t*>(outBuffer.data());

    const int outSamples = swr_convert(m_context.get(), &out, outCount, &in, buffer.frameCount());
    outBuffer.resize(outFormat.bytesForFrames(outSamples));

    const uint64_t startTime = outFormat.durationForFrames(static_cast<int>(m_samplesConverted)) + m_startTime;
    outBuffer.setStartTime(startTime);

    m_samplesConverted += outSamples;

    return outBuffer;
}
} // namespace Fooyin
