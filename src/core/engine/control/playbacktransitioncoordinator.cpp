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
#include <utility>

namespace Fooyin {
PlaybackTransitionCoordinator::PlaybackTransitionCoordinator()
    : m_autoTransitionMode{AutoTransitionMode::None}
    , m_trackEnding{false}
    , m_switchReady{false}
    , m_seekInProgress{false}
    , m_pendingSeekActive{false}
    , m_pendingInitialSeekActive{false}
    , m_pendingInitialSeekPositionMs{0}
    , m_pendingInitialSeekTrackId{-1}
{ }

PlaybackTransitionCoordinator::TrackEndingResult
PlaybackTransitionCoordinator::evaluateTrackEnding(const TrackEndingInput& input)
{
    TrackEndingResult result;

    if(input.endOfInput && input.bufferEmpty) {
        m_trackEnding = true;
        m_switchReady = false;
        clearAutoTransitionReadiness();
        result.endReached = true;
        return result;
    }

    if(!m_trackEnding && shouldSignalTrackEnding(input)) {
        m_trackEnding        = true;
        result.aboutToFinish = true;
    }

    if(m_trackEnding && !m_switchReady && shouldSignalReadyToSwitch(input)) {
        m_switchReady        = true;
        result.readyToSwitch = true;
    }

    return result;
}

bool PlaybackTransitionCoordinator::inTimelineWindow(const TrackEndingInput& input, const uint64_t windowMs)
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

    const bool durationBoundaryReached = input.durationMs > 0 && input.positionMs >= input.durationMs;

    bool crossfadeReady = false;
    if(input.autoCrossfadeEnabled) {
        const auto fadeOutWindowMs = static_cast<uint64_t>(std::max(0, input.autoFadeOutMs));
        const bool readyByTimeline = inTimelineWindow(input, fadeOutWindowMs);
        const bool readyByDrain    = input.endOfInput && input.remainingOutputMs <= fadeOutWindowMs;
        crossfadeReady             = readyByTimeline || readyByDrain;
    }

    bool gaplessReady = false;
    if(input.gaplessEnabled) {
        const bool readyByTimeline = inTimelineWindow(input, input.gaplessPrepareWindowMs);
        const bool readyByDrain    = input.endOfInput && input.remainingOutputMs <= input.gaplessPrepareWindowMs;
        gaplessReady               = readyByTimeline || readyByDrain;
    }

    if(crossfadeReady || gaplessReady) {
        m_autoTransitionMode = crossfadeReady ? AutoTransitionMode::Crossfade : AutoTransitionMode::Gapless;
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
            const uint64_t fadeOutWindowMs = static_cast<uint64_t>(std::max(0, input.autoFadeOutMs));
            const uint64_t fadeInWindowMs  = static_cast<uint64_t>(std::max(0, input.autoFadeInMs));
            const uint64_t overlapWindowMs = std::min(fadeOutWindowMs, fadeInWindowMs);
            const bool readyByTimeline     = inTimelineWindow(input, overlapWindowMs);
            const bool readyByDrain        = input.endOfInput && input.remainingOutputMs <= overlapWindowMs;
            return readyByTimeline || readyByDrain;
        }
        case AutoTransitionMode::Gapless: {
            const bool readyByTimeline = inTimelineWindow(input, input.gaplessPrepareWindowMs);
            const bool readyByDrain    = input.endOfInput && input.remainingOutputMs <= input.gaplessPrepareWindowMs;
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
    m_trackEnding              = false;
    m_switchReady              = false;
    m_pendingInitialSeekActive = false;
}

void PlaybackTransitionCoordinator::clearTrackEnding()
{
    m_trackEnding = false;
    m_switchReady = false;
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

void PlaybackTransitionCoordinator::queueInitialSeek(uint64_t positionMs, int trackId)
{
    m_pendingInitialSeekActive     = true;
    m_pendingInitialSeekPositionMs = positionMs;
    m_pendingInitialSeekTrackId    = trackId;
}

bool PlaybackTransitionCoordinator::hasPendingInitialSeekForTrack(int trackId) const
{
    if(!m_pendingInitialSeekActive) {
        return false;
    }
    return m_pendingInitialSeekTrackId == -1 || m_pendingInitialSeekTrackId == trackId;
}

std::optional<uint64_t> PlaybackTransitionCoordinator::takePendingInitialSeekForTrack(int trackId)
{
    if(!m_pendingInitialSeekActive) {
        return {};
    }

    const bool applies         = m_pendingInitialSeekTrackId == -1 || m_pendingInitialSeekTrackId == trackId;
    const auto positionMs      = m_pendingInitialSeekPositionMs;
    m_pendingInitialSeekActive = false;

    if(!applies) {
        return {};
    }

    return positionMs;
}
} // namespace Fooyin
