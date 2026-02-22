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

#include "playbackcoordinator.h"

#include "engine/enginehandler.h"
#include "engine/enginehelpers.h"

#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <core/track.h>

#include <QLoggingCategory>
#include <QTimer>

#include <limits>

Q_LOGGING_CATEGORY(PLAYBACK_COORDINATOR, "fy.playback")

namespace Fooyin {
PlaybackCoordinator::PlaybackCoordinator(EngineHandler* engine, PlayerController* playerController,
                                         PlaylistHandler* playlistHandler, QObject* parent)
    : QObject{parent}
    , m_engine{engine}
    , m_playerController{playerController}
    , m_playlistHandler{playlistHandler}
    , m_pendingBoundaryReady{false}
    , m_pendingBoundarySwitchGateOpen{false}
{
    QObject::connect(m_engine, &EngineController::trackAboutToFinish, this,
                     &PlaybackCoordinator::handleTrackAboutToFinish);
    QObject::connect(m_engine, &EngineController::trackReadyToSwitch, this,
                     &PlaybackCoordinator::handleTrackReadyToSwitch);
    QObject::connect(m_engine, &EngineController::trackStatusContextChanged, this,
                     &PlaybackCoordinator::handleTrackStatusContext);
    QObject::connect(m_engine, &EngineController::nextTrackReadiness, this,
                     &PlaybackCoordinator::handleNextTrackReadiness);
}

PlaybackCoordinator::EndAction
PlaybackCoordinator::evaluateEndForAutoAdvance(const Track& currentTrack, const Track& endedTrack,
                                               const uint64_t generation, std::optional<uint64_t>& suppressedGeneration)
{
    if(suppressedGeneration && *suppressedGeneration == generation) {
        suppressedGeneration.reset();
        return EndAction::Suppress;
    }

    if(sameTrackIdentity(currentTrack, endedTrack)) {
        return EndAction::Advance;
    }

    return EndAction::IgnoreStale;
}

void PlaybackCoordinator::handleTrackAboutToFinish(const Engine::AboutToFinishContext& context)
{
    const Track currentTrack = m_playerController->currentTrack();
    if(!sameTrackIdentity(currentTrack, context.track)) {
        return;
    }

    const Track nextTrack = m_playerController->upcomingTrack();
    if(!nextTrack.isValid()) {
        return;
    }

    if(isMultiTrackFileTransition(currentTrack, nextTrack)) {
        advancePreparedAtBoundary(context.generation, currentTrack, nextTrack);
        return;
    }

    // Prepare early for upcoming transitions. Actual switch is gated by
    // trackReadyToSwitch and nextTrackReadiness.
    const auto request = m_engine->prepareNextTrackForPlayback(nextTrack);

    m_pendingBoundaryAdvanceGeneration = context.generation;
    m_pendingBoundaryPrepareRequestId  = request.requestId;
    m_pendingBoundaryExpectedTrack     = currentTrack;
    m_pendingBoundaryExpectedNextTrack = nextTrack;
    m_pendingBoundaryReady             = request.readyNow;
    m_pendingBoundarySwitchGateOpen    = false;

    tryAdvancePreparedBoundary();
}

void PlaybackCoordinator::handleTrackReadyToSwitch(const Engine::AboutToFinishContext& context)
{
    if(!m_pendingBoundaryAdvanceGeneration || *m_pendingBoundaryAdvanceGeneration != context.generation) {
        return;
    }

    if(!sameTrackIdentity(context.track, m_pendingBoundaryExpectedTrack)) {
        return;
    }

    m_pendingBoundarySwitchGateOpen = true;
    tryAdvancePreparedBoundary();
}

void PlaybackCoordinator::handleTrackStatusContext(const Engine::TrackStatusContext& context)
{
    switch(context.status) {
        case Engine::TrackStatus::End: {
            if(m_pendingBoundaryAdvanceGeneration && *m_pendingBoundaryAdvanceGeneration == context.generation) {
                clearPendingBoundaryAdvance();
            }
            const auto endAction = evaluateEndForAutoAdvance(m_playerController->currentTrack(), context.track,
                                                             context.generation, m_suppressedEndGeneration);
            switch(endAction) {
                case EndAction::Suppress:
                    qCDebug(PLAYBACK_COORDINATOR)
                        << "Suppressing auto-advance for track" << context.track.prettyFilepath();
                    break;
                case EndAction::IgnoreStale:
                    qCDebug(PLAYBACK_COORDINATOR) << "Ignoring stale end for track" << context.track.prettyFilepath();
                    break;
                case EndAction::Advance:
                    m_playerController->advance(Player::AdvanceReason::NaturalEnd);
                    break;
            }
            break;
        }
        case Engine::TrackStatus::NoTrack:
            m_suppressedEndGeneration.reset();
            clearPendingBoundaryAdvance();
            break;
        case Engine::TrackStatus::Invalid:
        case Engine::TrackStatus::Loading:
        case Engine::TrackStatus::Loaded:
        case Engine::TrackStatus::Buffered:
        case Engine::TrackStatus::Unreadable:
            break;
    }
}

void PlaybackCoordinator::handleNextTrackReadiness(const Track& track, bool ready, uint64_t requestId)
{
    if(!m_pendingBoundaryAdvanceGeneration.has_value() || !m_pendingBoundaryPrepareRequestId.has_value()) {
        return;
    }

    if(*m_pendingBoundaryPrepareRequestId != requestId) {
        return;
    }

    if(!sameTrackIdentity(track, m_pendingBoundaryExpectedNextTrack)) {
        return;
    }

    m_pendingBoundaryReady = ready;
    if(!ready) {
        return;
    }

    tryAdvancePreparedBoundary();
}

void PlaybackCoordinator::tryAdvancePreparedBoundary()
{
    if(!m_pendingBoundaryAdvanceGeneration.has_value() || !m_pendingBoundaryPrepareRequestId.has_value()
       || !m_pendingBoundaryReady || !m_pendingBoundarySwitchGateOpen) {
        return;
    }

    const Track currentTrack = m_playerController->currentTrack();
    if(!sameTrackIdentity(currentTrack, m_pendingBoundaryExpectedTrack)) {
        clearPendingBoundaryAdvance();
        return;
    }

    const Track nextTrack = m_playerController->upcomingTrack();
    if(!sameTrackIdentity(nextTrack, m_pendingBoundaryExpectedNextTrack)) {
        clearPendingBoundaryAdvance();
        return;
    }

    m_suppressedEndGeneration = m_pendingBoundaryAdvanceGeneration;
    clearPendingBoundaryAdvance();
    m_playlistHandler->prepareUpcomingTrack();
    m_playerController->advance(Player::AdvanceReason::PreparedCrossfade);
}

void PlaybackCoordinator::advancePreparedAtBoundary(uint64_t generation, const Track& expectedTrack,
                                                    const Track& expectedNextTrack)
{
    m_pendingBoundaryAdvanceGeneration = generation;
    m_pendingBoundaryPrepareRequestId  = {};
    m_pendingBoundaryExpectedTrack     = expectedTrack;
    m_pendingBoundaryExpectedNextTrack = expectedNextTrack;
    m_pendingBoundaryReady             = false;
    m_pendingBoundarySwitchGateOpen    = false;

    // Same-file multi-track handoffs are timeline-bound and do not go through
    // crossfade/gapless readiness gates. Keep a boundary delay for these
    // transitions so UI/track context flip near the segment edge.
    const uint64_t currentPosMs = m_playerController->currentPosition();
    const uint64_t durationMs   = expectedTrack.duration();
    const bool hasDuration      = durationMs > 0;
    const uint64_t remainingMs  = hasDuration && currentPosMs < durationMs ? (durationMs - currentPosMs) : 0;
    const int delayMs           = static_cast<int>(std::min<uint64_t>(remainingMs, std::numeric_limits<int>::max()));

    QTimer::singleShot(delayMs, this, [this, generation, expectedTrack, expectedNextTrack]() {
        if(!m_pendingBoundaryAdvanceGeneration || *m_pendingBoundaryAdvanceGeneration != generation) {
            return;
        }

        const Track currentTrack = m_playerController->currentTrack();
        if(!sameTrackIdentity(currentTrack, expectedTrack)) {
            clearPendingBoundaryAdvance();
            return;
        }

        const Track nextTrack = m_playerController->upcomingTrack();
        if(!sameTrackIdentity(nextTrack, expectedNextTrack)) {
            clearPendingBoundaryAdvance();
            return;
        }

        if(!isMultiTrackFileTransition(expectedTrack, expectedNextTrack)) {
            clearPendingBoundaryAdvance();
            return;
        }

        m_suppressedEndGeneration = generation;
        clearPendingBoundaryAdvance();
        m_playlistHandler->prepareUpcomingTrack();
        m_playerController->advance(Player::AdvanceReason::PreparedCrossfade);
    });
}

void PlaybackCoordinator::clearPendingBoundaryAdvance()
{
    m_pendingBoundaryAdvanceGeneration.reset();
    m_pendingBoundaryPrepareRequestId.reset();
    m_pendingBoundaryExpectedTrack     = {};
    m_pendingBoundaryExpectedNextTrack = {};
    m_pendingBoundaryReady             = false;
    m_pendingBoundarySwitchGateOpen    = false;
}
} // namespace Fooyin
