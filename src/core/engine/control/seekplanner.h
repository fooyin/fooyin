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

#include <cstdint>

namespace Fooyin {
// Pure seek decision planner.
// Consumes a full context snapshot and returns a deterministic plan.
// Execution details (decoder/pipeline mutation) stay in AudioEngine.
enum class SeekStrategy : uint8_t
{
    NoOp = 0,
    SimpleSeek,
    CrossfadeSeek
};

struct SeekPlanContext
{
    bool decoderValid{false};
    bool isPlaying{false};
    bool crossfadeEnabled{false};
    uint64_t trackDurationMs{0};
    int autoCrossfadeOutMs{0};

    int seekFadeOutMs{0};
    int seekFadeInMs{0};

    bool hasCurrentStream{false};
    uint64_t requestedPositionMs{0};
    uint64_t bufferedDurationMs{0};

    int decoderHighWatermarkMs{0};
    int bufferLengthMs{0};
};

struct SeekPlan
{
    SeekStrategy strategy{SeekStrategy::NoOp};
    int reserveTargetMs{0};
    int fadeOutDurationMs{0};
    int fadeInDurationMs{0};
};

FYCORE_EXPORT SeekPlan planSeek(const SeekPlanContext& context);
} // namespace Fooyin
