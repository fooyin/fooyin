/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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
    AudioBuffer(std::span<const uint8_t> data, AudioFormat format, uint64_t startTime);
    AudioBuffer(const uint8_t* data, int size, AudioFormat format, uint64_t startTime);
    ~AudioBuffer();

    AudioBuffer(const AudioBuffer& other);
    AudioBuffer& operator=(const AudioBuffer& other);
    AudioBuffer(AudioBuffer&& other) noexcept;

    void reserve(size_t size);
    void append(std::span<const uint8_t> data);
    void clear();
    void reset();

    bool isValid() const;
    void detach();

    AudioFormat format() const;
    int frameCount() const;
    int sampleCount() const;
    int byteCount() const;
    uint64_t startTime() const;
    uint64_t duration() const;

    std::span<const uint8_t> constData() const;
    uint8_t* data();

    void fillSilence();
    void fillRemainingWithSilence();

private:
    struct Private;
    QExplicitlySharedDataPointer<Private> p;
};
using AudioData = std::vector<AudioBuffer>;
} // namespace Fooyin
