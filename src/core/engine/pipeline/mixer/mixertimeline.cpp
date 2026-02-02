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

#include "mixertimeline.h"

#include <algorithm>

namespace Fooyin::Timeline {
void appendSegment(std::vector<AudioMixer::TimelineSegment>& segments, const AudioMixer::TimelineSegment& segment)
{
    if(segment.frames <= 0) {
        return;
    }

    if(segments.empty()) {
        segments.push_back(segment);
        return;
    }

    auto& tail = segments.back();
    if(!tail.valid && !segment.valid && tail.streamId == segment.streamId) {
        tail.frames += segment.frames;
        return;
    }

    if(tail.valid && segment.valid && tail.streamId == segment.streamId
       && tail.frameDurationNs == segment.frameDurationNs) {
        const uint64_t expectedStartNs
            = tail.startNs + (tail.frameDurationNs * static_cast<uint64_t>(std::max(0, tail.frames)));

        if(expectedStartNs == segment.startNs) {
            tail.frames += segment.frames;
            return;
        }
    }

    segments.push_back(segment);
}

void appendUnknown(std::vector<AudioMixer::TimelineSegment>& segments, int frames, const StreamId streamId)
{
    appendSegment(segments, AudioMixer::TimelineSegment{
                                .valid           = false,
                                .startNs         = 0,
                                .frameDurationNs = 0,
                                .frames          = frames,
                                .streamId        = streamId,
                            });
}

void consumeRange(std::span<const TimedAudioFifo::TimelineChunk> chunks, int offsetFrames, int frames,
                  const StreamId fallbackStreamId, std::vector<AudioMixer::TimelineSegment>& out)
{
    out.clear();

    if(frames <= 0) {
        return;
    }

    int remaining = frames;
    int rangePos  = std::max(0, offsetFrames);
    int cursor{0};

    for(const auto& chunk : chunks) {
        const int chunkFrames = std::max(0, chunk.frames);

        if(chunkFrames <= 0) {
            continue;
        }

        const int chunkStart = cursor;
        const int chunkEnd   = chunkStart + chunkFrames;
        cursor               = chunkEnd;

        if(chunkEnd <= rangePos) {
            continue;
        }

        if(remaining <= 0) {
            break;
        }

        const int skip = std::max(0, rangePos - chunkStart);
        const int take = std::min(std::max(0, chunkFrames - skip), remaining);

        if(take <= 0) {
            continue;
        }

        uint64_t startNs{0};
        uint64_t frameDurationNs{0};

        const bool valid = chunk.valid && chunk.frameDurationNs > 0;
        if(valid) {
            frameDurationNs = chunk.frameDurationNs;
            startNs         = chunk.startNs + (frameDurationNs * static_cast<uint64_t>(skip));
        }

        const StreamId segmentStreamId = chunk.streamId != InvalidStreamId ? chunk.streamId : fallbackStreamId;
        appendSegment(out, AudioMixer::TimelineSegment{
                               .valid           = valid,
                               .startNs         = valid ? startNs : 0,
                               .frameDurationNs = valid ? frameDurationNs : 0,
                               .frames          = take,
                               .streamId        = segmentStreamId,
                           });
        rangePos += take;
        remaining -= take;
    }

    if(remaining > 0) {
        appendSegment(out, AudioMixer::TimelineSegment{
                               .valid           = false,
                               .startNs         = 0,
                               .frameDurationNs = 0,
                               .frames          = remaining,
                               .streamId        = fallbackStreamId,
                           });
    }
}

bool startNs(const std::vector<AudioMixer::TimelineSegment>& segments, uint64_t& startNsOut)
{
    for(const auto& segment : segments) {
        if(!segment.valid) {
            continue;
        }

        startNsOut = segment.startNs;
        return true;
    }

    return false;
}
} // namespace Fooyin::Timeline
