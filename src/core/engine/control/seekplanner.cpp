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

#include "seekplanner.h"

#include <algorithm>
#include <limits>

namespace Fooyin {
SeekPlan planSeek(const SeekPlanContext& context)
{
    SeekPlan plan;

    if(!context.decoderValid) {
        plan.strategy = SeekStrategy::NoOp;
        return plan;
    }

    if(!context.isPlaying) {
        plan.strategy = SeekStrategy::SimpleSeek;
        return plan;
    }

    if(context.crossfadeEnabled && context.trackDurationMs > 0) {
        const int autoFadeOutMs = std::max(0, context.autoCrossfadeOutMs);
        if(autoFadeOutMs > 0) {
            const auto fadeWindowMs = static_cast<uint64_t>(autoFadeOutMs);
            const uint64_t triggerPosMs
                = context.trackDurationMs > fadeWindowMs ? (context.trackDurationMs - fadeWindowMs) : 0;
            if(context.requestedPositionMs >= triggerPosMs) {
                plan.strategy = SeekStrategy::SimpleSeek;
                return plan;
            }
        }
    }

    if(!context.crossfadeEnabled || (context.seekFadeOutMs <= 0 && context.seekFadeInMs <= 0)) {
        plan.strategy = SeekStrategy::SimpleSeek;
        return plan;
    }

    if(!context.hasCurrentStream) {
        plan.strategy = SeekStrategy::SimpleSeek;
        return plan;
    }

    const int requestedFadeOutMs = std::max(0, context.seekFadeOutMs);
    if(requestedFadeOutMs > 0) {
        const int highWatermarkMs = std::max(0, context.decoderHighWatermarkMs);
        plan.reserveTargetMs      = std::min(std::max(requestedFadeOutMs, highWatermarkMs), context.bufferLengthMs);
    }

    const int availableFadeOutMs = static_cast<int>(
        std::min<uint64_t>(context.bufferedDurationMs, static_cast<uint64_t>(std::numeric_limits<int>::max())));
    plan.fadeOutDurationMs = std::min(requestedFadeOutMs, availableFadeOutMs);
    if(plan.fadeOutDurationMs <= 0) {
        plan.strategy          = SeekStrategy::SimpleSeek;
        plan.fadeOutDurationMs = 0;
        plan.fadeInDurationMs  = 0;
        return plan;
    }

    plan.fadeInDurationMs = std::max(0, context.seekFadeInMs);
    plan.strategy         = SeekStrategy::CrossfadeSeek;

    return plan;
}
} // namespace Fooyin
