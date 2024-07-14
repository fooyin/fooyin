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

#include <core/engine/audiobuffer.h>

#include <QDebug>

namespace Fooyin {
class AudioBufferPrivate : public QSharedData
{
public:
    AudioBufferPrivate(std::span<const std::byte> data, AudioFormat format, uint64_t startTime)
        : m_format{format}
        , m_startTime{startTime}
    {
        m_buffer.assign(data.begin(), data.end());
    }

    AudioBufferPrivate(const uint8_t* data, size_t size, AudioFormat format, uint64_t startTime)
        : m_format{format}
        , m_startTime{startTime}
    {
        m_buffer.resize(size);
        std::memmove(m_buffer.data(), data, size);
    }

    void fillSilence()
    {
        const bool unsignedFormat = m_format.sampleFormat() == SampleFormat::U8;
        std::ranges::fill(m_buffer, unsignedFormat ? std::byte{0x80} : std::byte{0});
    }

    void fillRemainingWithSilence()
    {
        const bool unsignedFormat = m_format.sampleFormat() == SampleFormat::U8;
        std::fill(m_buffer.begin() + static_cast<long>(m_buffer.size()),
                  m_buffer.begin() + static_cast<long>(m_buffer.capacity()),
                  unsignedFormat ? std::byte{0x80} : std::byte{0});
    }

    template <typename T>
    void scale(const double volume)
    {
        const auto bytes = static_cast<int>(m_buffer.size());
        const int bps    = m_format.bytesPerSample();

        for(int i{0}; i < bytes; i += bps) {
            T sample;
            std::memcpy(&sample, m_buffer.data() + i, bps);
            sample *= volume;
            std::memcpy(m_buffer.data() + i, &sample, bps);
        }
    }

    std::vector<std::byte> m_buffer;
    AudioFormat m_format;
    uint64_t m_startTime;
};

AudioBuffer::AudioBuffer() = default;

AudioBuffer::AudioBuffer(std::span<const std::byte> data, AudioFormat format, uint64_t startTime)
    : p{new AudioBufferPrivate(data, format, startTime)}
{ }

AudioBuffer::AudioBuffer(AudioFormat format, uint64_t startTime)
    : AudioBuffer{{}, format, startTime}
{ }

AudioBuffer::AudioBuffer(const uint8_t* data, size_t size, AudioFormat format, uint64_t startTime)
    : p{new AudioBufferPrivate(data, size, format, startTime)}
{ }

AudioBuffer::~AudioBuffer() = default;

AudioBuffer::AudioBuffer(const AudioBuffer& other)            = default;
AudioBuffer& AudioBuffer::operator=(const AudioBuffer& other) = default;
AudioBuffer::AudioBuffer(AudioBuffer&& other) noexcept        = default;

void AudioBuffer::reserve(size_t size)
{
    if(isValid()) {
        p->m_buffer.reserve(size);
    }
}

void AudioBuffer::resize(size_t size)
{
    if(isValid()) {
        p->m_buffer.resize(size);
    }
}

void AudioBuffer::append(std::span<const std::byte> data)
{
    if(isValid()) {
        append(data.data(), data.size());
    }
}

void AudioBuffer::append(const std::byte* data, size_t size)
{
    if(isValid()) {
        const size_t index = p->m_buffer.size();
        p->m_buffer.resize(index + size);
        std::memcpy(p->m_buffer.data() + index, data, size);
    }
}

void AudioBuffer::erase(size_t size)
{
    if(isValid()) {
        p->m_buffer.erase(p->m_buffer.begin(), p->m_buffer.begin() + static_cast<long>(size));
    }
}

void AudioBuffer::clear()
{
    if(isValid()) {
        p->m_buffer.clear();
    }
}

void AudioBuffer::reset()
{
    if(isValid()) {
        p.reset();
    }
}

bool AudioBuffer::isValid() const
{
    return !!p;
}

void AudioBuffer::detach()
{
    if(isValid()) {
        p = new AudioBufferPrivate(*p);
    }
}

AudioFormat AudioBuffer::format() const
{
    if(isValid()) {
        return p->m_format;
    }
    return {};
}

int AudioBuffer::frameCount() const
{
    return isValid() ? p->m_format.framesForBytes(byteCount()) : 0;
}

int AudioBuffer::sampleCount() const
{
    return frameCount() * format().channelCount();
}

int AudioBuffer::byteCount() const
{
    return isValid() ? static_cast<int>(p->m_buffer.size()) : 0;
}

uint64_t AudioBuffer::startTime() const
{
    return isValid() ? p->m_startTime : -1;
}

uint64_t AudioBuffer::endTime() const
{
    return isValid() ? p->m_startTime + duration() : -1;
}

uint64_t AudioBuffer::duration() const
{
    return format().durationForFrames(frameCount());
}

std::span<const std::byte> AudioBuffer::constData() const
{
    if(isValid()) {
        return p->m_buffer;
    }
    return {};
}

const std::byte* AudioBuffer::data() const
{
    if(isValid()) {
        return p->m_buffer.data();
    }
    return {};
}

std::byte* AudioBuffer::data()
{
    if(isValid()) {
        return p->m_buffer.data();
    }
    return {};
}

void AudioBuffer::fillSilence()
{
    if(isValid()) {
        p->fillSilence();
    }
}

void AudioBuffer::fillRemainingWithSilence()
{
    if(isValid()) {
        p->fillRemainingWithSilence();
    }
}

void AudioBuffer::scale(double volume)
{
    if(!isValid() || volume == 1.0) {
        return;
    }

    if(volume == 0.0) {
        fillSilence();
        return;
    }

    switch(format().sampleFormat()) {
        case(SampleFormat::U8):
            p->scale<uint8_t>(volume);
            break;
        case(SampleFormat::S16):
            p->scale<int16_t>(volume);
            break;
        case(SampleFormat::S24):
        case(SampleFormat::S32):
            p->scale<int32_t>(volume);
            break;
        case(SampleFormat::F32):
            p->scale<float>(volume);
            break;
        case(SampleFormat::Unknown):
        default:
            qDebug() << "Unable to scale samples of unsupported format";
    }
}
} // namespace Fooyin
