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

#include "enginehelpers.h"

namespace Fooyin {
PlaybackTransitionCoordinator::PlaybackTransitionCoordinator()
    : m_autoTransitionMode{AutoTransitionMode::None}
    , m_trackEnding{false}
    , m_switchReady{false}
    , m_boundaryReached{false}
    , m_seekInProgress{false}
{ }

PlaybackTransitionCoordinator::TrackEndingResult
PlaybackTransitionCoordinator::evaluateTrackEnding(const TrackEndingInput& input)
{
    TrackEndingResult result;
    result.remainingOutputMs = input.remainingOutputMs;

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

bool PlaybackTransitionCoordinator::inTransitionWindow(const TrackEndingInput& input, uint64_t leadMs,
                                                       bool timelineEnabled)
{
    const uint64_t windowMs    = saturatingAdd(leadMs, input.timelineDelayMs);
    const bool readyByTimeline = timelineEnabled && inTimelineWindow(input, windowMs);
    const bool readyByDrain    = input.endOfInput && input.remainingOutputMs <= windowMs;
    return readyByTimeline || readyByDrain;
}

bool PlaybackTransitionCoordinator::shouldSignalTrackEnding(const TrackEndingInput& input)
{
    if(input.endOfInput && input.bufferEmpty) {
        clearAutoTransitionReadiness();
        return true;
    }

    const bool durationBoundaryReached
        = input.durationBoundaryEnabled && input.durationMs > 0 && input.positionMs >= input.durationMs;
    const bool timelineWindowEligible = input.predictiveTimelineHintsEnabled && input.durationMs > 0;

    bool crossfadeReady = false;
    if(input.autoCrossfadeEnabled) {
        const uint64_t fadeOutWindowMs = static_cast<uint64_t>(std::max(0, input.autoFadeOutMs));
        crossfadeReady                 = inTransitionWindow(input, fadeOutWindowMs, timelineWindowEligible);
    }

    bool boundaryFadeReady = false;
    if(input.boundaryFadeEnabled) {
        const uint64_t fadeOutWindowMs = static_cast<uint64_t>(std::max(0, input.boundaryFadeOutMs));
        const uint64_t prepareWindowMs = std::max(fadeOutWindowMs, input.gaplessPrepareWindowMs);
        boundaryFadeReady              = inTransitionWindow(input, prepareWindowMs, true);
    }

    bool gaplessReady = false;
    if(input.gaplessEnabled) {
        gaplessReady
            = input.endOfInput || inTransitionWindow(input, input.gaplessPrepareWindowMs, timelineWindowEligible);
    }

    const bool crossfadeConfigured = input.autoCrossfadeEnabled;
    const bool boundaryConfigured  = input.boundaryFadeEnabled;

    if(crossfadeReady || boundaryFadeReady || gaplessReady) {
        // Keep configured transition ownership stable when gapless lead trips first.
        if(crossfadeConfigured) {
            m_autoTransitionMode = AutoTransitionMode::Crossfade;
        }
        else if(boundaryConfigured) {
            m_autoTransitionMode = AutoTransitionMode::BoundaryFade;
        }
        else {
            m_autoTransitionMode = AutoTransitionMode::Gapless;
        }
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
    if(input.autoCrossfadeEnabled || input.boundaryFadeEnabled || input.gaplessEnabled) {
        return false;
    }

    return input.endOfInput;
}

bool PlaybackTransitionCoordinator::shouldSignalReadyToSwitch(const TrackEndingInput& input) const
{
    switch(m_autoTransitionMode) {
        case AutoTransitionMode::Crossfade: {
            const uint64_t fadeOutWindowMs = static_cast<uint64_t>(std::max(0, input.autoFadeOutMs));
            const uint64_t fadeInWindowMs  = static_cast<uint64_t>(std::max(0, input.autoFadeInMs));
            const uint64_t overlapWindowMs = std::min(fadeOutWindowMs, fadeInWindowMs);
            return inTransitionWindow(input, overlapWindowMs, true);
        }
        case AutoTransitionMode::Gapless:
            return inTransitionWindow(input, input.gaplessPrepareWindowMs, true);
        case AutoTransitionMode::BoundaryFade:
        case AutoTransitionMode::None:
            return false;
    }

    return false;
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
    return m_autoTransitionMode == AutoTransitionMode::Gapless
        || m_autoTransitionMode == AutoTransitionMode::BoundaryFade;
}

void PlaybackTransitionCoordinator::clearForStop()
{
    clearAutoTransitionReadiness();
    m_trackEnding     = false;
    m_switchReady     = false;
    m_boundaryReached = false;
    m_pendingInitialSeek.reset();
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
    m_pendingInitialSeek = PendingInitialSeek{
        .request = {.positionMs = positionMs, .requestId = requestId},
        .trackId = trackId,
    };
}

bool PlaybackTransitionCoordinator::hasPendingInitialSeekForTrack(int trackId) const
{
    return m_pendingInitialSeek && (m_pendingInitialSeek->trackId == -1 || m_pendingInitialSeek->trackId == trackId);
}

std::optional<PlaybackTransitionCoordinator::InitialSeekRequest>
PlaybackTransitionCoordinator::takePendingInitialSeekForTrack(int trackId)
{
    const auto pending = std::exchange(m_pendingInitialSeek, std::nullopt);
    if(!pending || (pending->trackId != -1 && pending->trackId != trackId)) {
        return {};
    }

    return pending->request;
}
} // namespace Fooyin
