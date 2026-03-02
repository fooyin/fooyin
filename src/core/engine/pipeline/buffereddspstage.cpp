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

#include "buffereddspstage.h"

#include "chunktimelineassembler.h"
#include "core/engine/audioconverter.h"
#include <utils/timeconstants.h>

#include <cstring>
#include <limits>
#include <span>

namespace Fooyin {
bool BufferedDspStage::hasPendingOutput() const
{
    return m_pendingOutput.hasData();
}

int BufferedDspStage::pendingOutputFrames() const
{
    return m_pendingOutput.queuedFrames();
}

int BufferedDspStage::pendingWriteOffsetFrames() const
{
    return m_pendingOutput.writeOffsetFrames();
}

StreamId BufferedDspStage::pendingStreamId() const
{
    return m_pendingOutput.streamId();
}

uint64_t BufferedDspStage::pendingEpoch() const
{
    return m_pendingOutput.epoch();
}

const std::vector<BufferedDspStage::TimelineChunk>& BufferedDspStage::pendingTimeline() const
{
    return m_pendingOutput.timeline();
}

void BufferedDspStage::clearPendingOutput()
{
    m_pendingOutput.clear();
}

void BufferedDspStage::compactPendingOutput()
{
    m_pendingOutput.compact();
}

void BufferedDspStage::dropPendingFrontFrames(int frames)
{
    m_pendingOutput.dropFrontFrames(frames);
}

TimedAudioFifo::AppendResult BufferedDspStage::appendPendingOutput(const AudioBuffer& buffer, const StreamId streamId)
{
    return appendPendingOutput(buffer, {}, streamId);
}

TimedAudioFifo::AppendResult BufferedDspStage::appendPendingOutput(const AudioBuffer& buffer,
                                                                   std::span<const TimelineChunk> timelineChunks,
                                                                   const StreamId streamId)
{
    return m_pendingOutput.append(buffer, timelineChunks, streamId, m_pendingOutput.epoch());
}

TimedAudioFifo::AppendResult BufferedDspStage::appendPendingOutput(const AudioBuffer& buffer,
                                                                   std::span<const TimelineChunk> timelineChunks,
                                                                   const StreamId streamId, uint64_t epoch)
{
    return m_pendingOutput.append(buffer, timelineChunks, streamId, epoch);
}

TimedAudioFifo::AppendResult
BufferedDspStage::appendPendingOutputOrReplace(const AudioBuffer& buffer, std::span<const TimelineChunk> timelineChunks,
                                               const StreamId streamId)
{
    return m_pendingOutput.appendOrReplace(buffer, timelineChunks, streamId, m_pendingOutput.epoch());
}

TimedAudioFifo::AppendResult
BufferedDspStage::appendPendingOutputOrReplace(const AudioBuffer& buffer, std::span<const TimelineChunk> timelineChunks,
                                               const StreamId streamId, uint64_t epoch)
{
    return m_pendingOutput.appendOrReplace(buffer, timelineChunks, streamId, epoch);
}

void BufferedDspStage::setPendingOutput(const AudioBuffer& buffer, std::span<const TimelineChunk> timelineChunks,
                                        const StreamId streamId)
{
    m_pendingOutput.set(buffer, timelineChunks, streamId, m_pendingOutput.epoch());
}

void BufferedDspStage::setPendingOutput(const AudioBuffer& buffer, std::span<const TimelineChunk> timelineChunks,
                                        const StreamId streamId, uint64_t epoch)
{
    m_pendingOutput.set(buffer, timelineChunks, streamId, epoch);
}

BufferedDspStage::QueueResult BufferedDspStage::queueProcessedOutput(const ProcessingBufferList& chunks,
                                                                     const AudioFormat& outputFormat, bool dither,
                                                                     const StreamId streamId, uint64_t timelineEpoch)
{
    QueueResult result;

    if(!outputFormat.isValid()) {
        result.coalesceFailed = true;
        return result;
    }

    size_t validChunkCount{0};
    const ProcessingBuffer* singleChunk{nullptr};

    for(size_t i{0}; i < chunks.count(); ++i) {
        const auto* chunk = chunks.item(i);

        if(!chunk || !chunk->isValid() || chunk->frameCount() <= 0) {
            continue;
        }

        ++validChunkCount;

        if(validChunkCount == 1) {
            singleChunk = chunk;
            continue;
        }

        singleChunk = nullptr;
        break;
    }

    if(singleChunk && validChunkCount == 1) {
        const int frameCount     = singleChunk->frameCount();
        const auto outputBytes64 = outputFormat.bytesForFrames(frameCount);

        if(frameCount <= 0 || outputBytes64 == 0 || outputBytes64 > std::numeric_limits<size_t>::max()) {
            result.zeroFrameCoalesce = true;
            return result;
        }

        const auto outputBytes     = static_cast<size_t>(outputBytes64);
        const uint64_t startTimeMs = singleChunk->startTimeNs() / Time::NsPerMs;

        if(!m_coalescedOutputBuffer.isValid() || m_coalescedOutputBuffer.format() != outputFormat) {
            m_coalescedOutputBuffer = AudioBuffer{outputFormat, startTimeMs};
        }
        else {
            m_coalescedOutputBuffer.setStartTime(startTimeMs);
        }

        m_coalescedOutputBuffer.resize(outputBytes);

        if(singleChunk->format() == outputFormat) {
            const auto inputBytes = std::as_bytes(singleChunk->constData());

            if(inputBytes.size() < outputBytes) {
                result.coalesceFailed = true;
                return result;
            }

            std::memcpy(m_coalescedOutputBuffer.data(), inputBytes.data(), outputBytes);
        }
        else {
            const auto inputBytes = std::as_bytes(singleChunk->constData());

            if(!Audio::convert(singleChunk->format(), inputBytes.data(), outputFormat, m_coalescedOutputBuffer.data(),
                               frameCount, dither)) {
                result.coalesceFailed = true;
                return result;
            }
        }

        const auto fmt = singleChunk->format();
        m_timelineScratch.clear();

        if(fmt.isValid() && fmt.sampleRate() > 0) {
            const uint64_t srcFrameNs = singleChunk->sourceFrameDurationNs();
            m_timelineScratch.push_back(TimelineChunk{
                .valid           = srcFrameNs > 0,
                .startNs         = srcFrameNs > 0 ? singleChunk->startTimeNs() : 0,
                .frameDurationNs = srcFrameNs,
                .frames          = frameCount,
                .sampleRate      = fmt.sampleRate(),
                .streamId        = streamId,
                .epoch           = timelineEpoch,
            });
        }
    }
    else if(!Timeline::coalesceChunks(chunks, outputFormat, m_coalescedOutputBuffer, dither)) {
        result.coalesceFailed = true;
        return result;
    }

    if(m_coalescedOutputBuffer.frameCount() <= 0) {
        result.zeroFrameCoalesce = true;
        return result;
    }

    compactPendingOutput();

    const bool haveTimeline = (singleChunk && validChunkCount == 1U)
                                ? !m_timelineScratch.empty()
                                : Timeline::buildTimelineChunks(chunks, m_timelineScratch, streamId, timelineEpoch);
    const std::span<const TimelineChunk> timelineSpan
        = haveTimeline ? std::span<const TimelineChunk>{m_timelineScratch} : std::span<const TimelineChunk>{};

    const auto appendResult
        = m_pendingOutput.appendOrReplace(m_coalescedOutputBuffer, timelineSpan, streamId, timelineEpoch);
    result.appendedFrames = appendResult.appended ? appendResult.appendedFrames : 0;
    result.formatReset    = appendResult.formatMismatch;

    return result;
}
} // namespace Fooyin
