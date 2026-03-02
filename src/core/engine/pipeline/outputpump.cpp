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

#include "outputpump.h"

#include <algorithm>
#include <cmath>

namespace Fooyin {
OutputPump::OutputPump()
    : OutputPump{{}}
{ }

OutputPump::OutputPump(const Config& config)
    : m_config{config}
{ }

bool OutputPump::shouldHoldForPreroll(int queuedPendingFrames, int minBufferedFramesToDrain)
{
    return queuedPendingFrames > 0 && queuedPendingFrames < std::max(0, minBufferedFramesToDrain);
}

bool OutputPump::shouldWaitAfterDrain(int freeFrames, int writtenFrames)
{
    return freeFrames <= 0 || writtenFrames <= 0;
}

std::chrono::milliseconds OutputPump::writeBackoff(const AudioFormat& outputFormat,
                                                   const OutputState& outputState) const
{
    const int minWaitMs = std::max(1, m_config.minWaitMs);
    const int maxWaitMs = std::max(minWaitMs, m_config.maxWaitMs);
    const int divisor   = std::max(1, m_config.queuedDrainDivisor);

    if(!outputFormat.isValid() || outputFormat.sampleRate() <= 0) {
        return std::chrono::milliseconds{minWaitMs};
    }

    const int queuedFrames = std::max(1, outputState.queuedFrames);
    const int drainFrames  = std::max(1, queuedFrames / divisor);
    const int waitMs       = static_cast<int>(
        std::llround(static_cast<double>(drainFrames) * 1000.0 / static_cast<double>(outputFormat.sampleRate())));

    return std::chrono::milliseconds{std::clamp(waitMs, minWaitMs, maxWaitMs)};
}
} // namespace Fooyin
