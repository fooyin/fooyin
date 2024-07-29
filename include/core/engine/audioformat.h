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
enum class SampleFormat
{
    Unknown,
    U8,
    S16,
    S24,
    S32,
    F32,
};

// TODO: Handle channel layout
class FYCORE_EXPORT AudioFormat
{
public:
    AudioFormat();
    AudioFormat(SampleFormat format, int sampleRate, int channelCount);

    bool operator==(const AudioFormat& other) const noexcept;
    bool operator!=(const AudioFormat& other) const noexcept;

    [[nodiscard]] bool isValid() const;

    [[nodiscard]] int sampleRate() const;
    [[nodiscard]] int channelCount() const;
    [[nodiscard]] SampleFormat sampleFormat() const;

    void setSampleRate(int sampleRate);
    void setChannelCount(int channelCount);
    void setSampleFormat(SampleFormat format);

    [[nodiscard]] int bytesForDuration(uint64_t ms) const;
    [[nodiscard]] uint64_t durationForBytes(int byteCount) const;

    [[nodiscard]] int bytesForFrames(int frameCount) const;
    [[nodiscard]] int framesForBytes(int byteCount) const;

    [[nodiscard]] int framesForDuration(uint64_t ms) const;
    [[nodiscard]] uint64_t durationForFrames(int frameCount) const;

    [[nodiscard]] int bytesPerFrame() const;
    [[nodiscard]] int bytesPerSample() const;
    [[nodiscard]] int bitsPerSample() const;

private:
    SampleFormat m_sampleFormat;
    int m_channelCount;
    int m_sampleRate;
};
} // namespace Fooyin
