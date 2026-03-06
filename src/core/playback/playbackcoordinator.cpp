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
#include "internalcoresettings.h"

#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <core/track.h>
#include <utils/enum.h>
#include <utils/settings/settingsmanager.h>

#include <QLoggingCategory>
#include <QTimer>

#include <limits>

Q_LOGGING_CATEGORY(PLAYBACK, "fy.playback")

constexpr int PreparedBoundaryWatchdogGraceMs = 1500;

namespace Fooyin {
PlaybackCoordinator::PlaybackCoordinator(EngineHandler* engine, PlayerController* playerController,
                                         PlaylistHandler* playlistHandler, SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_engine{engine}
    , m_playerController{playerController}
    , m_playlistHandler{playlistHandler}
    , m_crossfadeSwitchPolicy{settings ? static_cast<Engine::CrossfadeSwitchPolicy>(
                                             settings->value<Settings::Core::Internal::CrossfadeSwitchPolicy>())
                                       : Engine::CrossfadeSwitchPolicy::OverlapStart}
    , m_pendingBoundaryReady{false}
    , m_pendingBoundarySwitchGateOpen{false}
    , m_pendingBoundaryCrossfadeArmAttempted{false}
    , m_pendingBoundaryCrossfadeArmInFlight{false}
    , m_pendingBoundaryGaplessArmAttempted{false}
    , m_pendingBoundaryGaplessArmInFlight{false}
    , m_pendingBoundaryPreparedTransition{PreparedTransition::None}
    , m_pendingBoundarySwitchAnchor{SwitchAnchor::Unknown}
    , m_pendingBoundaryLastReason{BoundaryTransitionReason::Unknown}
{
    QObject::connect(m_engine, &EngineController::trackAboutToFinish, this,
                     &PlaybackCoordinator::handleTrackAboutToFinish);
    QObject::connect(m_engine, &EngineController::trackReadyToSwitch, this,
                     &PlaybackCoordinator::handleTrackReadyToSwitch);
    QObject::connect(m_engine, &EngineController::trackBoundaryReached, this,
                     &PlaybackCoordinator::handleTrackBoundaryReached);
    QObject::connect(m_engine, &EngineController::trackStatusContextChanged, this,
                     &PlaybackCoordinator::handleTrackStatusContext);
    QObject::connect(m_engine, &EngineController::nextTrackReadiness, this,
                     &PlaybackCoordinator::handleNextTrackReadiness);
    QObject::connect(m_engine, &EngineController::preparedCrossfadeArmResult, this,
                     &PlaybackCoordinator::handlePreparedCrossfadeArmResult);
    QObject::connect(m_engine, &EngineController::preparedGaplessArmResult, this,
                     &PlaybackCoordinator::handlePreparedGaplessArmResult);

    settings->subscribe<Settings::Core::Internal::CrossfadeSwitchPolicy>(
        this, [this](int policy) { m_crossfadeSwitchPolicy = static_cast<Engine::CrossfadeSwitchPolicy>(policy); });
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

bool PlaybackCoordinator::isPreparedArmResultRelevant(std::optional<uint64_t> pendingGeneration,
                                                      const Track& expectedNextTrack, const bool armInFlight,
                                                      const Track& resultTrack, const uint64_t resultGeneration)
{
    if(!pendingGeneration || *pendingGeneration != resultGeneration) {
        return false;
    }

    if(!sameTrackIdentity(resultTrack, expectedNextTrack)) {
        return false;
    }

    return armInFlight;
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
    // transition anchor signal (trackReadyToSwitch or trackBoundaryReached)
    // and nextTrackReadiness.
    const auto request = m_engine->prepareNextTrackForPlayback(nextTrack);

    m_pendingBoundaryAdvanceGeneration     = context.generation;
    m_pendingBoundaryPrepareRequestId      = request.requestId;
    m_pendingBoundaryExpectedTrack         = currentTrack;
    m_pendingBoundaryExpectedNextTrack     = nextTrack;
    m_pendingBoundaryReady                 = request.readyNow;
    m_pendingBoundarySwitchGateOpen        = false;
    m_pendingBoundaryCrossfadeArmAttempted = false;
    m_pendingBoundaryCrossfadeArmInFlight  = false;
    m_pendingBoundaryGaplessArmAttempted   = false;
    m_pendingBoundaryGaplessArmInFlight    = false;
    m_pendingBoundaryPreparedTransition    = PreparedTransition::None;
    m_pendingBoundarySwitchAnchor          = SwitchAnchor::Unknown;
    markBoundaryTransition(BoundaryTransitionReason::AboutToFinishQueued);

    qCDebug(PLAYBACK) << "Prepared boundary transition queued:" << "currentTrackId=" << currentTrack.id()
                      << "nextTrackId=" << nextTrack.id() << "generation=" << context.generation
                      << "prepareRequestId=" << request.requestId << "readyNow=" << request.readyNow;

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

    if(m_pendingBoundarySwitchAnchor == SwitchAnchor::BoundarySignal) {
        return;
    }

    m_pendingBoundarySwitchAnchor = SwitchAnchor::ReadySignal;
    markBoundaryTransition(BoundaryTransitionReason::ReadySignalAnchorSet);

    if(m_crossfadeSwitchPolicy != Engine::CrossfadeSwitchPolicy::Boundary) {
        m_pendingBoundarySwitchGateOpen = true;
    }

    tryAdvancePreparedBoundary();
}

void PlaybackCoordinator::handleTrackBoundaryReached(const Engine::AboutToFinishContext& context)
{
    if(!m_pendingBoundaryAdvanceGeneration || *m_pendingBoundaryAdvanceGeneration != context.generation) {
        return;
    }

    if(!sameTrackIdentity(context.track, m_pendingBoundaryExpectedTrack)) {
        return;
    }

    m_pendingBoundarySwitchGateOpen = true;
    m_pendingBoundarySwitchAnchor   = SwitchAnchor::BoundarySignal;
    markBoundaryTransition(BoundaryTransitionReason::BoundarySignalAnchorSet);

    // Same-file multi-track transitions do not use prepare-readiness request IDs.
    // Commit immediately on boundary signal to avoid waiting for TrackStatus::End.
    if(!m_pendingBoundaryPrepareRequestId.has_value()) {
        const Track currentTrack = m_playerController->currentTrack();
        const Track nextTrack    = m_playerController->upcomingTrack();
        if(!sameTrackIdentity(currentTrack, m_pendingBoundaryExpectedTrack)
           || !sameTrackIdentity(nextTrack, m_pendingBoundaryExpectedNextTrack)
           || !isMultiTrackFileTransition(currentTrack, nextTrack)) {
            clearPendingBoundaryAdvance(BoundaryTransitionReason::ClearTrackMismatch);
            return;
        }

        m_suppressedEndGeneration = m_pendingBoundaryAdvanceGeneration;
        markBoundaryTransition(BoundaryTransitionReason::AdvancingPreparedTransition);
        clearPendingBoundaryAdvance(BoundaryTransitionReason::AdvancingPreparedTransition);
        m_playlistHandler->prepareUpcomingTrack();
        m_playerController->advance(Player::AdvanceReason::PreparedCrossfade);
        return;
    }

    tryAdvancePreparedBoundary();
    armBoundaryCommitWatchdog();
}

void PlaybackCoordinator::handleTrackStatusContext(const Engine::TrackStatusContext& context)
{
    switch(context.status) {
        case Engine::TrackStatus::End: {
            if(m_pendingBoundaryAdvanceGeneration && *m_pendingBoundaryAdvanceGeneration == context.generation) {
                clearPendingBoundaryAdvance(BoundaryTransitionReason::ClearFromTrackEnd);
            }
            const auto endAction = evaluateEndForAutoAdvance(m_playerController->currentTrack(), context.track,
                                                             context.generation, m_suppressedEndGeneration);
            switch(endAction) {
                case EndAction::Suppress:
                case EndAction::IgnoreStale:
                    break;
                case EndAction::Advance:
                    m_playerController->advance(Player::AdvanceReason::NaturalEnd);
                    break;
            }
            break;
        }
        case Engine::TrackStatus::NoTrack:
            m_suppressedEndGeneration.reset();
            clearPendingBoundaryAdvance(BoundaryTransitionReason::ClearFromNoTrack);
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
    markBoundaryTransition(BoundaryTransitionReason::ReadinessUpdated);
    if(!ready) {
        return;
    }

    tryAdvancePreparedBoundary();
}

void PlaybackCoordinator::handlePreparedCrossfadeArmResult(const Track& track, uint64_t generation, bool armed)
{
    if(!isPreparedArmResultRelevant(m_pendingBoundaryAdvanceGeneration, m_pendingBoundaryExpectedNextTrack,
                                    m_pendingBoundaryCrossfadeArmInFlight, track, generation)) {
        return;
    }

    m_pendingBoundaryCrossfadeArmInFlight = false;

    qCDebug(PLAYBACK) << "Prepared boundary crossfade arm result:"
                      << "nextTrackId=" << track.id() << "generation=" << generation << "armed=" << armed;

    if(armed) {
        m_pendingBoundaryPreparedTransition = PreparedTransition::Crossfade;
        m_pendingBoundaryReady              = true;
        markBoundaryTransition(BoundaryTransitionReason::CrossfadeArmed);
    }

    tryAdvancePreparedBoundary();
}

void PlaybackCoordinator::handlePreparedGaplessArmResult(const Track& track, uint64_t generation, bool armed)
{
    if(!isPreparedArmResultRelevant(m_pendingBoundaryAdvanceGeneration, m_pendingBoundaryExpectedNextTrack,
                                    m_pendingBoundaryGaplessArmInFlight, track, generation)) {
        return;
    }

    m_pendingBoundaryGaplessArmInFlight = false;

    qCDebug(PLAYBACK) << "Prepared boundary gapless arm result:"
                      << "nextTrackId=" << track.id() << "generation=" << generation << "armed=" << armed;

    if(armed) {
        m_pendingBoundaryPreparedTransition = PreparedTransition::Gapless;
        m_pendingBoundaryReady              = true;
        markBoundaryTransition(BoundaryTransitionReason::GaplessArmed);
    }

    tryAdvancePreparedBoundary();
}

void PlaybackCoordinator::tryAdvancePreparedBoundary()
{
    if(!m_pendingBoundaryAdvanceGeneration.has_value() || !m_pendingBoundaryPrepareRequestId.has_value()) {
        return;
    }

    if(!m_pendingBoundaryReady && m_pendingBoundaryPreparedTransition == PreparedTransition::None) {
        return;
    }

    const Track currentTrack = m_playerController->currentTrack();
    if(!sameTrackIdentity(currentTrack, m_pendingBoundaryExpectedTrack)) {
        clearPendingBoundaryAdvance(BoundaryTransitionReason::ClearTrackMismatch);
        return;
    }

    const Track nextTrack = m_playerController->upcomingTrack();
    if(!sameTrackIdentity(nextTrack, m_pendingBoundaryExpectedNextTrack)) {
        clearPendingBoundaryAdvance(BoundaryTransitionReason::ClearNextMismatch);
        return;
    }

    const uint64_t generation = *m_pendingBoundaryAdvanceGeneration;

    if(m_pendingBoundaryPreparedTransition == PreparedTransition::None) {
        if(m_pendingBoundaryCrossfadeArmInFlight) {
            return;
        }

        if(!m_pendingBoundaryCrossfadeArmAttempted) {
            m_pendingBoundaryCrossfadeArmAttempted = true;
            m_pendingBoundaryCrossfadeArmInFlight  = true;
            m_engine->armPreparedCrossfadeTransition(m_pendingBoundaryExpectedNextTrack, generation);
            return;
        }
    }

    if(m_pendingBoundaryPreparedTransition == PreparedTransition::None) {
        if(m_pendingBoundaryGaplessArmInFlight) {
            return;
        }

        if(!m_pendingBoundaryGaplessArmAttempted) {
            m_pendingBoundaryGaplessArmAttempted = true;
            m_pendingBoundaryGaplessArmInFlight  = true;
            m_engine->armPreparedGaplessTransition(m_pendingBoundaryExpectedNextTrack, generation);
            return;
        }
    }

    if(!m_pendingBoundarySwitchGateOpen) {
        armBoundaryCommitWatchdog();
        return;
    }

    if(m_pendingBoundaryPreparedTransition == PreparedTransition::Gapless
       && m_pendingBoundarySwitchAnchor != SwitchAnchor::BoundarySignal) {
        return;
    }

    m_suppressedEndGeneration     = m_pendingBoundaryAdvanceGeneration;
    const auto preparedTransition = m_pendingBoundaryPreparedTransition;
    const auto anchor             = m_pendingBoundarySwitchAnchor;
    markBoundaryTransition(BoundaryTransitionReason::AdvancingPreparedTransition);
    clearPendingBoundaryAdvance(BoundaryTransitionReason::AdvancingPreparedTransition);
    m_playlistHandler->prepareUpcomingTrack();

    qCDebug(PLAYBACK) << "Prepared boundary advancing:" << "nextTrackId=" << nextTrack.id()
                      << "generation=" << generation << "anchor=" << static_cast<int>(anchor)
                      << "preparedTransition=" << static_cast<int>(preparedTransition);

    switch(preparedTransition) {
        case PreparedTransition::Gapless:
            m_playerController->advance(Player::AdvanceReason::PreparedCommit);
            break;
        case PreparedTransition::Crossfade:
            m_playerController->advance(Player::AdvanceReason::PreparedCrossfadeCommit);
            break;
        case PreparedTransition::None:
            m_playerController->advance(Player::AdvanceReason::PreparedCrossfade);
            break;
    }
}

void PlaybackCoordinator::armBoundaryCommitWatchdog()
{
    if(!m_pendingBoundaryAdvanceGeneration.has_value() || !m_pendingBoundaryPrepareRequestId.has_value()
       || m_pendingBoundaryPreparedTransition == PreparedTransition::None
       || m_pendingBoundarySwitchAnchor != SwitchAnchor::BoundarySignal) {
        m_pendingBoundaryWatchdogGeneration.reset();
        return;
    }

    const uint64_t generation           = *m_pendingBoundaryAdvanceGeneration;
    m_pendingBoundaryWatchdogGeneration = generation;
    const Track expectedTrack           = m_pendingBoundaryExpectedTrack;
    const Track expectedNextTrack       = m_pendingBoundaryExpectedNextTrack;

    markBoundaryTransition(BoundaryTransitionReason::WatchdogArmed);

    QTimer::singleShot(PreparedBoundaryWatchdogGraceMs, this, [this, generation, expectedTrack, expectedNextTrack]() {
        handleBoundaryCommitWatchdogTimeout(generation, expectedTrack, expectedNextTrack);
    });
}

void PlaybackCoordinator::handleBoundaryCommitWatchdogTimeout(uint64_t generation, const Track& expectedTrack,
                                                              const Track& expectedNextTrack)
{
    if(!m_pendingBoundaryWatchdogGeneration || *m_pendingBoundaryWatchdogGeneration != generation) {
        return;
    }

    if(!m_pendingBoundaryAdvanceGeneration || *m_pendingBoundaryAdvanceGeneration != generation) {
        return;
    }

    if(!sameTrackIdentity(m_pendingBoundaryExpectedTrack, expectedTrack)
       || !sameTrackIdentity(m_pendingBoundaryExpectedNextTrack, expectedNextTrack)) {
        return;
    }

    const Track currentTrack = m_playerController->currentTrack();
    const Track nextTrack    = m_playerController->upcomingTrack();
    if(!sameTrackIdentity(currentTrack, expectedTrack) || !sameTrackIdentity(nextTrack, expectedNextTrack)) {
        return;
    }

    if(m_pendingBoundaryPreparedTransition == PreparedTransition::None
       || m_pendingBoundarySwitchAnchor != SwitchAnchor::BoundarySignal) {
        return;
    }

    qCWarning(PLAYBACK) << "Prepared boundary watchdog timeout, retrying advance:"
                        << "trackId=" << expectedTrack.id() << "nextTrackId=" << expectedNextTrack.id()
                        << "generation=" << generation
                        << "preparedTransition=" << static_cast<int>(m_pendingBoundaryPreparedTransition);
    markBoundaryTransition(BoundaryTransitionReason::WatchdogRetryAdvance);

    m_pendingBoundarySwitchGateOpen = true;
    tryAdvancePreparedBoundary();

    if(!m_pendingBoundaryAdvanceGeneration || *m_pendingBoundaryAdvanceGeneration != generation) {
        return;
    }

    const auto preparedTransition = m_pendingBoundaryPreparedTransition;
    qCWarning(PLAYBACK) << "Prepared boundary watchdog forcing fallback advance:"
                        << "trackId=" << expectedTrack.id() << "nextTrackId=" << expectedNextTrack.id()
                        << "generation=" << generation << "preparedTransition=" << static_cast<int>(preparedTransition);
    markBoundaryTransition(BoundaryTransitionReason::WatchdogForcedFallbackAdvance);

    m_suppressedEndGeneration = m_pendingBoundaryAdvanceGeneration;
    clearPendingBoundaryAdvance(BoundaryTransitionReason::WatchdogForcedFallbackAdvance);
    m_playlistHandler->prepareUpcomingTrack();

    switch(preparedTransition) {
        case PreparedTransition::Gapless:
            m_playerController->advance(Player::AdvanceReason::PreparedCommit);
            break;
        case PreparedTransition::Crossfade:
            m_playerController->advance(Player::AdvanceReason::PreparedCrossfadeCommit);
            break;
        case PreparedTransition::None:
            m_playerController->advance(Player::AdvanceReason::PreparedCrossfade);
            break;
    }
}

void PlaybackCoordinator::advancePreparedAtBoundary(uint64_t generation, const Track& expectedTrack,
                                                    const Track& expectedNextTrack)
{
    m_pendingBoundaryAdvanceGeneration     = generation;
    m_pendingBoundaryPrepareRequestId      = {};
    m_pendingBoundaryExpectedTrack         = expectedTrack;
    m_pendingBoundaryExpectedNextTrack     = expectedNextTrack;
    m_pendingBoundaryReady                 = false;
    m_pendingBoundarySwitchGateOpen        = false;
    m_pendingBoundaryCrossfadeArmAttempted = false;
    m_pendingBoundaryCrossfadeArmInFlight  = false;
    m_pendingBoundaryGaplessArmAttempted   = false;
    m_pendingBoundaryGaplessArmInFlight    = false;
    m_pendingBoundaryPreparedTransition    = PreparedTransition::None;
    m_pendingBoundarySwitchAnchor          = SwitchAnchor::Unknown;
    markBoundaryTransition(BoundaryTransitionReason::AboutToFinishQueued);

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
            clearPendingBoundaryAdvance(BoundaryTransitionReason::ClearFromSegmentTimer);
            return;
        }

        const Track nextTrack = m_playerController->upcomingTrack();
        if(!sameTrackIdentity(nextTrack, expectedNextTrack)) {
            clearPendingBoundaryAdvance(BoundaryTransitionReason::ClearFromSegmentTimer);
            return;
        }

        if(!isMultiTrackFileTransition(expectedTrack, expectedNextTrack)) {
            clearPendingBoundaryAdvance(BoundaryTransitionReason::ClearFromSegmentTimer);
            return;
        }

        m_suppressedEndGeneration = generation;
        markBoundaryTransition(BoundaryTransitionReason::AdvancingPreparedTransition);
        clearPendingBoundaryAdvance(BoundaryTransitionReason::AdvancingPreparedTransition);
        m_playlistHandler->prepareUpcomingTrack();
        m_playerController->advance(Player::AdvanceReason::PreparedCrossfade);
    });
}

void PlaybackCoordinator::clearPendingBoundaryAdvance(const BoundaryTransitionReason reason)
{
    markBoundaryTransition(reason);
    m_pendingBoundaryAdvanceGeneration.reset();
    m_pendingBoundaryPrepareRequestId.reset();
    m_pendingBoundaryExpectedTrack         = {};
    m_pendingBoundaryExpectedNextTrack     = {};
    m_pendingBoundaryReady                 = false;
    m_pendingBoundarySwitchGateOpen        = false;
    m_pendingBoundaryCrossfadeArmAttempted = false;
    m_pendingBoundaryCrossfadeArmInFlight  = false;
    m_pendingBoundaryGaplessArmAttempted   = false;
    m_pendingBoundaryGaplessArmInFlight    = false;
    m_pendingBoundaryPreparedTransition    = PreparedTransition::None;
    m_pendingBoundarySwitchAnchor          = SwitchAnchor::Unknown;
    m_pendingBoundaryWatchdogGeneration.reset();
}

void PlaybackCoordinator::markBoundaryTransition(const BoundaryTransitionReason reason)
{
    m_pendingBoundaryLastReason = reason;
    const QString reasonName    = Utils::Enum::toString(reason);
    qCDebug(PLAYBACK) << "Boundary transition state change:"
                      << "reason=" << (reasonName.isEmpty() ? QStringLiteral("Unknown") : reasonName)
                      << "generation=" << (m_pendingBoundaryAdvanceGeneration ? *m_pendingBoundaryAdvanceGeneration : 0)
                      << "requestId=" << (m_pendingBoundaryPrepareRequestId ? *m_pendingBoundaryPrepareRequestId : 0)
                      << "preparedTransition=" << Utils::Enum::toString(m_pendingBoundaryPreparedTransition)
                      << "anchor=" << Utils::Enum::toString(m_pendingBoundarySwitchAnchor)
                      << "ready=" << m_pendingBoundaryReady << "gateOpen=" << m_pendingBoundarySwitchGateOpen;
}
} // namespace Fooyin
