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

#include "core/engine/pipeline/audiopipeline.h"

#include <core/engine/enginedefs.h>

namespace Fooyin {
class FYCORE_EXPORT PositionCoordinator
{
public:
    struct PreparedCrossfadeContext
    {
        bool armed{false};
        StreamId streamId{InvalidStreamId};
        uint64_t bufferedAtArmMs{0};
        uint64_t boundaryLeadMs{0};
        bool boundarySignalled{false};
    };

    struct Input
    {
        Engine::PlaybackState playbackState{Engine::PlaybackState::Stopped};
        bool seekInProgress{false};
        bool decoderTimerActive{false};
        int decoderLowWatermarkMs{0};

        StreamId streamId{InvalidStreamId};
        AudioStream::State streamState{AudioStream::State::Stopped};
        bool streamEndOfInput{false};
        uint64_t streamBufferedDurationMs{0};
        uint64_t streamPositionMs{0};

        uint64_t streamToTrackOriginMs{0};
        uint64_t trackOffsetMs{0};

        uint64_t pipelineDelayMs{0};
        double delayToSourceScale{1.0};
        PipelineStatus pipelineStatus;

        PreparedCrossfadeContext preparedCrossfade;
    };

    struct Output
    {
        bool shouldEnsureDecodeTimer{false};
        bool positionAvailable{false};
        bool discontinuity{false};
        bool emitNow{false};
        uint64_t relativePosMs{0};
        bool pendingWithoutMappedAudio{false};
        bool hasRenderedSegment{false};
        bool preparedCrossfadeArmed{false};
        uint64_t trackEndingPosMs{0};
        bool boundaryFallbackReached{false};
        bool evaluateTrackEnding{false};
    };

    PositionCoordinator();

    void reset();
    void resetContinuity();
    void clearGaplessHold();
    void setGaplessHold(StreamId streamId);
    void notePublishedDiscontinuity(uint64_t sourcePositionMs);

    [[nodiscard]] Output evaluate(const Input& input);

private:
    bool m_lastSourcePositionValid;
    uint64_t m_lastSourcePositionMs;
    bool m_holdGaplessPositionUntilRendered;
    StreamId m_gaplessHoldStreamId;
};
} // namespace Fooyin
