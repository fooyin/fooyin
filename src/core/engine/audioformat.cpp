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

#include <tuple>

namespace Fooyin {
AudioFormat::AudioFormat()
    : AudioFormat{SampleFormat::Unknown, 0, 0}
{ }

AudioFormat::AudioFormat(SampleFormat format, int sampleRate, int channelCount)
    : m_sampleFormat{format}
    , m_channelCount{channelCount}
    , m_sampleRate{sampleRate}
{ }

bool AudioFormat::operator==(const AudioFormat& other) const noexcept
{
    return std::tie(m_sampleFormat, m_channelCount, m_sampleRate)
        == std::tie(other.m_sampleFormat, other.m_channelCount, other.m_sampleRate);
}

bool AudioFormat::operator!=(const AudioFormat& other) const noexcept
{
    return !(*this == other);
}

bool AudioFormat::isValid() const
{
    return m_sampleRate > 0 && m_channelCount > 0 && m_sampleFormat != SampleFormat::Unknown;
}

int AudioFormat::sampleRate() const
{
    return m_sampleRate;
}

int AudioFormat::channelCount() const
{
    return m_channelCount;
}

SampleFormat AudioFormat::sampleFormat() const
{
    return m_sampleFormat;
}

bool AudioFormat::sampleFormatIsPlanar() const
{
    return m_sampleFormatPlanar;
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

void AudioFormat::setSampleFormatIsPlanar(bool planar)
{
    m_sampleFormatPlanar = planar;
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

    return static_cast<uint64_t>(1000 * (byteCount / bytesPerFrame()) / sampleRate());
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

    return static_cast<int>(ms * sampleRate() / 1000);
}

uint64_t AudioFormat::durationForFrames(int frameCount) const
{
    if(!isValid() || frameCount <= 0) {
        return 0;
    }

    return static_cast<uint64_t>(frameCount) * 1000 / sampleRate();
}

int AudioFormat::bytesPerFrame() const
{
    return bytesPerSample() * channelCount();
}

int AudioFormat::bytesPerSample() const
{
    switch(m_sampleFormat) {
        case(SampleFormat::U8):
            return 1;
        case(SampleFormat::S16):
            return 2;
        case(SampleFormat::S24):
            // Stored in low three bytes of 32bit int
        case(SampleFormat::S32):
        case(SampleFormat::F32):
            return 4;
        case(SampleFormat::F64):
            return 8;
        case(SampleFormat::Unknown):
        default:
            return 0;
    }
}

int AudioFormat::bitsPerSample() const
{
    return bytesPerSample() * 8;
}

QString AudioFormat::prettyFormat() const
{
    switch(m_sampleFormat) {
        case(SampleFormat::U8):
            return QStringLiteral("8 bit (unsigned)");
        case(SampleFormat::S16):
            return QStringLiteral("16 bit (signed)");
        case(SampleFormat::S24):
            return QStringLiteral("24 bit (signed)");
        case(SampleFormat::S32):
            return QStringLiteral("32 bit (signed)");
        case(SampleFormat::F32):
            return QStringLiteral("32 bit (float)");
        case(SampleFormat::F64):
            return QStringLiteral("64 bit (float)");
        case(SampleFormat::Unknown):
        default:
            return QStringLiteral("Unknown format");
    }
}
} // namespace Fooyin
