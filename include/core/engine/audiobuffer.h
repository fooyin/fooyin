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

#include <QExplicitlySharedDataPointer>

#include <span>

namespace Fooyin {
class AudioBufferPrivate;

/*!
 * Interleaved PCM buffer with timeline metadata.
 *
 * `AudioBuffer` is implicitly shared (copy-on-write). The payload is raw bytes
 * interpreted according to `format()`, and `startTime()` anchors the buffer in
 * source timeline milliseconds.
 */
class FYCORE_EXPORT AudioBuffer
{
public:
    AudioBuffer();
    AudioBuffer(AudioFormat format, uint64_t startTime);
    AudioBuffer(std::span<const std::byte> data, AudioFormat format, uint64_t startTime);
    AudioBuffer(const uint8_t* data, size_t size, AudioFormat format, uint64_t startTime);
    ~AudioBuffer();

    AudioBuffer(const AudioBuffer& other);
    AudioBuffer& operator=(const AudioBuffer& other);
    AudioBuffer(AudioBuffer&& other) noexcept;

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
    //! Ensure unique ownership before in-place mutation.
    void detach();

    [[nodiscard]] AudioFormat format() const;
    //! Number of audio frames represented by payload and format.
    [[nodiscard]] int frameCount() const;
    //! Number of interleaved samples represented by payload and format.
    [[nodiscard]] int sampleCount() const;
    //! Payload size in bytes.
    [[nodiscard]] int byteCount() const;
    //! Buffer start timestamp in milliseconds.
    [[nodiscard]] uint64_t startTime() const;
    //! Buffer end timestamp in milliseconds.
    [[nodiscard]] uint64_t endTime() const;
    //! Buffer duration in milliseconds.
    [[nodiscard]] uint64_t duration() const;

    [[nodiscard]] std::span<const std::byte> constData() const;
    [[nodiscard]] const std::byte* data() const;
    std::byte* data();

    void setStartTime(uint64_t startTime);

    //! Fill payload with silence according to sample format.
    void fillSilence();

private:
    QExplicitlySharedDataPointer<AudioBufferPrivate> p;
};
using AudioData = std::vector<AudioBuffer>;
} // namespace Fooyin
