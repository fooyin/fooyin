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

#include <core/engine/audioformat.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Fooyin {
/*!
 * Interleaved PCM buffer with timeline metadata.
 *
 * The payload is raw bytes interpreted according to `format()`, and
 * `startTime()` anchors the buffer in source timeline milliseconds.
 *
 * `isValid()` requires both a valid `AudioFormat` and a payload size aligned
 * to whole frames for that format.
 */
class FYCORE_EXPORT AudioBuffer
{
public:
    AudioBuffer();
    AudioBuffer(const AudioFormat& format, uint64_t startTime);
    AudioBuffer(std::span<const std::byte> data, const AudioFormat& format, uint64_t startTime);
    AudioBuffer(const uint8_t* data, size_t size, const AudioFormat& format, uint64_t startTime);
    ~AudioBuffer();

    AudioBuffer(const AudioBuffer& other);
    AudioBuffer& operator=(const AudioBuffer& other);
    AudioBuffer(AudioBuffer&& other) noexcept;
    AudioBuffer& operator=(AudioBuffer&& other) noexcept;

    //! Reserve payload capacity in bytes.
    void reserve(size_t size);
    //! Resize payload to `size` bytes.
    void resize(size_t size);
    //! Append raw bytes to payload.
    void append(std::span<const std::byte> data);
    //! Append raw bytes to payload.
    void append(const std::byte* data, size_t size);
    //! Remove `size` bytes from beginning of payload.
    void erase(size_t size);
    //! Clear payload bytes, keeping format/timing metadata.
    void clear();
    //! Reset payload and metadata to default-invalid state.
    void reset();

    //! True when format and payload are consistent/usable.
    [[nodiscard]] bool isValid() const;

    //! Returns buffer format, or invalid format when `!isValid()`.
    [[nodiscard]] AudioFormat format() const;
    //! Number of audio frames represented by payload and format.
    [[nodiscard]] int frameCount() const;
    //! Number of interleaved samples represented by payload and format.
    [[nodiscard]] int sampleCount() const;
    //! Payload size in bytes.
    [[nodiscard]] int byteCount() const;
    //! Buffer start timestamp in milliseconds, or `UINT64_MAX` when invalid.
    [[nodiscard]] uint64_t startTime() const;
    //! Buffer end timestamp in milliseconds, or `UINT64_MAX` when invalid.
    [[nodiscard]] uint64_t endTime() const;
    //! Buffer duration in milliseconds.
    [[nodiscard]] uint64_t duration() const;

    //! Const byte view of payload, or empty span when invalid.
    [[nodiscard]] std::span<const std::byte> constData() const;
    //! Raw payload pointer, or `nullptr` when invalid.
    [[nodiscard]] const std::byte* data() const;
    //! Mutable payload pointer, or `nullptr` when invalid.
    std::byte* data();

    //! Set start timestamp in milliseconds when buffer is valid.
    void setStartTime(uint64_t startTime);

    //! Fill payload with silence according to sample format.
    void fillSilence();

private:
    std::vector<std::byte> m_buffer;
    AudioFormat m_format;
    uint64_t m_startTime;
};
using AudioData = std::vector<AudioBuffer>;
} // namespace Fooyin
