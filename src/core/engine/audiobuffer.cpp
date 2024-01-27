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

#include <core/engine/audiobuffer.h>

#include <utility>

namespace Fooyin {
struct AudioBuffer::Private : public QSharedData
{
public:
    QByteArray data;
    AudioFormat format;
    uint64_t startTime;

    Private(QByteArray data, AudioFormat format, uint64_t startTime)
        : data{std::move(data)}
        , format{format}
        , startTime{startTime}
    { }
};

AudioBuffer::AudioBuffer() = default;

AudioBuffer::AudioBuffer(QByteArray data, AudioFormat format, uint64_t startTime)
    : p{new Private(std::move(data), format, startTime)}
{ }

AudioBuffer::~AudioBuffer() = default;

AudioBuffer::AudioBuffer(AudioBuffer&& other) noexcept = default;
AudioBuffer::AudioBuffer(const AudioBuffer& other)     = default;

AudioBuffer& AudioBuffer::operator=(const AudioBuffer& other) = default;

bool AudioBuffer::isValid() const
{
    return !!p;
}

void AudioBuffer::detach()
{
    p = new Private(*p);
}

AudioFormat AudioBuffer::format() const
{
    return !!p ? p->format : AudioFormat{};
}

int AudioBuffer::frameCount() const
{
    return !!p ? p->format.framesForBytes(byteCount()) : 0;
}

int AudioBuffer::sampleCount() const
{
    return frameCount() * format().channelCount();
}

int AudioBuffer::byteCount() const
{
    return !!p ? static_cast<int>(p->data.size()) : 0;
}

uint64_t AudioBuffer::startTime() const
{
    return !!p ? p->startTime : -1;
}

uint64_t AudioBuffer::duration() const
{
    return format().durationForFrames(frameCount());
}

uint8_t* AudioBuffer::constData() const
{
    return std::bit_cast<uint8_t*>(p->data.constData());
}

uint8_t* AudioBuffer::data()
{
    return std::bit_cast<uint8_t*>(p->data.data());
}
} // namespace Fooyin
