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

#include "playbacktransitioncoordinator.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace Fooyin {
namespace {
uint64_t saturatingAddWindow(const uint64_t lhs, const uint64_t rhs)
{
    return lhs > (std::numeric_limits<uint64_t>::max() - rhs) ? std::numeric_limits<uint64_t>::max() : lhs + rhs;
}
} // namespace

PlaybackTransitionCoordinator::PlaybackTransitionCoordinator()
    : m_autoTransitionMode{AutoTransitionMode::None}
    , m_trackEnding{false}
    , m_switchReady{false}
    , m_boundaryReached{false}
    , m_seekInProgress{false}
    , m_pendingSeekActive{false}
    , m_pendingInitialSeekActive{false}
    , m_pendingInitialSeekPositionMs{0}
    , m_pendingInitialSeekRequestId{0}
    , m_pendingInitialSeekTrackId{-1}
{ }

PlaybackTransitionCoordinator::TrackEndingResult
PlaybackTransitionCoordinator::evaluateTrackEnding(const TrackEndingInput& input)
{
    TrackEndingResult result;

    const bool durationBoundaryReached
        = input.durationBoundaryEnabled && input.durationMs > 0 && input.positionMs >= input.durationMs;

    if(!m_boundaryReached && durationBoundaryReached) {
        m_boundaryReached      = true;
        result.boundaryReached = true;
    }

    if(input.endOfInput && input.bufferEmpty) {
        m_trackEnding = true;
        m_switchReady = false;
        if(!m_boundaryReached) {
            m_boundaryReached      = true;
            result.boundaryReached = true;
        }
        clearAutoTransitionReadiness();
        result.endReached = true;
        return result;
    }

    if(!m_trackEnding && shouldSignalTrackEnding(input)) {
        m_trackEnding        = true;
        result.aboutToFinish = true;
    }

    // Cue-style logical segments may continue decoding on the same stream.
    // Treat duration boundary as a logical end even without decoder EOF.
    if(durationBoundaryReached && input.durationBoundaryEnabled) {
        result.endReached = true;
    }

    if(m_trackEnding && !m_switchReady && shouldSignalReadyToSwitch(input)) {
        m_switchReady        = true;
        result.readyToSwitch = true;
    }

    return result;
}

bool PlaybackTransitionCoordinator::inTimelineWindow(const TrackEndingInput& input, uint64_t windowMs)
{
    if(input.durationMs == 0) {
        return false;
    }

    const uint64_t triggerPos = (input.durationMs > windowMs) ? (input.durationMs - windowMs) : 0;
    return input.positionMs >= triggerPos;
}

bool PlaybackTransitionCoordinator::shouldSignalTrackEnding(const TrackEndingInput& input)
{
    if(input.endOfInput && input.bufferEmpty) {
        clearAutoTransitionReadiness();
        return true;
    }

    const bool durationBoundaryReached
        = input.durationBoundaryEnabled && input.durationMs > 0 && input.positionMs >= input.durationMs;

    // DecoderEofOnly mode: do not derive "about to finish" from timeline.
    // Wait until decoder end-of-input is observed.
    const bool timelineWindowEligible = input.durationBoundaryEnabled;

    bool crossfadeReady = false;
    if(input.autoCrossfadeEnabled) {
        const uint64_t fadeOutWindowMs  = static_cast<uint64_t>(std::max(0, input.autoFadeOutMs));
        const uint64_t timelineWindowMs = saturatingAddWindow(fadeOutWindowMs, input.timelineDelayMs);
        const bool readyByTimeline      = timelineWindowEligible && inTimelineWindow(input, timelineWindowMs);
        const bool readyByDrain         = input.endOfInput && input.remainingOutputMs <= timelineWindowMs;
        crossfadeReady                  = readyByTimeline || readyByDrain;
    }

    bool gaplessReady = false;
    if(input.gaplessEnabled) {
        const uint64_t timelineWindowMs = saturatingAddWindow(input.gaplessPrepareWindowMs, input.timelineDelayMs);
        const bool readyByTimeline      = timelineWindowEligible && inTimelineWindow(input, timelineWindowMs);
        const bool readyByDrain         = input.endOfInput && input.remainingOutputMs <= timelineWindowMs;
        gaplessReady                    = input.endOfInput || readyByTimeline || readyByDrain;
    }

    if(crossfadeReady || gaplessReady) {
        // When both are enabled, auto crossfade should own transition mode;
        // gapless is only a readiness lead/fallback for non-crossfade paths
        m_autoTransitionMode = input.autoCrossfadeEnabled ? AutoTransitionMode::Crossfade : AutoTransitionMode::Gapless;
        return true;
    }

    // Multi-track files can hit the track duration boundary
    // while decoder input is still active. Emit about-to-finish so callers can
    // perform UI/playlist advancement for the next track.
    if(durationBoundaryReached) {
        clearAutoTransitionReadiness();
        return true;
    }

    clearAutoTransitionReadiness();
    if(input.autoCrossfadeEnabled || input.gaplessEnabled) {
        return false;
    }

    return input.endOfInput;
}

