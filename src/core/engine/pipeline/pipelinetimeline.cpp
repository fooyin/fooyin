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

#include "pipelinetimeline.h"

#include <limits>

namespace Fooyin {
PipelineTimeline::PipelineTimeline()
    : m_position{0}
    , m_positionIsMapped{false}
    , m_renderedSegmentValid{false}
    , m_renderedSegmentStreamId{InvalidStreamId}
    , m_renderedSegmentStartMs{0}
    , m_renderedSegmentEndMs{0}
    , m_renderedSegmentOutputFrames{0}
    , m_playbackDelayMs{0}
    , m_playbackDelayToTrackScale{1.0}
    , m_bufferUnderrun{false}
    , m_cycleMappedPositionValid{false}
    , m_cycleMappedPositionMs{0}
    , m_cycleRenderedSegmentValid{false}
    , m_cycleRenderedSegmentStreamId{InvalidStreamId}
    , m_cycleRenderedSegmentEpoch{0}
    , m_cycleRenderedSegmentStartMs{0}
    , m_cycleRenderedSegmentEndMs{0}
    , m_cycleRenderedSegmentOutputFrames{0}
    , m_cycleInputPrimaryStreamId{InvalidStreamId}
    , m_timelineEpoch{1}
    , m_timelineEpochAnchorPosMs{0}
    , m_timelineEpochOffsetMs{0}
    , m_timelineEpochOffsetResolved{true}
    , m_observedMasterRateScale{1.0}
    , m_prevMasterOutputStartMs{0}
    , m_prevMasterOutputFrames{0}
    , m_hasMasterRateObservation{false}
{ }

void PipelineTimeline::clearRenderedSegmentSnapshot()
{
    m_renderedSegmentValid.store(false, std::memory_order_relaxed);
    m_renderedSegmentStreamId.store(InvalidStreamId, std::memory_order_relaxed);
    m_renderedSegmentStartMs.store(0, std::memory_order_relaxed);
    m_renderedSegmentEndMs.store(0, std::memory_order_relaxed);
    m_renderedSegmentOutputFrames.store(0, std::memory_order_relaxed);
}

void PipelineTimeline::clearMappedPositionState(std::optional<uint64_t> positionMs)
{
    if(positionMs.has_value()) {
        m_position.store(*positionMs, std::memory_order_relaxed);
    }

    m_positionIsMapped.store(false, std::memory_order_relaxed);
    clearRenderedSegmentSnapshot();
    resetCycleRenderedPosition();
}

void PipelineTimeline::resetPlaybackDelayState(bool resetScale)
{
    m_playbackDelayMs.store(0, std::memory_order_relaxed);

    if(resetScale) {
        m_playbackDelayToTrackScale.store(1.0, std::memory_order_relaxed);
    }
}

void PipelineTimeline::resetCycleRenderedPosition()
{
    m_cycleMappedPositionValid         = false;
    m_cycleMappedPositionMs            = 0;
    m_cycleRenderedSegmentValid        = false;
    m_cycleRenderedSegmentStreamId     = InvalidStreamId;
    m_cycleRenderedSegmentEpoch        = 0;
    m_cycleRenderedSegmentStartMs      = 0;
    m_cycleRenderedSegmentEndMs        = 0;
    m_cycleRenderedSegmentOutputFrames = 0;
    m_cycleInputPrimaryStreamId        = InvalidStreamId;
}

void PipelineTimeline::resetMasterRateObservation()
{
    m_observedMasterRateScale  = 1.0;
    m_prevMasterOutputStartMs  = 0;
    m_prevMasterOutputFrames   = 0;
    m_hasMasterRateObservation = false;
}

void PipelineTimeline::beginTimelineEpoch(uint64_t anchorPosMs)
{
    uint64_t epoch = m_timelineEpoch.load(std::memory_order_relaxed);

    if(epoch == std::numeric_limits<uint64_t>::max()) {
        epoch = 1;
    }
    else {
        ++epoch;
    }

    m_timelineEpoch.store(epoch, std::memory_order_relaxed);

    m_timelineEpochAnchorPosMs    = anchorPosMs;
    m_timelineEpochOffsetMs       = 0;
    m_timelineEpochOffsetResolved = false;
}

uint64_t PipelineTimeline::positionMs() const
{
    return m_position.load(std::memory_order_relaxed);
}

void PipelineTimeline::setPositionMs(uint64_t positionMs)
{
    m_position.store(positionMs, std::memory_order_relaxed);
}

bool PipelineTimeline::positionIsMapped() const
{
    return m_positionIsMapped.load(std::memory_order_relaxed);
}

void PipelineTimeline::setPositionIsMapped(bool mapped)
{
    m_positionIsMapped.store(mapped, std::memory_order_relaxed);
}

PipelineTimeline::RenderedSegment PipelineTimeline::renderedSegment() const
{
    return {.valid        = m_renderedSegmentValid.load(std::memory_order_relaxed),
            .streamId     = m_renderedSegmentStreamId.load(std::memory_order_relaxed),
            .startMs      = m_renderedSegmentStartMs.load(std::memory_order_relaxed),
            .endMs        = m_renderedSegmentEndMs.load(std::memory_order_relaxed),
            .outputFrames = m_renderedSegmentOutputFrames.load(std::memory_order_relaxed)};
}

void PipelineTimeline::setRenderedSegment(const RenderedSegment& segment)
{
    if(!segment.valid) {
        clearRenderedSegmentSnapshot();
        return;
    }

    m_renderedSegmentValid.store(true, std::memory_order_relaxed);
    m_renderedSegmentStreamId.store(segment.streamId, std::memory_order_relaxed);
    m_renderedSegmentStartMs.store(segment.startMs, std::memory_order_relaxed);
    m_renderedSegmentEndMs.store(segment.endMs, std::memory_order_relaxed);
    m_renderedSegmentOutputFrames.store(segment.outputFrames, std::memory_order_relaxed);
}

uint64_t PipelineTimeline::playbackDelayMs() const
{
    return m_playbackDelayMs.load(std::memory_order_relaxed);
}

void PipelineTimeline::setPlaybackDelayMs(uint64_t playbackDelayMs)
{
    m_playbackDelayMs.store(playbackDelayMs, std::memory_order_relaxed);
}

double PipelineTimeline::playbackDelayToTrackScale() const
{
    return m_playbackDelayToTrackScale.load(std::memory_order_relaxed);
}

void PipelineTimeline::setPlaybackDelayToTrackScale(double scale)
{
    m_playbackDelayToTrackScale.store(scale, std::memory_order_relaxed);
}

bool PipelineTimeline::bufferUnderrun() const
{
    return m_bufferUnderrun.load(std::memory_order_relaxed);
}

void PipelineTimeline::setBufferUnderrun(bool underrun)
{
    m_bufferUnderrun.store(underrun, std::memory_order_relaxed);
}

bool PipelineTimeline::cycleMappedPositionValid() const
{
    return m_cycleMappedPositionValid;
}

uint64_t PipelineTimeline::cycleMappedPositionMs() const
{
    return m_cycleMappedPositionMs;
}

void PipelineTimeline::setCycleMappedPosition(uint64_t positionMs)
{
    m_cycleMappedPositionValid = true;
    m_cycleMappedPositionMs    = positionMs;
}

PipelineTimeline::CycleRenderedSegment PipelineTimeline::cycleRenderedSegment() const
{
    return {.valid        = m_cycleRenderedSegmentValid,
            .streamId     = m_cycleRenderedSegmentStreamId,
            .epoch        = m_cycleRenderedSegmentEpoch,
            .startMs      = m_cycleRenderedSegmentStartMs,
            .endMs        = m_cycleRenderedSegmentEndMs,
            .outputFrames = m_cycleRenderedSegmentOutputFrames};
}

void PipelineTimeline::setCycleRenderedSegment(const CycleRenderedSegment& segment)
{
    m_cycleRenderedSegmentValid        = segment.valid;
    m_cycleRenderedSegmentStreamId     = segment.streamId;
    m_cycleRenderedSegmentEpoch        = segment.epoch;
    m_cycleRenderedSegmentStartMs      = segment.startMs;
    m_cycleRenderedSegmentEndMs        = segment.endMs;
    m_cycleRenderedSegmentOutputFrames = segment.outputFrames;
}

StreamId PipelineTimeline::cycleInputPrimaryStreamId() const
{
    return m_cycleInputPrimaryStreamId;
}

void PipelineTimeline::setCycleInputPrimaryStreamId(StreamId streamId)
{
    m_cycleInputPrimaryStreamId = streamId;
}

uint64_t PipelineTimeline::timelineEpoch() const
{
    return m_timelineEpoch.load(std::memory_order_relaxed);
}

uint64_t PipelineTimeline::timelineEpochAnchorPosMs() const
{
    return m_timelineEpochAnchorPosMs;
}

int64_t PipelineTimeline::timelineEpochOffsetMs() const
{
    return m_timelineEpochOffsetMs;
}

void PipelineTimeline::setTimelineEpochOffsetMs(int64_t offsetMs, bool resolved)
{
    m_timelineEpochOffsetMs       = offsetMs;
    m_timelineEpochOffsetResolved = resolved;
}

bool PipelineTimeline::timelineEpochOffsetResolved() const
{
    return m_timelineEpochOffsetResolved;
}

double PipelineTimeline::observedMasterRateScale() const
{
    return m_observedMasterRateScale;
}

void PipelineTimeline::setObservedMasterRateScale(double scale)
{
    m_observedMasterRateScale = scale;
}

uint64_t PipelineTimeline::prevMasterOutputStartMs() const
{
    return m_prevMasterOutputStartMs;
}

void PipelineTimeline::setPrevMasterOutputStartMs(uint64_t value)
{
    m_prevMasterOutputStartMs = value;
}

int PipelineTimeline::prevMasterOutputFrames() const
{
    return m_prevMasterOutputFrames;
}

void PipelineTimeline::setPrevMasterOutputFrames(int frames)
{
    m_prevMasterOutputFrames = frames;
}

bool PipelineTimeline::hasMasterRateObservation() const
{
    return m_hasMasterRateObservation;
}

void PipelineTimeline::setHasMasterRateObservation(bool hasObservation)
{
    m_hasMasterRateObservation = hasObservation;
}
} // namespace Fooyin
