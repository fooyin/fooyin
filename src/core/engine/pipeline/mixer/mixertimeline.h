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

#include "audiomixer.h"

#include <cstdint>
#include <span>
#include <vector>

namespace Fooyin::Timeline {
/*!
 * Timeline utilities for mixer output segments.
 *
 * Provides helpers for appending/merging contiguous segments and consuming
 * timeline ranges aligned to drained frame windows.
 */
//! Append a segment, merging with tail where continuity allows.
FYCORE_EXPORT void appendSegment(std::vector<AudioMixer::TimelineSegment>& segments,
                                 const AudioMixer::TimelineSegment& segment);
//! Append unknown-timestamp segment for `frames`.
FYCORE_EXPORT void appendUnknown(std::vector<AudioMixer::TimelineSegment>& segments, int frames,
                                 StreamId streamId = InvalidStreamId);
//! Convert chunk timeline range into mixer segments for drained output window.
FYCORE_EXPORT void consumeRange(std::span<const TimedAudioFifo::TimelineChunk> chunks, int offsetFrames, int frames,
                                StreamId fallbackStreamId, std::vector<AudioMixer::TimelineSegment>& out);
//! First valid start timestamp across segments (if present).
[[nodiscard]] FYCORE_EXPORT bool startNs(const std::vector<AudioMixer::TimelineSegment>& segments,
                                         uint64_t& startNsOut);
} // namespace Fooyin::Timeline
