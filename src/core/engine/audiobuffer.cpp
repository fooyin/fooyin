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

#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

namespace {
int clampSizeToInt(size_t value)
{
    constexpr auto IntMax = static_cast<size_t>(std::numeric_limits<int>::max());
    return (value > IntMax) ? std::numeric_limits<int>::max() : static_cast<int>(value);
}
} // namespace

namespace Fooyin {
AudioBuffer::AudioBuffer()
    : m_startTime{0}
{ }

AudioBuffer::AudioBuffer(std::span<const std::byte> data, const AudioFormat& format, uint64_t startTime)
    : m_buffer{data.begin(), data.end()}
    , m_format{format}
    , m_startTime{startTime}
{ }

AudioBuffer::AudioBuffer(const AudioFormat& format, uint64_t startTime)
    : AudioBuffer{{}, std::move(format), startTime}
{ }

AudioBuffer::AudioBuffer(const uint8_t* data, size_t size, const AudioFormat& format, uint64_t startTime)
    : m_buffer(size)
    , m_format{format}
    , m_startTime{startTime}
{
    if(data && size > 0) {
        std::memmove(m_buffer.data(), data, size);
    }
}

AudioBuffer::~AudioBuffer() = default;

AudioBuffer::AudioBuffer(const AudioBuffer& other)                = default;
AudioBuffer& AudioBuffer::operator=(const AudioBuffer& other)     = default;
AudioBuffer::AudioBuffer(AudioBuffer&& other) noexcept            = default;
AudioBuffer& AudioBuffer::operator=(AudioBuffer&& other) noexcept = default;

void AudioBuffer::reserve(size_t size)
{
    if(isValid()) {
        m_buffer.reserve(size);
    }
}

void AudioBuffer::resize(size_t size)
{
    if(isValid()) {
        m_buffer.resize(size);
    }
}

void AudioBuffer::append(const std::span<const std::byte> data)
{
    if(isValid()) {
        append(data.data(), data.size());
    }
}

void AudioBuffer::append(const std::byte* data, size_t size)
{
    if(!isValid() || !data || size == 0) {
        return;
    }

    const size_t index = m_buffer.size();
    m_buffer.resize(index + size);
    std::memcpy(m_buffer.data() + index, data, size);
}

void AudioBuffer::erase(size_t size)
{
    if(!isValid() || size == 0) {
        return;
    }

    const size_t bytesToErase = std::min(size, m_buffer.size());
    m_buffer.erase(m_buffer.begin(), m_buffer.begin() + static_cast<ptrdiff_t>(bytesToErase));
}

void AudioBuffer::clear()
{
    if(isValid()) {
        m_buffer.clear();
    }
}

void AudioBuffer::reset()
{
    m_buffer.clear();
    m_format    = {};
    m_startTime = 0;
}

bool AudioBuffer::isValid() const
{
    if(!m_format.isValid()) {
        return false;
    }

    const int bytesPerFrame = m_format.bytesPerFrame();
    if(bytesPerFrame <= 0) {
        return false;
    }

    return (m_buffer.size() % static_cast<size_t>(bytesPerFrame)) == 0;
}

AudioFormat AudioBuffer::format() const
{
    if(isValid()) {
        return m_format;
    }
    return {};
}

int AudioBuffer::frameCount() const
{
    return isValid() ? m_format.framesForBytes(byteCount()) : 0;
}

int AudioBuffer::sampleCount() const
{
    return frameCount() * format().channelCount();
}

int AudioBuffer::byteCount() const
{
    return isValid() ? clampSizeToInt(m_buffer.size()) : 0;
}

uint64_t AudioBuffer::startTime() const
{
    return isValid() ? m_startTime : static_cast<uint64_t>(-1);
}

uint64_t AudioBuffer::endTime() const
{
    return isValid() ? m_startTime + duration() : static_cast<uint64_t>(-1);
}

uint64_t AudioBuffer::duration() const
{
    return format().durationForFrames(frameCount());
}

std::span<const std::byte> AudioBuffer::constData() const
{
    if(isValid()) {
        return m_buffer;
    }
    return {};
}

const std::byte* AudioBuffer::data() const
{
    if(isValid()) {
        return m_buffer.data();
    }
    return {};
}

std::byte* AudioBuffer::data()
{
    if(isValid()) {
        return m_buffer.data();
    }
    return {};
}

void AudioBuffer::setStartTime(uint64_t startTime)
{
    if(isValid()) {
        m_startTime = startTime;
    }
}

void AudioBuffer::fillSilence()
{
    if(!isValid()) {
        return;
    }

    const bool unsignedFormat = m_format.sampleFormat() == SampleFormat::U8;
    std::ranges::fill(m_buffer, unsignedFormat ? std::byte{0x80} : std::byte{0});
}
} // namespace Fooyin
