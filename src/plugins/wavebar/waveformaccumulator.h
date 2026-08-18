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
 */

#pragma once

#include "waveformdata.h"

#include <core/engine/audiobuffer.h>

#include <stop_token>
#include <vector>

namespace Fooyin::WaveBar {
class WaveformAccumulator
{
public:
    static constexpr auto MaxTargetSampleCount = 65'536;

    enum class ProcessResult : uint8_t
    {
        Accepted = 0,
        Complete,
        Cancelled,
        Invalid,
    };

    WaveformAccumulator(WaveformData<float>* data, uint64_t totalFrames, int targetSampleCount);

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] bool complete() const;
    [[nodiscard]] uint64_t processedFrames() const;
    [[nodiscard]] int targetSampleCount() const;

    ProcessResult process(const AudioBuffer& buffer, std::stop_token stopToken = {});
    bool finish();

private:
    [[nodiscard]] uint64_t binEndFrame() const;
    bool finishBin();

    WaveformData<float>* m_data;
    uint64_t m_totalFrames;
    uint64_t m_processedFrames;
    uint64_t m_framesInBin;
    int m_targetSampleCount;
    int m_binIndex;
    std::vector<float> m_binMax;
    std::vector<float> m_binMin;
    std::vector<double> m_binSquareSum;
};
} // namespace Fooyin::WaveBar
