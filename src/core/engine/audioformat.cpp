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

#include <core/engine/audioformat.h>

namespace Fooyin {
AudioFormat::AudioFormat()
    : m_sampleFormat{Unknown}
    , m_channelCount{0}
    , m_sampleRate{0}
{ }

bool AudioFormat::operator==(const AudioFormat& other) const
{
    return std::tie(m_sampleFormat, m_channelCount, m_sampleRate)
        == std::tie(other.m_sampleFormat, other.m_channelCount, other.m_sampleRate);
}

bool AudioFormat::operator!=(const AudioFormat& other) const
{
    return !(*this == other);
}

bool AudioFormat::isValid() const
{
    return m_sampleRate > 0 && m_channelCount > 0 && m_sampleFormat != Unknown;
}

int AudioFormat::sampleRate() const
{
    return m_sampleRate;
}

int AudioFormat::channelCount() const
{
    return m_channelCount;
}

AudioFormat::SampleFormat AudioFormat::sampleFormat() const
{
    return m_sampleFormat;
}

void AudioFormat::setSampleRate(int sampleRate)
{
    m_sampleRate = sampleRate;
}

void AudioFormat::setChannelCount(int channelCount)
{
    m_channelCount = channelCount;
}

void AudioFormat::setSampleFormat(SampleFormat format)
{
    m_sampleFormat = format;
}

int AudioFormat::bytesForDuration(uint64_t ms) const
{
    return bytesPerFrame() * framesForDuration(ms);
}

uint64_t AudioFormat::durationForBytes(int byteCount) const
{
    if(!isValid() || byteCount <= 0) {
        return 0;
    }

    return static_cast<uint64_t>((1000LL * (byteCount / bytesPerFrame())) / sampleRate());
}

int AudioFormat::bytesForFrames(int frameCount) const
{
    return frameCount * bytesPerFrame();
}

int AudioFormat::framesForBytes(int byteCount) const
{
    const int size = bytesPerFrame();

    if(size > 0) {
        return byteCount / size;
    }

    return 0;
}

int AudioFormat::framesForDuration(uint64_t ms) const
{
    if(!isValid()) {
        return 0;
    }

    return static_cast<int>(((ms * sampleRate()) / 1000LL));
}

uint64_t AudioFormat::durationForFrames(int frameCount) const
{
    if(!isValid() || frameCount <= 0) {
        return 0;
    }

    return (frameCount * 1000LL) / sampleRate();
}

int AudioFormat::bytesPerFrame() const
{
    return bytesPerSample() * channelCount();
}

int AudioFormat::bytesPerSample() const
{
    switch(m_sampleFormat) {
        case(Unknown):
            return 0;
        case(UInt8):
            return 1;
        case(Int16):
            return 2;
        case(Int32):
        case(Float):
            return 4;
        case(Double):
            return 8;
    }

    return 0;
}
} // namespace Fooyin
