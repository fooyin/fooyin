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

#include "positioncoordinator.h"

#include "core/engine/enginehelpers.h"

constexpr uint64_t MaxContinuousPositionJumpMs = 750;
constexpr uint64_t MaxBackwardDriftToleranceMs = 50;

namespace Fooyin {
PositionCoordinator::PositionCoordinator()
    : m_lastSourcePositionValid{false}
    , m_lastSourcePositionMs{0}
    , m_holdGaplessPositionUntilRendered{false}
    , m_gaplessHoldStreamId{InvalidStreamId}
{ }

void PositionCoordinator::reset()
{
    m_lastSourcePositionValid          = false;
    m_lastSourcePositionMs             = 0;
    m_holdGaplessPositionUntilRendered = false;
    m_gaplessHoldStreamId              = InvalidStreamId;
}

void PositionCoordinator::resetContinuity()
{
    m_lastSourcePositionValid = false;
    m_lastSourcePositionMs    = 0;
}

void PositionCoordinator::clearGaplessHold()
{
    m_holdGaplessPositionUntilRendered = false;
    m_gaplessHoldStreamId              = InvalidStreamId;
}

void PositionCoordinator::setGaplessHold(const StreamId streamId)
{
    m_holdGaplessPositionUntilRendered = streamId != InvalidStreamId;
    m_gaplessHoldStreamId              = m_holdGaplessPositionUntilRendered ? streamId : InvalidStreamId;
}

void PositionCoordinator::notePublishedDiscontinuity(const uint64_t sourcePositionMs)
{
    m_lastSourcePositionMs    = sourcePositionMs;
    m_lastSourcePositionValid = true;
}

PositionCoordinator::Output PositionCoordinator::evaluate(const Input& input)
{
    Output output;

    const bool stoppedPlayback = input.playbackState == Engine::PlaybackState::Stopped;
    const bool noActiveStream  = input.streamId == InvalidStreamId;

    if(stoppedPlayback || input.seekInProgress || noActiveStream) {
        return output;
    }

    const bool playing = input.playbackState == Engine::PlaybackState::Playing;
    const bool stateBlocksDecode
        = input.streamState == AudioStream::State::Pending || input.streamState == AudioStream::State::Paused;
    const auto decodeLowWatermarkMs = static_cast<uint64_t>(std::max(1, input.decoderLowWatermarkMs));
    const bool needsDecodeTimer = playing && !input.decoderTimerActive && !stateBlocksDecode && !input.streamEndOfInput
                               && input.streamBufferedDurationMs < decodeLowWatermarkMs;

    if(needsDecodeTimer) {
        output.shouldEnsureDecodeTimer = true;
    }

    const bool mappedForActiveStream
        = input.pipelineStatus.renderedSegment.valid && input.pipelineStatus.renderedSegment.streamId == input.streamId;

    if(m_holdGaplessPositionUntilRendered && m_gaplessHoldStreamId == input.streamId) {
        const bool hasRenderedProgress = mappedForActiveStream && input.pipelineStatus.renderedSegment.outputFrames > 0;
        if(!hasRenderedProgress) {
            output.positionAvailable         = true;
            output.discontinuity             = true;
            output.emitNow                   = true;
            output.relativePosMs             = 0;
            m_lastSourcePositionMs           = 0;
            m_lastSourcePositionValid        = true;
            output.pendingWithoutMappedAudio = false;
            output.hasRenderedSegment        = mappedForActiveStream;
            output.preparedCrossfadeArmed    = input.preparedCrossfade.armed;
            output.trackEndingPosMs          = 0;
            output.boundaryFallbackReached   = false;
            return output;
        }

        m_holdGaplessPositionUntilRendered = false;
        m_gaplessHoldStreamId              = InvalidStreamId;
    }

    output.preparedCrossfadeArmed = input.preparedCrossfade.armed;

    const bool pendingWithoutMappedAudio
        = !output.preparedCrossfadeArmed && input.streamState == AudioStream::State::Pending && !mappedForActiveStream;
    output.pendingWithoutMappedAudio = pendingWithoutMappedAudio;
    output.hasRenderedSegment        = mappedForActiveStream;

    uint64_t sourcePosMs = input.streamPositionMs;
    if(output.pendingWithoutMappedAudio) {
        sourcePosMs = 0;
    }

    if(output.hasRenderedSegment) {
        const uint64_t mappedSourcePosMs = input.pipelineStatus.renderedSegment.sourceEndMs;

        if(mappedSourcePosMs <= input.streamPositionMs) {
            sourcePosMs = mappedSourcePosMs;
        }
        else {
            const auto expectedGapMs = static_cast<uint64_t>(
                std::llround(static_cast<double>(input.pipelineDelayMs) * input.delayToSourceScale));
            const uint64_t maxAllowedGapMs = expectedGapMs + std::max<uint64_t>(1000, MaxContinuousPositionJumpMs);
            const uint64_t aheadGapMs      = mappedSourcePosMs - input.streamPositionMs;

            if(aheadGapMs <= maxAllowedGapMs) {
                sourcePosMs = mappedSourcePosMs;
            }
            else {
                output.hasRenderedSegment = false;
            }
        }
    }

    uint64_t relativePosMs = relativeTrackPositionMs(sourcePosMs, input.streamToTrackOriginMs, input.trackOffsetMs);

    const bool shouldClampToLastKnown
        = m_lastSourcePositionValid
       && (output.pendingWithoutMappedAudio || (!output.hasRenderedSegment && !output.preparedCrossfadeArmed));
    if(shouldClampToLastKnown) {
        relativePosMs = std::min(relativePosMs, m_lastSourcePositionMs);
    }

    output.positionAvailable   = true;
    output.relativePosMs       = relativePosMs;
    output.discontinuity       = false;
    output.emitNow             = false;
    output.evaluateTrackEnding = true;

    if(m_lastSourcePositionValid) {
        const bool backwardDrift = relativePosMs < m_lastSourcePositionMs
                                && (m_lastSourcePositionMs - relativePosMs) > MaxBackwardDriftToleranceMs;
        const bool forwardJump = relativePosMs > m_lastSourcePositionMs
                              && (relativePosMs - m_lastSourcePositionMs) > MaxContinuousPositionJumpMs;

        if(backwardDrift || forwardJump) {
            output.discontinuity = true;
            output.emitNow       = true;
        }
    }

    m_lastSourcePositionMs    = relativePosMs;
    m_lastSourcePositionValid = true;

    const uint64_t decoderRelativePosMs
        = output.pendingWithoutMappedAudio
            ? relativePosMs
            : relativeTrackPositionMs(input.streamPositionMs, input.streamToTrackOriginMs, input.trackOffsetMs);
    output.trackEndingPosMs = decoderRelativePosMs;
    if(output.hasRenderedSegment) {
        if(input.streamEndOfInput) {
            output.trackEndingPosMs = relativePosMs;
        }
        else {
            output.trackEndingPosMs = std::min(relativePosMs, decoderRelativePosMs);
        }
    }

    const bool evaluatingCrossfadeBoundaryFallback = output.preparedCrossfadeArmed
                                                  && !input.preparedCrossfade.boundarySignalled
                                                  && input.preparedCrossfade.streamId == input.streamId;
    if(evaluatingCrossfadeBoundaryFallback) {
        uint64_t consumedMs{0};
        if(input.preparedCrossfade.bufferedAtArmMs > input.streamBufferedDurationMs) {
            consumedMs = input.preparedCrossfade.bufferedAtArmMs - input.streamBufferedDurationMs;
        }
        output.boundaryFallbackReached = consumedMs >= input.preparedCrossfade.boundaryLeadMs;
    }

    return output;
}
} // namespace Fooyin
