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

#include "transitionorchestrator.h"

namespace Fooyin {
TransitionOrchestrator::TrackEndingResult TransitionOrchestrator::evaluateTrackEnding(const TrackEndingInput& input)
{
    return m_state.evaluateTrackEnding(input);
}

TransitionOrchestrator::TransitionOrchestrator()
    : m_activeTransportTransitionId{0}
{ }

bool TransitionOrchestrator::isReadyForAutoTransition() const
{
    return m_state.isReadyForAutoTransition();
}

AutoTransitionMode TransitionOrchestrator::autoTransitionMode() const
{
    return m_state.autoTransitionMode();
}

bool TransitionOrchestrator::isSeekInProgress() const
{
    return m_state.isSeekInProgress();
}

void TransitionOrchestrator::setSeekInProgress(bool inProgress)
{
    m_state.setSeekInProgress(inProgress);
}

void TransitionOrchestrator::clearTrackEnding()
{
    m_state.clearTrackEnding();
}

void TransitionOrchestrator::cancelPendingSeek()
{
    m_state.cancelPendingSeek();
}

bool TransitionOrchestrator::hasPendingSeek() const
{
    return m_state.hasPendingSeek();
}

std::optional<TransitionOrchestrator::CrossfadeParams>
TransitionOrchestrator::pendingSeekIfBuffered(uint64_t bufferedMs) const
{
    return m_state.pendingSeekIfBuffered(bufferedMs);
}

bool TransitionOrchestrator::hasPendingInitialSeekForTrack(int trackId) const
{
    return m_state.hasPendingInitialSeekForTrack(trackId);
}

std::optional<TransitionOrchestrator::InitialSeekRequest>
TransitionOrchestrator::takePendingInitialSeekForTrack(int trackId)
{
    return m_state.takePendingInitialSeekForTrack(trackId);
}

void TransitionOrchestrator::queueInitialSeek(uint64_t positionMs, int trackId, uint64_t requestId)
{
    m_state.queueInitialSeek(positionMs, trackId, requestId);
}

void TransitionOrchestrator::clearForStop()
{
    m_state.clearForStop();
}

uint64_t TransitionOrchestrator::beginTransportTransition(uint64_t transitionId)
{
    m_activeTransportTransitionId = transitionId;
    return transitionId;
}

void TransitionOrchestrator::clearTransportTransition()
{
    m_activeTransportTransitionId = 0;
}

bool TransitionOrchestrator::isActiveTransportTransition(uint64_t transitionId) const
{
    return transitionId != 0 && transitionId == m_activeTransportTransitionId;
}
} // namespace Fooyin
