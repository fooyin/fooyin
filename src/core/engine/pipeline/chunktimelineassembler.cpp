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

#include "chunktimelineassembler.h"

#include "core/engine/audioconverter.h"
#include <utils/timeconstants.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace Fooyin::Timeline {
bool coalesceChunks(const ProcessingBufferList& chunks, const AudioFormat& outputFormat, AudioBuffer& combined,
                    bool dither)
{
    if(!outputFormat.isValid()) {
        return false;
    }

    size_t totalBytes{0};
    uint64_t startTime{0};
    bool hasStartTime{false};

    for(size_t i{0}; i < chunks.count(); ++i) {
        const auto* chunk = chunks.item(i);

        if(!chunk || !chunk->isValid()) {
            continue;
        }

        const int frames = chunk->frameCount();
        if(frames <= 0) {
            continue;
        }

        const uint64_t bytes = outputFormat.bytesForFrames(frames);
        if(bytes == 0) {
            continue;
        }

        if(!hasStartTime) {
            startTime    = chunk->startTimeNs() / Time::NsPerMs;
            hasStartTime = true;
        }

        if(bytes > std::numeric_limits<size_t>::max() - totalBytes) {
            return false;
        }

        totalBytes += static_cast<size_t>(bytes);
    }

    if(totalBytes == 0 || !hasStartTime) {
        return false;
    }

    if(!combined.isValid() || combined.format() != outputFormat) {
        combined = AudioBuffer{outputFormat, startTime};
    }
    else {
        combined.setStartTime(startTime);
    }

    if(std::cmp_less(combined.byteCount(), totalBytes)) {
        combined.reserve(totalBytes);
    }

    combined.resize(totalBytes);

    size_t writeOffset{0};

    for(size_t i{0}; i < chunks.count(); ++i) {
        const auto* chunk = chunks.item(i);
        if(!chunk || !chunk->isValid()) {
            continue;
        }

        const int frames = chunk->frameCount();
        if(frames <= 0) {
            continue;
        }

        const uint64_t bytes = outputFormat.bytesForFrames(frames);
        if(bytes <= 0) {
            continue;
        }

        auto* outputData = combined.data() + writeOffset;

        if(chunk->format() == outputFormat) {
            const auto inputData  = chunk->constData();
            const auto inputBytes = std::as_bytes(inputData);

            if(inputBytes.size() < static_cast<size_t>(bytes)) {
                return false;
            }

            std::memcpy(outputData, inputBytes.data(), static_cast<size_t>(bytes));
        }
        else {
            const auto inputData  = chunk->constData();
            const auto inputBytes = std::as_bytes(inputData);

            if(!Audio::convert(chunk->format(), inputBytes.data(), outputFormat, outputData, frames, dither)) {
                return false;
            }
        }

        writeOffset += static_cast<size_t>(bytes);
    }

    return true;
}

bool buildTimelineChunks(const ProcessingBufferList& chunks, std::vector<TimelineChunk>& timelineChunks,
                         const StreamId streamId, uint64_t timelineEpoch)
{
    timelineChunks.clear();
    timelineChunks.reserve(chunks.count());

    for(size_t i{0}; i < chunks.count(); ++i) {
        const auto* chunk = chunks.item(i);

        if(!chunk || !chunk->isValid()) {
            continue;
        }

        const int frames = chunk->frameCount();
        if(frames <= 0) {
            continue;
        }

        const auto fmt = chunk->format();
        if(!fmt.isValid() || fmt.sampleRate() <= 0) {
            continue;
        }

        const uint64_t srcFrameNs = chunk->sourceFrameDurationNs();
        timelineChunks.push_back(TimelineChunk{
            .valid           = srcFrameNs > 0,
            .startNs         = srcFrameNs > 0 ? chunk->startTimeNs() : 0,
            .frameDurationNs = srcFrameNs,
            .frames          = frames,
            .sampleRate      = fmt.sampleRate(),
            .streamId        = streamId,
            .epoch           = timelineEpoch,
        });
    }

    return !timelineChunks.empty();
}
} // namespace Fooyin::Timeline
