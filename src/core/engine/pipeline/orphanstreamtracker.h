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

#include "audiostream.h"

#include <chrono>
#include <cstdint>
#include <optional>

namespace Fooyin {
/*!
 * Tracks a single fading-out orphan stream and its drain deadline.
 *
 * Used by `AudioPipeline` during crossfade/seek transitions where the previous
 * stream is allowed to drain briefly while a new stream becomes primary.
 */
class OrphanStreamTracker
{
public:
    struct Evaluation
    {
        //! True when orphan should be force-cleaned from pipeline state.
        bool shouldCleanup{false};
        //! True when cleanup reason is deadline timeout rather than natural drain.
        bool timedOut{false};
    };

    //! True when an orphan stream id is currently tracked.
    [[nodiscard]] bool hasOrphan() const;
    //! Current orphan stream id (or `InvalidStreamId`).
    [[nodiscard]] StreamId streamId() const;
    //! Replace tracked orphan id and return previous one.
    [[nodiscard]] StreamId replace(StreamId streamId);
    //! Clear orphan id and deadline.
    void clear();
    //! Arm deadline using current buffered+delay estimate clamped to timeout bounds.
    void armDrainDeadline(uint64_t bufferedDurationMs, uint64_t playbackDelayMs, std::chrono::milliseconds minTimeout,
                          std::chrono::milliseconds maxTimeout);
    //! Evaluate whether orphan can remain active or should be cleaned now.
    [[nodiscard]] Evaluation evaluate(const AudioStream* orphanStream) const;

private:
    StreamId m_streamId{InvalidStreamId};
    std::optional<std::chrono::steady_clock::time_point> m_drainDeadline;
};
} // namespace Fooyin
