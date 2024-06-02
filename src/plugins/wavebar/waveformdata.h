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

#include <core/engine/audioformat.h>

#include <tuple>
#include <vector>

namespace Fooyin::WaveBar {
struct WaveformSample
{
    float max{-1.0};
    float min{1.0};
    float rms{0.0};
};

template <typename T>
struct WaveformData
{
    AudioFormat format;
    uint64_t duration{0};
    int channels{0};
    bool complete{false};
    int samplesPerChannel{2048};

    struct ChannelData
    {
        std::vector<T> max;
        std::vector<T> min;
        std::vector<T> rms;

        bool operator==(const ChannelData& other) const noexcept
        {
            return std::tie(max, min, rms) == std::tie(other.max, other.min, other.rms);
        }

        bool operator!=(const ChannelData& other) const noexcept
        {
            return !(*this == other);
        }
    };
    std::vector<ChannelData> channelData;

    bool operator==(const WaveformData<T>& other) const noexcept
    {
        return std::tie(format, duration, channels, complete, samplesPerChannel, channelData)
            == std::tie(other.format, other.duration, other.channels, other.complete, other.samplesPerChannel,
                        other.channelData);
    }

    bool operator!=(const WaveformData<T>& other) const noexcept
    {
        return !(*this == other);
    }

    [[nodiscard]] bool empty() const
    {
        return !format.isValid() && channelData.empty();
    }

    [[nodiscard]] int sampleCount() const
    {
        if(channelData.empty()) {
            return 0;
        }

        return static_cast<int>(channelData.front().max.size());
    }
};
} // namespace Fooyin::WaveBar
