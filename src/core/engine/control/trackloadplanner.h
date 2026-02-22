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
// Pure strategy planner for loadTrack decisions.
// Output describes strategy ordering only; timing/buffering remains runtime execution logic.
enum class LoadStrategy : uint8_t
{
    SegmentSwitch = 0,
    ManualFadeDefer,
    AutoTransition,
    FullReinit
};

struct TrackLoadContext
{
    bool manualChange{false};
    bool isPlaying{false};
    bool decoderValid{false};
    bool hasPendingTransportFade{false};
    bool isContiguousSameFileSegment{false};
};

struct LoadPlan
{
    LoadStrategy firstStrategy{LoadStrategy::FullReinit};
    bool trySegmentSwitch{false};
    bool tryManualFadeDefer{false};
    bool tryAutoTransition{false};
};

FYCORE_EXPORT LoadPlan planTrackLoad(const TrackLoadContext& context);
} // namespace Fooyin
