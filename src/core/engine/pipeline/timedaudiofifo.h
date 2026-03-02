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

#include <core/engine/audiobuffer.h>
#include <core/engine/pipeline/audiostream.h>

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace Fooyin {
/*!
 * Single-threaded FIFO of rendered audio plus source timeline metadata.
 */
class FYCORE_EXPORT TimedAudioFifo
{
public:
    struct TimelineChunk
    {
        bool valid{false};
        uint64_t startNs{0};
        uint64_t frameDurationNs{0};
        int frames{0};
        int sampleRate{0};
        StreamId streamId{InvalidStreamId};
        uint64_t epoch{0};
    };

    struct WriteResult
    {
        int writtenFrames{0};
        int queueOffsetStartFrames{0};
        int queueOffsetEndFrames{0};
    };

    struct AppendResult
    {
        bool appended{false};
        bool formatMismatch{false};
        int appendedFrames{0};
    };

    TimedAudioFifo();

    [[nodiscard]] bool hasData() const;
    [[nodiscard]] int queuedFrames() const;
    [[nodiscard]] int writeOffsetFrames() const;
    [[nodiscard]] StreamId streamId() const;
    [[nodiscard]] uint64_t epoch() const;
    [[nodiscard]] const std::vector<TimelineChunk>& timeline() const;

    void clear();
    void compact();
    void dropFrontFrames(int frames);

    void set(const AudioBuffer& buffer, std::span<const TimelineChunk> timelineChunks, StreamId streamId,
             uint64_t epoch = 0);
    AppendResult append(const AudioBuffer& buffer, std::span<const TimelineChunk> timelineChunks, StreamId streamId,
                        uint64_t epoch = 0);
    AppendResult appendOrReplace(const AudioBuffer& buffer, std::span<const TimelineChunk> timelineChunks,
                                 StreamId streamId, uint64_t epoch = 0);

    template <typename Fn>
    WriteResult write(Fn&& writer)
    {
        const int queueOffsetStart = m_timelineBaseOffsetFrames + std::max(0, m_writeOffsetFrames);

        if(!hasData()) {
            return {.writtenFrames          = 0,
                    .queueOffsetStartFrames = queueOffsetStart,
                    .queueOffsetEndFrames   = queueOffsetStart};
        }

        const int physicalOffset  = m_bufferBaseOffsetFrames + std::max(0, m_writeOffsetFrames);
        const int remainingFrames = std::max(0, m_buffer.frameCount() - physicalOffset);
        const int writtenFrames   = std::clamp(std::forward<Fn>(writer)(m_buffer, physicalOffset), 0, remainingFrames);

        if(writtenFrames <= 0) {
            return {.writtenFrames          = 0,
                    .queueOffsetStartFrames = queueOffsetStart,
                    .queueOffsetEndFrames   = queueOffsetStart};
        }

        m_writeOffsetFrames += writtenFrames;
        const int queueOffsetEnd = m_timelineBaseOffsetFrames + std::max(0, m_writeOffsetFrames);

        return {.writtenFrames          = writtenFrames,
                .queueOffsetStartFrames = queueOffsetStart,
                .queueOffsetEndFrames   = queueOffsetEnd};
    }

private:
    static AudioBuffer sliceBufferFrames(const AudioBuffer& buffer, int frameOffset, int frameCount);
    static void discardTimelinePrefix(std::vector<TimelineChunk>& timelineChunks, int frames);
    static bool canMergeTimelineChunks(const TimelineChunk& prev, const TimelineChunk& next);
    static void appendTimelineChunkMerged(std::vector<TimelineChunk>& timelineChunks, const TimelineChunk& chunk);
    void appendTimeline(std::span<const TimelineChunk> timelineChunks, int frameCount);

    AudioBuffer m_buffer;
    std::vector<TimelineChunk> m_timeline;

    int m_writeOffsetFrames;
    int m_bufferBaseOffsetFrames;
    int m_timelineBaseOffsetFrames;

    StreamId m_streamId;
    uint64_t m_epoch;
};
} // namespace Fooyin
