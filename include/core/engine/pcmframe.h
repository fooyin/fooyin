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

#include <core/engine/audioformat.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>

namespace Fooyin {
/*!
 * Fixed-hop interleaved PCM snapshot for visualisers and other analysis consumers.
 *
 * Samples are linear float PCM in interleaved channel order.
 */
struct PcmFrame
{
    static constexpr int MaxChannels = 8;
    static constexpr int MaxFrames   = 1024;
    static constexpr int MaxSamples  = MaxChannels * MaxFrames;

    std::array<float, MaxSamples> samples{};
    AudioFormat format{SampleFormat::F32, 0, 0};
    int frameCount{0};
    uint64_t streamTimeMs{0};
    uint32_t streamId{0};
    std::chrono::steady_clock::time_point presentationTime;

    [[nodiscard]] size_t sampleCount() const
    {
        if(frameCount <= 0 || format.channelCount() <= 0) {
            return 0;
        }

        return static_cast<size_t>(frameCount) * static_cast<size_t>(format.channelCount());
    }

    [[nodiscard]] std::span<const float> interleavedSamples() const
    {
        return {samples.data(), sampleCount()};
    }
};
} // namespace Fooyin
