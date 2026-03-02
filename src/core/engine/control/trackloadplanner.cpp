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

#include "trackloadplanner.h"

namespace Fooyin {
LoadPlan planTrackLoad(const TrackLoadContext& context)
{
    LoadPlan plan;

    plan.trySegmentSwitch
        = !context.manualChange && context.isPlaying && context.decoderValid && context.isContiguousSameFileSegment;
    plan.tryManualFadeDefer = context.manualChange;
    plan.tryAutoTransition  = !context.manualChange && !context.hasPendingTransportFade;

    if(plan.trySegmentSwitch) {
        plan.firstStrategy = LoadStrategy::SegmentSwitch;
    }
    else if(plan.tryManualFadeDefer) {
        plan.firstStrategy = LoadStrategy::ManualFadeDefer;
    }
    else if(plan.tryAutoTransition) {
        plan.firstStrategy = LoadStrategy::AutoTransition;
    }
    else {
        plan.firstStrategy = LoadStrategy::FullReinit;
    }

    return plan;
}
} // namespace Fooyin
