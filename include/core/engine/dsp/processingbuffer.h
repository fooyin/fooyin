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

#pragma once

#include "fycore_export.h"

#include <core/engine/audiobuffer.h>
#include <core/engine/audioformat.h>

#include <span>
#include <vector>

namespace Fooyin {
/*!
 * Processing-stage audio buffer storing interleaved F64 samples.
 *
 * Used for intermediate engine processing before conversion back to
 * output format.
 */
class FYCORE_EXPORT ProcessingBuffer
{
public:
    ProcessingBuffer();
    ProcessingBuffer(AudioFormat format, uint64_t startTimeNs);
    ProcessingBuffer(std::span<const double> samples, AudioFormat format, uint64_t startTimeNs);

    [[nodiscard]] bool isValid() const;

    void reset();

    [[nodiscard]] AudioFormat format() const;
    [[nodiscard]] int frameCount() const;
    [[nodiscard]] int sampleCount() const;
    [[nodiscard]] uint64_t startTimeNs() const;
    [[nodiscard]] uint64_t endTimeNs() const;
    [[nodiscard]] uint64_t durationNs() const;

    void setStartTimeNs(uint64_t startTimeNs);

    void reserveSamples(size_t samples);
    void resizeSamples(size_t samples);

    void clear();

    void append(std::span<const double> samples);

    [[nodiscard]] std::span<const double> constData() const;
    [[nodiscard]] std::span<double> data();
    [[nodiscard]] const std::vector<double>& samples() const;
    [[nodiscard]] std::vector<double>& samples();

    [[nodiscard]] AudioBuffer toAudioBuffer() const;

private:
    std::vector<double> m_samples;
    AudioFormat m_format;
    uint64_t m_startTimeNs;
};
} // namespace Fooyin
