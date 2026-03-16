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

#include "mixeroutput.h"

#include "mixertimeline.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <span>

namespace Fooyin {
MixerOutput::MixerOutput()
    : m_silenceTimeline{}
{ }

void MixerOutput::clear()
{
    m_queue.clear();
}

int MixerOutput::queuedFrames() const
{
    return m_queue.queuedFrames();
}

void MixerOutput::commitMixed(const CommitPlan& plan, const CycleState& state, std::span<const double> mixedInterleaved,
                              std::span<const AudioMixer::TimelineSegment> outputTimeline)
{
    if(state.producedFrames > 0 && plan.outChannels > 0 && plan.outRate > 0) {
        m_timelineScratch.clear();
        m_timelineScratch.reserve(outputTimeline.size());
        for(const auto& segment : outputTimeline) {
            if(segment.frames <= 0) {
                continue;
            }

            m_timelineScratch.push_back(TimedAudioFifo::TimelineChunk{
                .valid           = segment.valid,
                .startNs         = segment.startNs,
                .frameDurationNs = segment.frameDurationNs,
                .frames          = segment.frames,
                .sampleRate      = plan.outRate,
                .streamId        = segment.streamId,
            });
        }

        const AudioBuffer mixedBuffer{
            std::as_bytes(mixedInterleaved),
            AudioFormat{SampleFormat::F64, plan.outRate, plan.outChannels},
            0,
        };
        m_queue.appendOrReplace(mixedBuffer, m_timelineScratch, state.trackedPrimaryStreamId);
    }

    const bool canPadWithSilence         = !state.startedPendingStream && plan.pendingStreamToStartId == InvalidStreamId
                                        && state.hasActiveStream && state.producedFrames > 0 && !state.dspBuffering
                                        && plan.outChannels > 0 && plan.outRate > 0;
    const bool needsPadToRequestedFrames = m_queue.queuedFrames() < plan.requestedFrames;

    if(canPadWithSilence && needsPadToRequestedFrames) {
        const int padFrames = std::max(0, plan.requestedFrames - m_queue.queuedFrames());
        if(padFrames > 0) {
            const auto silenceBytes
                = static_cast<size_t>(padFrames) * static_cast<size_t>(plan.outChannels) * sizeof(double);
            const AudioFormat silenceFormat{SampleFormat::F64, plan.outRate, plan.outChannels};

            if(!m_silenceScratch.isValid() || m_silenceScratch.format() != silenceFormat) {
                m_silenceScratch = AudioBuffer{silenceFormat, 0};
            }
            else {
                m_silenceScratch.setStartTime(0);
            }

            m_silenceScratch.resize(silenceBytes);
            m_silenceScratch.fillSilence();

            m_silenceTimeline[0] = TimedAudioFifo::TimelineChunk{
                .valid           = false,
                .startNs         = 0,
                .frameDurationNs = 0,
                .frames          = padFrames,
                .sampleRate      = plan.outRate,
                .streamId        = state.trackedPrimaryStreamId,
            };
            m_queue.appendOrReplace(m_silenceScratch, m_silenceTimeline, state.trackedPrimaryStreamId);
        }
    }
}

int MixerOutput::drain(ProcessingBuffer& output, int requestedFrames, int outChannels,
                       std::vector<AudioMixer::TimelineSegment>& consumedTimeline)
{
    const int availableFrames = std::min(m_queue.queuedFrames(), std::max(0, requestedFrames));
    if(availableFrames <= 0 || outChannels <= 0) {
        output.resizeSamples(0);
        return 0;
    }

    const size_t returnedSamples = static_cast<size_t>(availableFrames) * static_cast<size_t>(outChannels);
    output.resizeSamples(returnedSamples);
    auto outSpan = output.data();

    const auto writeResult = m_queue.write([&](const AudioBuffer& buffer, int frameOffset) -> int {
        if(!buffer.isValid() || frameOffset < 0) {
            return 0;
        }

        const int available  = std::max(0, buffer.frameCount() - frameOffset);
        const int takeFrames = std::min(availableFrames, available);
        if(takeFrames <= 0) {
            return 0;
        }

        const int bytesPerFrame = buffer.format().bytesPerFrame();
        if(bytesPerFrame <= 0) {
            return 0;
        }

        const size_t byteOffset = static_cast<size_t>(frameOffset) * static_cast<size_t>(bytesPerFrame);
        const size_t byteCount  = static_cast<size_t>(takeFrames) * static_cast<size_t>(bytesPerFrame);
        const auto srcBytes     = buffer.constData();
        auto dstBytes           = std::as_writable_bytes(outSpan);
        if(byteOffset + byteCount > srcBytes.size() || byteCount > dstBytes.size()) {
            return 0;
        }

        std::memcpy(dstBytes.data(), srcBytes.data() + byteOffset, byteCount);
        return takeFrames;
    });

    const int returnedFrames = writeResult.writtenFrames;
    if(returnedFrames <= 0) {
        output.resizeSamples(0);
        return 0;
    }

    const size_t actualSamples = static_cast<size_t>(returnedFrames) * static_cast<size_t>(outChannels);
    if(actualSamples < returnedSamples) {
        output.resizeSamples(actualSamples);
        outSpan = output.data();
    }

    Timeline::consumeRange(m_queue.timeline(), writeResult.queueOffsetStartFrames, returnedFrames, m_queue.streamId(),
                           consumedTimeline);

    uint64_t startNs{0};
    if(Timeline::startNs(consumedTimeline, startNs)) {
        output.setStartTimeNs(startNs);
    }
    else {
        output.setStartTimeNs(0);
    }

    uint64_t weightedDurationNs{0};
    int weightedFrames{0};

    for(const auto& segment : consumedTimeline) {
        if(!segment.valid || segment.frameDurationNs == 0 || segment.frames <= 0) {
            continue;
        }

        const uint64_t segmentFrames = static_cast<uint64_t>(segment.frames);
        const uint64_t segmentNs     = segment.frameDurationNs * segmentFrames;

        if(weightedDurationNs > (std::numeric_limits<uint64_t>::max() - segmentNs)) {
            weightedDurationNs = std::numeric_limits<uint64_t>::max();
        }
        else {
            weightedDurationNs += segmentNs;
        }

        if(weightedFrames > std::numeric_limits<int>::max() - segment.frames) {
            weightedFrames = std::numeric_limits<int>::max();
        }
        else {
            weightedFrames += segment.frames;
        }
    }

    if(weightedFrames > 0 && weightedDurationNs > 0) {
        output.setSourceFrameDurationNs(weightedDurationNs / static_cast<uint64_t>(weightedFrames));
    }
    else {
        output.setSourceFrameDurationNs(0);
    }

    if(m_queue.writeOffsetFrames() > 0 && (!m_queue.hasData() || m_queue.writeOffsetFrames() >= returnedFrames)) {
        m_queue.compact();
    }

    return returnedFrames;
}
} // namespace Fooyin
