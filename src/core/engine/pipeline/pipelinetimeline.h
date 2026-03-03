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

#include "audiostream.h"

#include <atomic>
#include <cstdint>
#include <optional>

namespace Fooyin {
/*!
 * Shared timeline/position state container for `AudioPipeline`.
 *
 * Stores atomically published playback snapshots (position, mapped status,
 * rendered segment, delay) plus per-cycle timeline bookkeeping used on the
 * audio thread (epoch anchor/offset and master-rate observation).
 */
class PipelineTimeline
{
public:
    struct RenderedSegment
    {
        bool valid{false};
        StreamId streamId{InvalidStreamId};
        uint64_t startMs{0};
        uint64_t endMs{0};
        int outputFrames{0};
    };

    struct CycleRenderedSegment
    {
        bool valid{false};
        StreamId streamId{InvalidStreamId};
        uint64_t epoch{0};
        uint64_t startMs{0};
        uint64_t endMs{0};
        int outputFrames{0};
    };

    PipelineTimeline();

    void clearRenderedSegmentSnapshot();
    void clearMappedPositionState(std::optional<uint64_t> positionMs = std::nullopt);

    void resetPlaybackDelayState(bool resetScale = true);
    void resetCycleRenderedPosition();
    void resetMasterRateObservation();

    void beginTimelineEpoch(uint64_t anchorPosMs);

    [[nodiscard]] uint64_t positionMs() const;
    void setPositionMs(uint64_t positionMs);

    [[nodiscard]] bool positionIsMapped() const;
    void setPositionIsMapped(bool mapped);

    [[nodiscard]] RenderedSegment renderedSegment() const;
    void setRenderedSegment(const RenderedSegment& segment);

    [[nodiscard]] uint64_t playbackDelayMs() const;
    void setPlaybackDelayMs(uint64_t playbackDelayMs);
    [[nodiscard]] uint64_t transitionPlaybackDelayMs() const;
    void setTransitionPlaybackDelayMs(uint64_t playbackDelayMs);

    [[nodiscard]] double playbackDelayToTrackScale() const;
    void setPlaybackDelayToTrackScale(double scale);

    [[nodiscard]] bool bufferUnderrun() const;
    void setBufferUnderrun(bool underrun);

    [[nodiscard]] bool cycleMappedPositionValid() const;
    [[nodiscard]] uint64_t cycleMappedPositionMs() const;
    void setCycleMappedPosition(uint64_t positionMs);

    [[nodiscard]] CycleRenderedSegment cycleRenderedSegment() const;
    void setCycleRenderedSegment(const CycleRenderedSegment& segment);

    [[nodiscard]] StreamId cycleInputPrimaryStreamId() const;
    void setCycleInputPrimaryStreamId(StreamId streamId);

    [[nodiscard]] uint64_t timelineEpoch() const;
    [[nodiscard]] uint64_t timelineEpochAnchorPosMs() const;
    [[nodiscard]] int64_t timelineEpochOffsetMs() const;
    //! Set epoch offset and whether offset was resolved from mapped data.
    void setTimelineEpochOffsetMs(int64_t offsetMs, bool resolved);
    [[nodiscard]] bool timelineEpochOffsetResolved() const;

    [[nodiscard]] double observedMasterRateScale() const;
    void setObservedMasterRateScale(double scale);

    [[nodiscard]] uint64_t prevMasterOutputStartMs() const;
    void setPrevMasterOutputStartMs(uint64_t value);

    [[nodiscard]] int prevMasterOutputFrames() const;
    void setPrevMasterOutputFrames(int frames);

    [[nodiscard]] bool hasMasterRateObservation() const;
    void setHasMasterRateObservation(bool hasObservation);

private:
    std::atomic<uint64_t> m_position;
    std::atomic<bool> m_positionIsMapped;
    std::atomic<bool> m_renderedSegmentValid;
    std::atomic<StreamId> m_renderedSegmentStreamId;
    std::atomic<uint64_t> m_renderedSegmentStartMs;
    std::atomic<uint64_t> m_renderedSegmentEndMs;
    std::atomic<int> m_renderedSegmentOutputFrames;
    std::atomic<uint64_t> m_playbackDelayMs;
    std::atomic<uint64_t> m_transitionPlaybackDelayMs;
    std::atomic<double> m_playbackDelayToTrackScale;
    std::atomic<bool> m_bufferUnderrun;

    bool m_cycleMappedPositionValid;
    uint64_t m_cycleMappedPositionMs;
    bool m_cycleRenderedSegmentValid;
    StreamId m_cycleRenderedSegmentStreamId;
    uint64_t m_cycleRenderedSegmentEpoch;
    uint64_t m_cycleRenderedSegmentStartMs;
    uint64_t m_cycleRenderedSegmentEndMs;
    int m_cycleRenderedSegmentOutputFrames;
    StreamId m_cycleInputPrimaryStreamId;
    std::atomic<uint64_t> m_timelineEpoch;
    uint64_t m_timelineEpochAnchorPosMs;
    int64_t m_timelineEpochOffsetMs;
    bool m_timelineEpochOffsetResolved;
    double m_observedMasterRateScale;
    uint64_t m_prevMasterOutputStartMs;
    int m_prevMasterOutputFrames;
    bool m_hasMasterRateObservation;
};
} // namespace Fooyin
