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

#include <core/engine/dsp/processingbuffer.h>
#include <core/engine/pipeline/timedaudiofifo.h>

#include <array>
#include <span>
#include <vector>

namespace Fooyin {
/*!
 * Mixer output queue that preserves source timeline metadata.
 *
 * `AudioMixer` commits rendered interleaved F64 samples and associated timeline
 * segments here, while pipeline consumers drain frame windows from it on demand.
 */
class MixerOutput
{
public:
    MixerOutput();

    struct CommitPlan
    {
        int outRate{0};
        int outChannels{0};
        int requestedFrames{0};
        StreamId pendingStreamToStartId{InvalidStreamId};
    };

    struct CycleState
    {
        bool startedPendingStream{false};
        bool hasActiveStream{false};
        bool dspBuffering{false};
        int producedFrames{0};
        StreamId trackedPrimaryStreamId{InvalidStreamId};
    };

    void clear();
    [[nodiscard]] int queuedFrames() const;

    void commitMixed(const CommitPlan& plan, const CycleState& state, std::span<const double> mixedInterleaved,
                     std::span<const AudioMixer::TimelineSegment> outputTimeline);

    int drain(ProcessingBuffer& output, int requestedFrames, int outChannels,
              std::vector<AudioMixer::TimelineSegment>& consumedTimeline);

private:
    TimedAudioFifo m_queue;
    std::vector<TimedAudioFifo::TimelineChunk> m_timelineScratch;
    AudioBuffer m_silenceScratch;
    std::array<TimedAudioFifo::TimelineChunk, 1> m_silenceTimeline;
};
} // namespace Fooyin