bool PlaybackTransitionCoordinator::shouldSignalReadyToSwitch(const TrackEndingInput& input) const
{
    switch(m_autoTransitionMode) {
        case AutoTransitionMode::Crossfade: {
            const uint64_t fadeOutWindowMs  = static_cast<uint64_t>(std::max(0, input.autoFadeOutMs));
            const uint64_t fadeInWindowMs   = static_cast<uint64_t>(std::max(0, input.autoFadeInMs));
            const uint64_t overlapWindowMs  = std::min(fadeOutWindowMs, fadeInWindowMs);
            const uint64_t timelineWindowMs = saturatingAddWindow(overlapWindowMs, input.timelineDelayMs);
            const bool readyByTimeline      = inTimelineWindow(input, timelineWindowMs);
            const bool readyByDrain         = input.endOfInput && input.remainingOutputMs <= timelineWindowMs;
            return readyByTimeline || readyByDrain;
        }
        case AutoTransitionMode::Gapless: {
            const uint64_t switchLeadMs     = input.gaplessPrepareWindowMs;
            const uint64_t timelineWindowMs = saturatingAddWindow(switchLeadMs, input.timelineDelayMs);
            const bool readyByTimeline      = inTimelineWindow(input, timelineWindowMs);
            const bool readyByDrain         = input.endOfInput && input.remainingOutputMs <= timelineWindowMs;
            return readyByTimeline || readyByDrain;
        }
        case AutoTransitionMode::None:
            return false;
    }

    return false;
}

void PlaybackTransitionCoordinator::beginPendingSeek(uint64_t positionMs, int fadeOutDurationMs, int fadeInDurationMs,
                                                     uint64_t transitionId)
{
    clearAutoTransitionReadiness();
    m_crossfadeParams.seekPositionMs    = positionMs;
    m_crossfadeParams.fadeOutDurationMs = fadeOutDurationMs;
    m_crossfadeParams.fadeInDurationMs  = fadeInDurationMs;
    m_crossfadeParams.transitionId      = transitionId;
    m_pendingSeekActive                 = true;
}

void PlaybackTransitionCoordinator::cancelPendingSeek()
{
    m_pendingSeekActive = false;
}

bool PlaybackTransitionCoordinator::hasPendingSeek() const
{
    return m_pendingSeekActive;
}

std::optional<PlaybackTransitionCoordinator::CrossfadeParams>
PlaybackTransitionCoordinator::pendingSeekIfBuffered(uint64_t bufferedMs) const
{
    if(!m_pendingSeekActive) {
        return {};
    }

    if(std::cmp_less(bufferedMs, m_crossfadeParams.fadeOutDurationMs)) {
        return {};
    }

    return m_crossfadeParams;
}

bool PlaybackTransitionCoordinator::isReadyForAutoTransition() const
{
    return m_autoTransitionMode != AutoTransitionMode::None;
}

PlaybackTransitionCoordinator::AutoTransitionMode PlaybackTransitionCoordinator::autoTransitionMode() const
{
    return m_autoTransitionMode;
}

void PlaybackTransitionCoordinator::clearAutoTransitionReadiness()
{
    m_autoTransitionMode = AutoTransitionMode::None;
}

bool PlaybackTransitionCoordinator::isReadyForAutoCrossfade() const
{
    return m_autoTransitionMode == AutoTransitionMode::Crossfade;
}

bool PlaybackTransitionCoordinator::isReadyForGaplessHandoff() const
{
    return m_autoTransitionMode == AutoTransitionMode::Gapless;
}

void PlaybackTransitionCoordinator::clearForStop()
{
    m_pendingSeekActive = false;
    clearAutoTransitionReadiness();
    m_trackEnding                 = false;
    m_switchReady                 = false;
    m_boundaryReached             = false;
    m_pendingInitialSeekActive    = false;
    m_pendingInitialSeekRequestId = 0;
}

void PlaybackTransitionCoordinator::clearTrackEnding()
{
    m_trackEnding     = false;
    m_switchReady     = false;
    m_boundaryReached = false;
    clearAutoTransitionReadiness();
}

bool PlaybackTransitionCoordinator::isSeekInProgress() const
{
    return m_seekInProgress;
}

void PlaybackTransitionCoordinator::setSeekInProgress(bool inProgress)
{
    m_seekInProgress = inProgress;
}

void PlaybackTransitionCoordinator::queueInitialSeek(uint64_t positionMs, int trackId, uint64_t requestId)
{
    m_pendingInitialSeekActive     = true;
    m_pendingInitialSeekPositionMs = positionMs;
    m_pendingInitialSeekRequestId  = requestId;
    m_pendingInitialSeekTrackId    = trackId;
}

bool PlaybackTransitionCoordinator::hasPendingInitialSeekForTrack(int trackId) const
{
    if(!m_pendingInitialSeekActive) {
        return false;
    }
    return m_pendingInitialSeekTrackId == -1 || m_pendingInitialSeekTrackId == trackId;
}

std::optional<PlaybackTransitionCoordinator::InitialSeekRequest>
PlaybackTransitionCoordinator::takePendingInitialSeekForTrack(int trackId)
{
    if(!m_pendingInitialSeekActive) {
        return {};
    }

    const bool applies            = m_pendingInitialSeekTrackId == -1 || m_pendingInitialSeekTrackId == trackId;
    const auto positionMs         = m_pendingInitialSeekPositionMs;
    const auto requestId          = m_pendingInitialSeekRequestId;
    m_pendingInitialSeekActive    = false;
    m_pendingInitialSeekRequestId = 0;

    if(!applies) {
        return {};
    }

    return InitialSeekRequest{
        .positionMs = positionMs,
        .requestId  = requestId,
    };
}
} // namespace Fooyin
