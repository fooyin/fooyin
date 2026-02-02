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

#include "timedaudiofifo.h"

#include <core/engine/dsp/processingbufferlist.h>

#include <cstdint>
#include <vector>

namespace Fooyin {
/*!
 * Shared helper functions for coalescing processed chunks and building timeline metadata.
 */
namespace Timeline {
using TimelineChunk = TimedAudioFifo::TimelineChunk;

[[nodiscard]] FYCORE_EXPORT bool coalesceChunks(const ProcessingBufferList& chunks, const AudioFormat& outputFormat,
                                                AudioBuffer& combined, bool dither);
[[nodiscard]] FYCORE_EXPORT bool buildTimelineChunks(const ProcessingBufferList& chunks,
                                                     std::vector<TimelineChunk>& timelineChunks,
                                                     StreamId streamId = InvalidStreamId, uint64_t timelineEpoch = 0);
} // namespace Timeline
} // namespace Fooyin
