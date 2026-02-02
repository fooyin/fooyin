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

#include <core/engine/dsp/processingbuffer.h>
#include <utils/timeconstants.h>

#include <cassert>
#include <cstring>
#include <limits>

namespace {
int clampSizeToInt(size_t value)
{
    constexpr auto kIntMax = static_cast<size_t>(std::numeric_limits<int>::max());
    return (value > kIntMax) ? std::numeric_limits<int>::max() : static_cast<int>(value);
}

} // namespace

namespace Fooyin {

ProcessingBuffer::ProcessingBuffer()
    : ProcessingBuffer{AudioFormat{}, 0}
{ }

ProcessingBuffer::ProcessingBuffer(AudioFormat format, uint64_t startTimeNs)
    : m_format{format}
    , m_startTimeNs{startTimeNs}
{ }

ProcessingBuffer::ProcessingBuffer(std::span<const double> samples, AudioFormat format, uint64_t startTimeNs)
    : m_format{format}
    , m_startTimeNs{startTimeNs}
{
    m_samples.assign(samples.begin(), samples.end());
}

bool ProcessingBuffer::isValid() const
{
    return m_format.isValid();
}

void ProcessingBuffer::reset()
{
    m_samples.clear();
    m_format      = {};
    m_startTimeNs = 0;
}

AudioFormat ProcessingBuffer::format() const
{
    return m_format;
}

int ProcessingBuffer::frameCount() const
{
    const int channels = m_format.channelCount();
    if(channels <= 0) {
        return 0;
    }

    const auto channelCount = static_cast<size_t>(channels);

    return clampSizeToInt(m_samples.size() / channelCount);
}

int ProcessingBuffer::sampleCount() const
{
    return clampSizeToInt(m_samples.size());
}

uint64_t ProcessingBuffer::startTimeNs() const
{
    return m_startTimeNs;
}

uint64_t ProcessingBuffer::endTimeNs() const
{
    return m_startTimeNs + durationNs();
}

uint64_t ProcessingBuffer::durationNs() const
{
    if(!m_format.isValid() || m_format.sampleRate() <= 0) {
        return 0;
    }

    return static_cast<uint64_t>(std::max(0, frameCount())) * Time::NsPerSecond
         / static_cast<uint64_t>(m_format.sampleRate());
}

void ProcessingBuffer::setStartTimeNs(uint64_t startTimeNs)
{
    m_startTimeNs = startTimeNs;
}

void ProcessingBuffer::reserveSamples(size_t samples)
{
    m_samples.reserve(samples);
}

void ProcessingBuffer::resizeSamples(size_t samples)
{
    m_samples.resize(samples);
}

void ProcessingBuffer::clear()
{
    m_samples.clear();
}

void ProcessingBuffer::append(std::span<const double> samples)
{
    m_samples.insert(m_samples.end(), samples.begin(), samples.end());

    const int channels = m_format.channelCount();
    if(channels > 0) {
        assert(m_samples.size() % static_cast<size_t>(channels) == 0);
    }
}

std::span<const double> ProcessingBuffer::constData() const
{
    return m_samples;
}

std::span<double> ProcessingBuffer::data()
{
    return m_samples;
}

const std::vector<double>& ProcessingBuffer::samples() const
{
    return m_samples;
}

std::vector<double>& ProcessingBuffer::samples()
{
    return m_samples;
}

AudioBuffer ProcessingBuffer::toAudioBuffer() const
{
    AudioBuffer buffer{m_format, m_startTimeNs / Time::NsPerMs};

    const size_t bytes = m_samples.size() * sizeof(double);
    buffer.resize(bytes);

    if(bytes > 0) {
        std::memcpy(buffer.data(), m_samples.data(), bytes);
    }

    return buffer;
}
} // namespace Fooyin
