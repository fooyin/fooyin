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

#pragma once

#include "fycore_export.h"

#include <cstdint>

namespace Fooyin {
// TODO: Handle channel layout
class FYCORE_EXPORT AudioFormat
{
public:
    enum SampleFormat : uint16_t
    {
        Unknown,
        UInt8,
        Int16,
        Int32,
        Int64,
        Float,
        Double,
    };

    AudioFormat();

    bool operator==(const AudioFormat& other) const;
    bool operator!=(const AudioFormat& other) const;

    bool isValid() const;

    int sampleRate() const;
    int channelCount() const;
    SampleFormat sampleFormat() const;

    void setSampleRate(int sampleRate);
    void setChannelCount(int channelCount);
    void setSampleFormat(SampleFormat format);

    int bytesForDuration(uint64_t ms) const;
    uint64_t durationForBytes(int byteCount) const;

    int bytesForFrames(int frameCount) const;
    int framesForBytes(int byteCount) const;

    int framesForDuration(uint64_t ms) const;
    uint64_t durationForFrames(int frameCount) const;

    int bytesPerFrame() const;
    int bytesPerSample() const;

private:
    SampleFormat m_sampleFormat;
    int m_channelCount;
    int m_sampleRate;
};
} // namespace Fooyin
