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

#include "ffmpegaudiobuffer.h"

#include <utility>

namespace Fooyin {
class FFmpegAudioBufferPrivate : public QSharedData
{
public:
    QByteArray data;
    AudioFormat format;
    uint64_t startTime;

    FFmpegAudioBufferPrivate(QByteArray data, AudioFormat format, uint64_t startTime)
        : data{std::move(data)}
        , format{format}
        , startTime{startTime}
    { }
};

FFmpegAudioBuffer::FFmpegAudioBuffer() = default;

FFmpegAudioBuffer::FFmpegAudioBuffer(QByteArray data, AudioFormat format, uint64_t startTime)
    : p{new FFmpegAudioBufferPrivate(std::move(data), format, startTime)}
{ }

FFmpegAudioBuffer::~FFmpegAudioBuffer() = default;

FFmpegAudioBuffer::FFmpegAudioBuffer(FFmpegAudioBuffer&& other) noexcept = default;
FFmpegAudioBuffer::FFmpegAudioBuffer(const FFmpegAudioBuffer& other)     = default;

FFmpegAudioBuffer& FFmpegAudioBuffer::operator=(const FFmpegAudioBuffer& other) = default;

bool FFmpegAudioBuffer::isValid() const
{
    return !!p;
}

void FFmpegAudioBuffer::detach()
{
    p = new FFmpegAudioBufferPrivate(*p);
}

AudioFormat FFmpegAudioBuffer::format() const
{
    return !!p ? p->format : AudioFormat{};
}

int FFmpegAudioBuffer::frameCount() const
{
    return !!p ? p->format.framesForBytes(byteCount()) : 0;
}

int FFmpegAudioBuffer::sampleCount() const
{
    return frameCount() * format().channelCount();
}

int FFmpegAudioBuffer::byteCount() const
{
    return !!p ? static_cast<int>(p->data.size()) : 0;
}

uint64_t FFmpegAudioBuffer::startTime() const
{
    return !!p ? p->startTime : -1;
}

uint64_t FFmpegAudioBuffer::duration() const
{
    return format().durationForFrames(frameCount());
}

uint8_t* FFmpegAudioBuffer::constData() const
{
    return std::bit_cast<uint8_t*>(p->data.constData());
}

uint8_t* FFmpegAudioBuffer::data()
{
    return std::bit_cast<uint8_t*>(p->data.data());
}
} // namespace Fooyin
