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

#include "pipelineoutput.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <span>

namespace Fooyin {
namespace {
size_t fillRepeatingFramePattern(std::byte* dst, size_t targetBytes, const std::span<const std::byte> frameBytes,
                                 size_t preparedBytes)
{
    if(!dst || frameBytes.empty() || frameBytes.size() > targetBytes) {
        return std::min(preparedBytes, targetBytes);
    }

    preparedBytes = std::min(preparedBytes, targetBytes);
    if(preparedBytes < frameBytes.size()) {
        std::memcpy(dst, frameBytes.data(), frameBytes.size());
        preparedBytes = frameBytes.size();
    }

    while(preparedBytes < targetBytes) {
        const size_t copyBytes = std::min(preparedBytes, targetBytes - preparedBytes);
        if(copyBytes == 0) {
            break;
        }
        std::memcpy(dst + static_cast<std::ptrdiff_t>(preparedBytes), dst, copyBytes);
        preparedBytes += copyBytes;
    }

    return preparedBytes;
}
} // namespace

PipelineOutput::PipelineOutput()
    : m_outputInitialized{false}
    , m_outputSupportsVolume{true}
    , m_underrunConcealPreparedBytes{0}
    , m_hasLastOutputFrame{false}
    , m_bufferFrames{0}
{ }

AudioOutput* PipelineOutput::output() const
{
    return m_output.get();
}

void PipelineOutput::setOutput(std::unique_ptr<AudioOutput> output)
{
    m_output = std::move(output);
}

bool PipelineOutput::isOutputInitialized() const
{
    return m_outputInitialized.load(std::memory_order_acquire);
}

void PipelineOutput::setOutputInitialized(bool initialized)
{
    m_outputInitialized.store(initialized, std::memory_order_release);
}

bool PipelineOutput::outputSupportsVolume() const
{
    return m_outputSupportsVolume;
}

void PipelineOutput::setOutputSupportsVolume(bool supportsVolume)
{
    m_outputSupportsVolume = supportsVolume;
}

int PipelineOutput::bufferFrames() const
{
    return m_bufferFrames;
}

void PipelineOutput::setBufferFrames(int frames)
{
    m_bufferFrames = frames;
}

AudioBuffer& PipelineOutput::coalescedOutputBuffer()
{
    return m_coalescedOutputBuffer;
}

BufferedDspStage::QueueResult PipelineOutput::queueProcessedOutput(const ProcessingBufferList& chunks,
                                                                   const AudioFormat& outputFormat, bool dither,
                                                                   const StreamId streamId, uint64_t epoch)
{
    return m_masterOutputStage.queueProcessedOutput(chunks, outputFormat, dither, streamId, epoch);
}

void PipelineOutput::setPendingOutput(const AudioBuffer& buffer, StreamId streamId, uint64_t epoch)
{
    m_masterOutputStage.setPendingOutput(buffer, {}, streamId, epoch);
}

bool PipelineOutput::hasPendingOutput() const
{
    return m_masterOutputStage.hasPendingOutput();
}

int PipelineOutput::pendingOutputFrames() const
{
    return m_masterOutputStage.pendingOutputFrames();
}

int PipelineOutput::pendingWriteOffsetFrames() const
{
    return m_masterOutputStage.pendingWriteOffsetFrames();
}

StreamId PipelineOutput::pendingStreamId() const
{
    return m_masterOutputStage.pendingStreamId();
}

uint64_t PipelineOutput::pendingEpoch() const
{
    return m_masterOutputStage.pendingEpoch();
}

const std::vector<TimedAudioFifo::TimelineChunk>& PipelineOutput::pendingTimeline() const
{
    return m_masterOutputStage.pendingTimeline();
}

void PipelineOutput::clearPendingOutput()
{
    m_masterOutputStage.clearPendingOutput();
}

void PipelineOutput::compactPendingOutput()
{
    m_masterOutputStage.compactPendingOutput();
}

void PipelineOutput::dropPendingFrontFrames(int frames)
{
    m_masterOutputStage.dropPendingFrontFrames(frames);
}

TimedAudioFifo::AppendResult
PipelineOutput::appendPendingOutput(const AudioBuffer& buffer,
                                    std::span<const TimedAudioFifo::TimelineChunk> timelineChunks, StreamId streamId,
                                    uint64_t epoch)
{
    return m_masterOutputStage.appendPendingOutput(buffer, timelineChunks, streamId, epoch);
}

int PipelineOutput::writeOutputBufferFrames(const AudioBuffer& buffer, int frameOffset, bool shouldStartOutput)
{
    if(!buffer.isValid() || frameOffset < 0 || !m_output || !m_output->initialised()) {
        return 0;
    }

    if(shouldStartOutput) {
        m_output->start();
    }

    const int totalFrames = buffer.frameCount();
    if(totalFrames <= 0 || frameOffset >= totalFrames) {
        return 0;
    }

    const int bytesPerFrame = buffer.format().bytesPerFrame();
    if(bytesPerFrame <= 0) {
        return 0;
    }

    const auto bytes     = buffer.constData();
    const auto startByte = static_cast<size_t>(frameOffset) * static_cast<size_t>(bytesPerFrame);
    if(startByte >= bytes.size()) {
        return 0;
    }

    const int remainingFrames = totalFrames - frameOffset;
    const int writtenFrames
        = std::clamp(m_output->write(bytes.subspan(startByte), remainingFrames), 0, remainingFrames);
    if(writtenFrames <= 0) {
        return 0;
    }

    const auto frameBytes         = static_cast<size_t>(bytesPerFrame);
    const auto lastFrameStartByte = startByte + (static_cast<size_t>(writtenFrames - 1) * frameBytes);

    if((lastFrameStartByte + frameBytes) <= bytes.size()) {
        m_lastOutputFrameBytes.resize(frameBytes);
        std::memcpy(m_lastOutputFrameBytes.data(), bytes.data() + static_cast<std::ptrdiff_t>(lastFrameStartByte),
                    frameBytes);
        m_hasLastOutputFrame = true;
    }

    return writtenFrames;
}

int PipelineOutput::writeUnderrunConcealFrames(int maxFrames, const AudioFormat& outputFormat, bool shouldStartOutput)
{
    if(maxFrames <= 0 || !outputFormat.isValid()) {
        return 0;
    }

    const int bytesPerFrame = outputFormat.bytesPerFrame();
    if(bytesPerFrame <= 0 || !m_hasLastOutputFrame
       || m_lastOutputFrameBytes.size() != static_cast<size_t>(bytesPerFrame)) {
        return 0;
    }

    const int clampedFrames = std::max(0, maxFrames);
    const auto concealBytes = outputFormat.bytesForFrames(clampedFrames);
    if(concealBytes == 0) {
        return 0;
    }

    if(!m_underrunConcealBuffer.isValid() || m_underrunConcealBuffer.format() != outputFormat) {
        m_underrunConcealBuffer        = AudioBuffer{outputFormat, 0};
        m_underrunConcealPreparedBytes = 0;
    }

    if(std::cmp_less(m_underrunConcealBuffer.byteCount(), concealBytes)) {
        m_underrunConcealBuffer.reserve(static_cast<size_t>(concealBytes));
    }

    const auto concealBytesCount = static_cast<size_t>(concealBytes);
    m_underrunConcealBuffer.resize(concealBytesCount);

    auto* dst = m_underrunConcealBuffer.data();
    if(!dst) {
        return 0;
    }

    if(std::cmp_not_equal(m_underrunConcealSeedFrameBytes.size(), bytesPerFrame)
       || !std::equal(m_underrunConcealSeedFrameBytes.begin(), m_underrunConcealSeedFrameBytes.end(),
                      m_lastOutputFrameBytes.begin())) {
        m_underrunConcealSeedFrameBytes = m_lastOutputFrameBytes;
        m_underrunConcealPreparedBytes  = 0;
    }

    m_underrunConcealPreparedBytes = fillRepeatingFramePattern(dst, concealBytesCount, m_underrunConcealSeedFrameBytes,
                                                               m_underrunConcealPreparedBytes);
    m_underrunConcealPreparedBytes = std::min(m_underrunConcealPreparedBytes, concealBytesCount);

    return writeOutputBufferFrames(m_underrunConcealBuffer, 0, shouldStartOutput);
}

void PipelineOutput::resetPendingState(bool clearLastOutputFrame)
{
    clearPendingOutput();
    if(clearLastOutputFrame) {
        m_hasLastOutputFrame = false;
        m_lastOutputFrameBytes.clear();
        m_underrunConcealSeedFrameBytes.clear();
        m_underrunConcealPreparedBytes = 0;
    }
}

std::chrono::milliseconds PipelineOutput::writeBackoff(const AudioFormat& outputFormat,
                                                       const OutputState& outputState) const
{
    return m_outputPump.writeBackoff(outputFormat, outputState);
}
} // namespace Fooyin
