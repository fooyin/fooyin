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

    void reserve(size_t size);
    void resize(size_t size);
    void append(std::span<const std::byte> data);
    void append(const std::byte* data, size_t size);
    void clear();
    void reset();

    [[nodiscard]] bool isValid() const;
    void detach();

    [[nodiscard]] AudioFormat format() const;
    [[nodiscard]] int frameCount() const;
    [[nodiscard]] int sampleCount() const;
    [[nodiscard]] int byteCount() const;
    [[nodiscard]] uint64_t startTime() const;
    [[nodiscard]] uint64_t duration() const;

    [[nodiscard]] std::span<const std::byte> constData() const;
    [[nodiscard]] const std::byte* data() const;
    std::byte* data();

    void fillSilence();
    void fillRemainingWithSilence();
    void adjustVolumeOfSamples(double volume);

private:
    struct Private;
    QExplicitlySharedDataPointer<Private> p;
};
using AudioData = std::vector<AudioBuffer>;
} // namespace Fooyin
