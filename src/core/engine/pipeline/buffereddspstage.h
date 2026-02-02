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

#include <span>
#include <utility>
#include <vector>

namespace Fooyin {
/*!
 * Shared helper for buffered DSP output stages:
 * - coalesces processed chunks into an output-format buffer,
 * - stores queued output and timeline metadata,
 * - provides write/compact operations for downstream pumps.
 */
class FYCORE_EXPORT BufferedDspStage
{
public:
    using TimelineChunk = TimedAudioFifo::TimelineChunk;

    struct QueueResult
    {
        int appendedFrames{0};
        bool coalesceFailed{false};
        bool zeroFrameCoalesce{false};
        bool formatReset{false};
    };

    [[nodiscard]] bool hasPendingOutput() const;
    [[nodiscard]] int pendingOutputFrames() const;
    [[nodiscard]] int pendingWriteOffsetFrames() const;
    [[nodiscard]] StreamId pendingStreamId() const;
    [[nodiscard]] uint64_t pendingEpoch() const;
    [[nodiscard]] const std::vector<TimelineChunk>& pendingTimeline() const;

    void clearPendingOutput();
    void compactPendingOutput();
    void dropPendingFrontFrames(int frames);

    TimedAudioFifo::AppendResult appendPendingOutput(const AudioBuffer& buffer, StreamId streamId);
    TimedAudioFifo::AppendResult appendPendingOutput(const AudioBuffer& buffer,
                                                     std::span<const TimelineChunk> timelineChunks, StreamId streamId);
    TimedAudioFifo::AppendResult appendPendingOutput(const AudioBuffer& buffer,
                                                     std::span<const TimelineChunk> timelineChunks, StreamId streamId,
                                                     uint64_t epoch);
    TimedAudioFifo::AppendResult appendPendingOutputOrReplace(const AudioBuffer& buffer,
                                                              std::span<const TimelineChunk> timelineChunks,
                                                              StreamId streamId);
    TimedAudioFifo::AppendResult appendPendingOutputOrReplace(const AudioBuffer& buffer,
                                                              std::span<const TimelineChunk> timelineChunks,
                                                              StreamId streamId, uint64_t epoch);
    void setPendingOutput(const AudioBuffer& buffer, std::span<const TimelineChunk> timelineChunks, StreamId streamId);
    void setPendingOutput(const AudioBuffer& buffer, std::span<const TimelineChunk> timelineChunks, StreamId streamId,
                          uint64_t epoch);

    template <typename Fn>
    TimedAudioFifo::WriteResult writePendingOutput(Fn&& writer)
    {
        return m_pendingOutput.write(std::forward<Fn>(writer));
    }

    QueueResult queueProcessedOutput(const ProcessingBufferList& chunks, const AudioFormat& outputFormat, bool dither,
                                     StreamId streamId, uint64_t timelineEpoch = 0);

private:
    AudioBuffer m_coalescedOutputBuffer;
    TimedAudioFifo m_pendingOutput;
    std::vector<TimelineChunk> m_timelineScratch;
};
} // namespace Fooyin
