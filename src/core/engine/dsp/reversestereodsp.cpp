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

#include "reversestereodsp.h"

#include <algorithm>

namespace Fooyin {
QString ReverseStereoDsp::name() const
{
    return QStringLiteral("Reverse stereo channels");
}

QString ReverseStereoDsp::id() const
{
    return QStringLiteral("fooyin.dsp.reversestereo");
}

void ReverseStereoDsp::prepare(const AudioFormat& format)
{
    m_format = format;
}

void ReverseStereoDsp::process(ProcessingBufferList& chunks)
{
    const size_t count = chunks.count();
    for(size_t i{0}; i < count; ++i) {
        auto* buffer = chunks.item(i);
        if(!buffer || !buffer->isValid()) {
            continue;
        }

        const int channels = buffer->format().channelCount();
        if(channels != 2) {
            continue;
        }

        const int frames = buffer->frameCount();
        if(frames <= 0) {
            continue;
        }

        auto dataSpan            = buffer->data();
        const size_t sampleCount = static_cast<size_t>(frames) * static_cast<size_t>(channels);
        if(dataSpan.size() < sampleCount) {
            continue;
        }

        double* data = dataSpan.data();
        for(size_t base{0}; base + 1 < sampleCount; base += 2) {
            std::swap(data[base], data[base + 1]);
        }
    }
}
} // namespace Fooyin
