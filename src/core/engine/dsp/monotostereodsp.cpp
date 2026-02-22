/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "monotostereodsp.h"

namespace Fooyin {
QString MonoToStereoDsp::name() const
{
    return QStringLiteral("Convert mono to stereo");
}

QString MonoToStereoDsp::id() const
{
    return QStringLiteral("fooyin.dsp.monotostereo");
}

AudioFormat MonoToStereoDsp::outputFormat(const AudioFormat& input) const
{
    if(!input.isValid()) {
        return {};
    }

    if(input.channelCount() == 1) {
        AudioFormat out{input};
        out.setChannelCount(2);
        return out;
    }

    return input;
}

void MonoToStereoDsp::prepare(const AudioFormat& format)
{
    m_format = format;
    m_format.setChannelCount(2);
    m_format.setChannelLayout({AudioFormat::ChannelPosition::FrontLeft, AudioFormat::ChannelPosition::FrontRight});
}

void MonoToStereoDsp::process(ProcessingBufferList& chunks)
{
    const size_t count = chunks.count();
    if(count == 0) {
        return;
    }

    ProcessingBufferList output;

    for(size_t i{0}; i < count; ++i) {
        const auto* buffer = chunks.item(i);
        if(!buffer || !buffer->isValid()) {
            continue;
        }

        const auto format = buffer->format();
        if(format.channelCount() != 1) {
            output.addChunk(*buffer);
            continue;
        }

        const int frames = buffer->frameCount();
        if(frames <= 0) {
            output.addChunk(*buffer);
            continue;
        }

        const auto inSamples = buffer->constData();
        if(inSamples.empty()) {
            output.addChunk(*buffer);
            continue;
        }

        const auto outSamples = static_cast<size_t>(frames) * 2;
        auto* outBuffer       = output.addItem(m_format, buffer->startTimeNs(), outSamples);
        auto outSpan          = outBuffer ? outBuffer->data() : std::span<double>{};

        if(!outBuffer || outSpan.size() < outSamples) {
            output.addChunk(*buffer);
            continue;
        }

        for(int frame{0}; frame < frames; ++frame) {
            const double sample  = inSamples[static_cast<size_t>(frame)];
            const size_t outBase = static_cast<size_t>(frame) * 2;
            outSpan[outBase]     = sample;
            outSpan[outBase + 1] = sample;
        }
    }

    chunks.clear();

    const size_t outCount = output.count();
    for(size_t i{0}; i < outCount; ++i) {
        const auto* buffer = output.item(i);
        if(buffer && buffer->isValid()) {
            chunks.addChunk(*buffer);
        }
    }
}
} // namespace Fooyin
