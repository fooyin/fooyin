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

#include "audioutils.h"

#include <algorithm>
#include <limits>

constexpr auto MaxStreamBufferSamples = 96U * 1000U * 1000U;

namespace Fooyin::Audio {
size_t bufferSamplesFromMs(int ms, int sampleRate, int channels)
{
    if(ms <= 0 || sampleRate <= 0 || channels <= 0) {
        return 0;
    }

    const auto checkedMul = [](int64_t a, int64_t b) {
        if(a <= 0 || b <= 0) {
            return int64_t{0};
        }
        if(a > (std::numeric_limits<int64_t>::max() / b)) {
            return std::numeric_limits<int64_t>::max();
        }
        return a * b;
    };

    const int64_t msRate    = checkedMul(static_cast<int64_t>(ms), sampleRate);
    const int64_t samples   = checkedMul(msRate, channels);
    const int64_t numerator = samples >= (std::numeric_limits<int64_t>::max() - 999)
                                ? std::numeric_limits<int64_t>::max()
                                : (samples + 999);

    const int64_t roundedSamples = numerator / 1000;
    const auto maxSamples        = static_cast<int64_t>(MaxStreamBufferSamples);

    return static_cast<size_t>(std::clamp<int64_t>(roundedSamples, 0, maxSamples));
}

bool inputFormatsMatch(const AudioFormat& lhs, const AudioFormat& rhs)
{
    if(lhs.sampleRate() != rhs.sampleRate() || lhs.channelCount() != rhs.channelCount()) {
        return false;
    }

    if(lhs.hasChannelLayout() != rhs.hasChannelLayout()) {
        return false;
    }
    if(!lhs.hasChannelLayout()) {
        return true;
    }

    for(int i = 0; i < lhs.channelCount(); ++i) {
        if(lhs.channelPosition(i) != rhs.channelPosition(i)) {
            return false;
        }
    }
    return true;
}

bool outputFormatsMatch(const AudioFormat& lhs, const AudioFormat& rhs)
{
    return inputFormatsMatch(lhs, rhs) && lhs.sampleFormat() == rhs.sampleFormat();
}
} // namespace Fooyin::Audio
