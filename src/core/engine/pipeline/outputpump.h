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

#include "buffereddspstage.h"

#include <core/engine/audioformat.h>
#include <core/engine/audiooutput.h>

#include <chrono>
#include <utility>

namespace Fooyin {
/*!
 * Output-side helper for pending-buffer drain and backoff decisions.
 */
class FYCORE_EXPORT OutputPump
{
public:
    struct Config
    {
        int minWaitMs{3};
        int maxWaitMs{40};
        int queuedDrainDivisor{2};
    };

    struct PendingDrainResult
    {
        int writtenFrames{0};
        int queueOffsetStartFrames{0};
        int queueOffsetEndFrames{0};
    };

    OutputPump();
    explicit OutputPump(const Config& config);

    [[nodiscard]] static bool shouldHoldForPreroll(int queuedPendingFrames, int minBufferedFramesToDrain);
    [[nodiscard]] static bool shouldWaitAfterDrain(int freeFrames, int writtenFrames);

    [[nodiscard]] std::chrono::milliseconds writeBackoff(const AudioFormat& outputFormat,
                                                         const OutputState& outputState) const;

    template <typename Fn>
    [[nodiscard]] PendingDrainResult drainPending(BufferedDspStage& stage, Fn&& writer) const
    {
        const auto writeResult = stage.writePendingOutput(std::forward<Fn>(writer));
        return {
            .writtenFrames          = writeResult.writtenFrames,
            .queueOffsetStartFrames = writeResult.queueOffsetStartFrames,
            .queueOffsetEndFrames   = writeResult.queueOffsetEndFrames,
        };
    }

private:
    Config m_config;
};
} // namespace Fooyin
