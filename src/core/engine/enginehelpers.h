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

#include <core/track.h>

namespace Fooyin {
//! True when two track handles refer to the same logical track request.
//! Uses DB id when available, otherwise falls back to segment identity.
FYCORE_EXPORT bool sameTrackIdentity(const Track& lhs, const Track& rhs);
//! True when two tracks refer to the exact same logical segment.
FYCORE_EXPORT bool sameTrackSegment(const Track& lhs, const Track& rhs);
//! True when @p nextTrack starts exactly at current offset + duration in same file/subsong.
FYCORE_EXPORT bool isContiguousSameFileSegment(const Track& currentTrack, const Track& nextTrack);
//! True when both tracks point to same file but represent different segment metadata.
FYCORE_EXPORT bool isMultiTrackFileTransition(const Track& currentTrack, const Track& nextTrack);
//! Convert absolute position to position relative to segment offset.
FYCORE_EXPORT uint64_t relativeTrackPositionMs(uint64_t absolutePosMs, uint64_t offsetMs);
//! Convert stream-relative position to absolute timeline position.
FYCORE_EXPORT uint64_t absoluteTrackPositionMs(uint64_t streamPosMs, uint64_t streamOriginMs);
//! Convert stream-relative position to track-relative logical position.
FYCORE_EXPORT uint64_t relativeTrackPositionMs(uint64_t streamPosMs, uint64_t streamOriginMs, uint64_t trackOffsetMs);
//! Latch helper to emit track-end signal only once per logical segment.
FYCORE_EXPORT bool shouldEmitTrackEndOnce(Track& lastEndedTrack, const Track& currentTrack);
} // namespace Fooyin
