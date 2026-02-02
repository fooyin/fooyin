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

#include "buffereddspstage.h"
#include "outputpump.h"

#include <core/engine/audiooutput.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace Fooyin {
/*!
 * Output-path state holder for pending master output and backend writes.
 *
 * Owns:
 * - active `AudioOutput` backend instance and capability flags,
 * - queued post-master output (`BufferedDspStage`),
 * - helper logic for pending drain policy and underrun conceal writes.
 */
class PipelineOutput
{
public:
    PipelineOutput();

    [[nodiscard]] AudioOutput* output() const;
    void setOutput(std::unique_ptr<AudioOutput> output);

    [[nodiscard]] bool isOutputInitialized() const;
    void setOutputInitialized(bool initialized);

    [[nodiscard]] bool outputSupportsVolume() const;
    void setOutputSupportsVolume(bool supportsVolume);

    [[nodiscard]] int bufferFrames() const;
    void setBufferFrames(int frames);

    [[nodiscard]] AudioBuffer& coalescedOutputBuffer();
    //! Convert/coalesce processed chunks into pending output queue.
    [[nodiscard]] BufferedDspStage::QueueResult queueProcessedOutput(const ProcessingBufferList& chunks,
                                                                     const AudioFormat& outputFormat, bool dither,
                                                                     StreamId streamId, uint64_t epoch);
    //! Replace pending output with a single buffer and stream context.
    void setPendingOutput(const AudioBuffer& buffer, StreamId streamId, uint64_t epoch);

    [[nodiscard]] bool hasPendingOutput() const;
    [[nodiscard]] int pendingOutputFrames() const;
    [[nodiscard]] int pendingWriteOffsetFrames() const;
    [[nodiscard]] StreamId pendingStreamId() const;
    [[nodiscard]] uint64_t pendingEpoch() const;
    [[nodiscard]] const std::vector<TimedAudioFifo::TimelineChunk>& pendingTimeline() const;

    void clearPendingOutput();
    void compactPendingOutput();
    void dropPendingFrontFrames(int frames);
    TimedAudioFifo::AppendResult appendPendingOutput(const AudioBuffer& buffer,
                                                     std::span<const TimedAudioFifo::TimelineChunk> timelineChunks,
                                                     StreamId streamId, uint64_t epoch);

    template <typename Fn>
    OutputPump::PendingDrainResult drainPending(Fn&& writer)
    {
        return m_outputPump.drainPending(m_masterOutputStage, std::forward<Fn>(writer));
    }
    template <typename Fn>
    TimedAudioFifo::WriteResult writePendingOutput(Fn&& writer)
    {
        return m_masterOutputStage.writePendingOutput(std::forward<Fn>(writer));
    }

    //! Write buffer frames to backend and optionally start output first.
    int writeOutputBufferFrames(const AudioBuffer& buffer, int frameOffset, bool shouldStartOutput);
    //! Write short repeated-frame conceal burst to backend.
    int writeUnderrunConcealFrames(int maxFrames, const AudioFormat& outputFormat, bool shouldStartOutput);

    //! Reset pending queue and optionally clear underrun conceal seed state.
    void resetPendingState(bool clearLastOutputFrame);

    [[nodiscard]] std::chrono::milliseconds writeBackoff(const AudioFormat& outputFormat,
                                                         const OutputState& outputState) const;

private:
    OutputPump m_outputPump;
    BufferedDspStage m_masterOutputStage;
    std::unique_ptr<AudioOutput> m_output;

    AudioBuffer m_coalescedOutputBuffer;
    AudioBuffer m_underrunConcealBuffer;

    std::vector<std::byte> m_lastOutputFrameBytes;
    std::vector<std::byte> m_underrunConcealSeedFrameBytes;

    std::atomic<bool> m_outputInitialized;
    bool m_outputSupportsVolume;

    size_t m_underrunConcealPreparedBytes;
    bool m_hasLastOutputFrame;
    int m_bufferFrames;
};
} // namespace Fooyin
