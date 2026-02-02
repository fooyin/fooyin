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

#include "playbacktransitioncoordinator.h"

#include <optional>

namespace Fooyin {
using AutoTransitionMode = PlaybackTransitionCoordinator::AutoTransitionMode;

/*!
 * Engine-side state holder for transition and seek coordination.
 *
 * Wraps `PlaybackTransitionCoordinator` with additional engine-level state for
 * initial-seek and transition ownership.
 */
class TransitionOrchestrator
{
public:
    using TrackEndingResult = PlaybackTransitionCoordinator::TrackEndingResult;
    using TrackEndingInput  = PlaybackTransitionCoordinator::TrackEndingInput;
    using CrossfadeParams   = PlaybackTransitionCoordinator::CrossfadeParams;

    TransitionOrchestrator();

    //! Evaluate current track-ending window and update auto-transition readiness.
    [[nodiscard]] TrackEndingResult evaluateTrackEnding(const TrackEndingInput& input);
    //! True when coordinator determined an auto transition may start.
    [[nodiscard]] bool isReadyForAutoTransition() const;
    //! Transition mode selected by coordinator.
    [[nodiscard]] AutoTransitionMode autoTransitionMode() const;

    [[nodiscard]] bool isSeekInProgress() const;
    void setSeekInProgress(bool inProgress);
    void clearTrackEnding();
    void cancelPendingSeek();
    [[nodiscard]] bool hasPendingSeek() const;

    //! Pending seek request becomes executable once current stream is sufficiently buffered.
    [[nodiscard]] std::optional<CrossfadeParams> pendingSeekIfBuffered(uint64_t bufferedMs) const;

    [[nodiscard]] bool hasPendingInitialSeekForTrack(int trackId) const;
    [[nodiscard]] std::optional<uint64_t> takePendingInitialSeekForTrack(int trackId);
    //! Queue a seek to apply immediately after initial load for the specified track.
    void queueInitialSeek(uint64_t positionMs, int trackId);

    //! Clear all transition/seek state for hard stop.
    void clearForStop();

    //! Mark a transport transition as active and return the active id.
    [[nodiscard]] uint64_t beginTransportTransition(uint64_t transitionId);
    void clearTransportTransition();
    //! True when provided transition id is still active.
    [[nodiscard]] bool isActiveTransportTransition(uint64_t transitionId) const;

private:
    PlaybackTransitionCoordinator m_state;
    uint64_t m_activeTransportTransitionId;
};
} // namespace Fooyin
