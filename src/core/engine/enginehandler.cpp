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

#include "enginehandler.h"

#include "audioengine.h"
#include "enginehelpers.h"
#include <core/coresettings.h>
#include <core/engine/enginedefs.h>
#include <core/engine/fadingdefs.h>
#include <core/internalcoresettings.h>
#include <core/player/playercontroller.h>
#include <core/track.h>
#include <utils/settings/settingsmanager.h>

#include <QLoggingCategory>
#include <QPointer>
#include <QTimer>

#include <limits>
#include <utility>

Q_LOGGING_CATEGORY(ENG_HANDLER, "fy.engine")

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
Engine::PlaybackItem makePlaybackItem(const Track& track, uint64_t itemId)
{
    return {.track = track, .itemId = itemId};
}

bool samePlaybackItem(const Engine::PlaybackItem& lhs, const Engine::PlaybackItem& rhs)
{
    if(lhs.itemId != 0 && rhs.itemId != 0) {
        return lhs.itemId == rhs.itemId;
    }
    return sameTrackIdentity(lhs.track, rhs.track);
}

int engineOwnedTransitionWatchdogDelayMs(int bufferLengthMs)
{
    const int safeBufferMs = std::max(250, bufferLengthMs);
    return std::clamp(safeBufferMs / 2, 250, 2000);
}

int engineOwnedTransitionWatchdogHardLimitMs(int bufferLengthMs)
{
    const int safeBufferMs = std::max(250, bufferLengthMs);
    return std::clamp(safeBufferMs * 2, 1000, 6000);
}
} // namespace

EngineHandler::EngineHandler(std::shared_ptr<AudioLoader> audioLoader, PlayerController* playerController,
                             SettingsManager* settings, DspRegistry* dspRegistry, QObject* parent)
    : EngineController{parent}
    , m_playerController{playerController}
    , m_settings{settings}
    , m_visualisationService{std::make_unique<VisualisationService>(this)}
    , m_engine{new AudioEngine(std::move(audioLoader), settings, dspRegistry, m_visualisationService->backend())}
    , m_levelReadyRelayConnected{false}
    , m_pcmReadyRelayConnected{false}
    , m_currentTrackItemId{0}
    , m_engineOwnedTransitionItemId{9}
    , m_engineOwnedTransitionGen{0}
    , m_endAdvanceSuppressed{false}
    , m_pendingBoundaryAdvanceGen{0}
    , m_lastPreparedNextTrackReady{false}
    , m_nextPrepareTrackRequestId{1}
    , m_nextSeekRequestId{1}
{
    m_engine->moveToThread(&m_engineThread);

    QObject::connect(m_playerController, &PlayerController::positionMoved, this, &EngineHandler::dispatchSeek);

    QObject::connect(m_engine, &AudioEngine::trackAboutToFinish, this, [this](const Track& track, uint64_t generation) {
        Q_EMIT trackAboutToFinish(Engine::AboutToFinishContext{.track = track, .generation = generation});
    });
    QObject::connect(m_engine, &AudioEngine::trackReadyToSwitch, this, [this](const Track& track, uint64_t generation) {
        Q_EMIT trackReadyToSwitch(Engine::AboutToFinishContext{.track = track, .generation = generation});
    });
    QObject::connect(
        m_engine, &AudioEngine::trackBoundaryReached, this,
        [this](const Track& track, uint64_t generation, uint64_t remainingOutputMs, bool engineOwnsTransition) {
            handleTrackBoundaryReached(track, generation, remainingOutputMs, engineOwnsTransition);
            Q_EMIT trackBoundaryReached(Engine::AboutToFinishContext{.track                = track,
                                                                     .generation           = generation,
                                                                     .remainingOutputMs    = remainingOutputMs,
                                                                     .engineOwnsTransition = engineOwnsTransition});
        });
    QObject::connect(m_engine, &AudioEngine::finished, this, &EngineHandler::finished);
    QObject::connect(m_engine, &AudioEngine::positionContextChanged, this, &EngineHandler::handlePositionContext);
    QObject::connect(m_engine, &AudioEngine::positionChangedWithContext, this, &EngineHandler::handlePositionSample);
    QObject::connect(m_engine, &AudioEngine::seekPositionApplied, this, &EngineHandler::handleSeekApplied);
    QObject::connect(m_engine, &AudioEngine::bitrateChanged, m_playerController, &PlayerController::setBitrate);
    QObject::connect(m_engine, &AudioEngine::stateChanged, this, &EngineHandler::handleStateChange);
    QObject::connect(m_engine, &AudioEngine::deviceError, this, &EngineController::engineError);
    QObject::connect(m_engine, &AudioEngine::trackChanged, this, &EngineHandler::handleEngineTrackChanged);
    QObject::connect(m_engine, &AudioEngine::trackCommitted, this, &EngineHandler::handleTrackCommitted);
    QObject::connect(m_engine, &AudioEngine::trackStatusContextChanged, this, &EngineHandler::handleTrackStatus);
    QObject::connect(m_engine, &AudioEngine::nextTrackReadiness, this, &EngineHandler::handleNextTrackReadiness);
    QObject::connect(m_visualisationService.get(), &VisualisationService::sessionActivityChanged, this, [this]() {
        QMetaObject::invokeMethod(this, &EngineHandler::updateAnalysisRelays, Qt::QueuedConnection);
    });

    QObject::connect(m_playerController, &PlayerController::trackChangeRequested, this,
                     &EngineHandler::handleTrackChangeRequest);
    QObject::connect(m_playerController, &PlayerController::upcomingTrackChanged, this,
                     &EngineHandler::handleUpcomingTrackChanged);
    QObject::connect(m_playerController, &PlayerController::trackEndAutoTransitionsEnabledChanged, this,
                     [this](bool enabled) {
                         clearPendingBoundaryAdvance();
                         clearEngineOwnedTransition();
                         dispatchCommand(&AudioEngine::setTrackEndAutoTransitionEnabled, enabled);
                     });

    m_engineThread.start();

    updateAnalysisRelays();

    QObject::connect(m_engine, &AudioEngine::volumeChanged, this, [this](double volume) {
        if(!qFuzzyCompare(m_settings->value<Settings::Core::OutputVolume>() + 1.0, volume + 1.0)) {
            m_settings->set<Settings::Core::OutputVolume>(volume);
        }
    });

    dispatchCommand(&AudioEngine::setTrackEndAutoTransitionEnabled,
                    m_playerController->trackEndAutoTransitionsEnabled());

    updateVolume(m_settings->value<Settings::Core::OutputVolume>());

    QObject::connect(m_playerController, &PlayerController::transportPlayRequested, this, &EngineHandler::requestPlay);
    QObject::connect(m_playerController, &PlayerController::transportPauseRequested, this,
                     &EngineHandler::requestPause);
    QObject::connect(m_playerController, &PlayerController::transportStopRequested, this, &EngineHandler::requestStop);

    QObject::connect(this, &EngineHandler::outputChanged, this, [this](const QString& output, const QString& device) {
        if(m_outputs.contains(output)) {
            dispatchCommand(&AudioEngine::setAudioOutput, m_outputs.at(output), device);
        }
    });
    QObject::connect(this, &EngineHandler::deviceChanged, this,
                     [this](const QString& device) { dispatchCommand(&AudioEngine::setOutputDevice, device); });

    m_settings->subscribe<Settings::Core::AudioOutput>(this, &EngineHandler::changeOutput);
    m_settings->subscribe<Settings::Core::OutputVolume>(this, &EngineHandler::updateVolume);

    m_settings->subscribe<Settings::Core::Shutdown>(this, &EngineHandler::savePlaybackState);
    m_pendingStartupRestore = readStartupRestoreState();
}

EngineHandler::~EngineHandler()
{
    m_engine->deleteLater();
    m_engineThread.quit();
    m_engineThread.wait();
}

void EngineHandler::connectNotify(const QMetaMethod& signal)
{
    EngineController::connectNotify(signal);

    if(signal == QMetaMethod::fromSignal(&EngineController::levelReady)
       || signal == QMetaMethod::fromSignal(&EngineController::pcmReady)) {
        QMetaObject::invokeMethod(this, &EngineHandler::updateAnalysisRelays, Qt::QueuedConnection);
    }
}

void EngineHandler::disconnectNotify(const QMetaMethod& signal)
{
    EngineController::disconnectNotify(signal);

    if(signal == QMetaMethod::fromSignal(&EngineController::levelReady)
       || signal == QMetaMethod::fromSignal(&EngineController::pcmReady)) {
        QMetaObject::invokeMethod(this, &EngineHandler::updateAnalysisRelays, Qt::QueuedConnection);
    }
}

void EngineHandler::publishEvent(const Engine::PlaybackItem& item, bool ready, uint64_t requestId)
{
    m_lastPreparedNextTrack      = item;
    m_lastPreparedNextTrackReady = ready;
    Q_EMIT nextTrackReadiness(item, ready, requestId);
}

uint64_t EngineHandler::nextPrepareRequestId()
{
    return m_nextPrepareTrackRequestId++;
}

Engine::NextTrackPrepareRequest EngineHandler::requestPrepareNextTrack(const Track& track)
{
    const uint64_t requestId = nextPrepareRequestId();
    const auto item          = sameTrackIdentity(track, m_upcomingTrack.track.track)
                                 ? makePlaybackItem(track, m_upcomingTrack.itemId)
                                 : makePlaybackItem(track, 0);
    const bool readyNow      = cachedNextTrackReadyFor(item);

    dispatchCommand(&AudioEngine::prepareNextTrack, item, requestId);

    return Engine::NextTrackPrepareRequest{
        .requestId = requestId,
        .readyNow  = readyNow,
    };
}

void EngineHandler::requestArmPreparedCrossfadeTransition(const Engine::PlaybackItem& item, uint64_t generation)
{
    QPointer<EngineHandler> self{this};

    QMetaObject::invokeMethod(
        m_engine,
        [engine = m_engine, self, item, generation]() {
            const bool armed = engine->armPreparedCrossfadeTransition(item, generation);
            if(!self) {
                return;
            }

            QMetaObject::invokeMethod(
                self,
                [self, item, generation, armed]() {
                    if(!self) {
                        return;
                    }
                    Q_EMIT self->preparedCrossfadeArmResult(item, generation, armed);
                },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
}

void EngineHandler::requestCommitPreparedCrossfadeTransition(const Engine::PlaybackItem& item, bool manualChange)
{
    QMetaObject::invokeMethod(
        m_engine,
        [engine = m_engine, item, manualChange]() {
            if(engine->commitPreparedCrossfadeTransition(item)) {
                return;
            }
            engine->loadTrack(item, manualChange);
        },
        Qt::QueuedConnection);
}

void EngineHandler::requestArmPreparedGaplessTransition(const Engine::PlaybackItem& item, uint64_t generation)
{
    QPointer<EngineHandler> self{this};

    QMetaObject::invokeMethod(
        m_engine,
        [engine = m_engine, self, item, generation]() {
            const bool armed = engine->armPreparedGaplessTransition(item, generation);
            if(!self) {
                return;
            }

            QMetaObject::invokeMethod(
                self,
                [self, item, generation, armed]() {
                    if(!self) {
                        return;
                    }
                    Q_EMIT self->preparedGaplessArmResult(item, generation, armed);
                },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
}

void EngineHandler::requestCommitPreparedGaplessTransition(const Engine::PlaybackItem& item, bool manualChange)
{
    QMetaObject::invokeMethod(
        m_engine,
        [engine = m_engine, item, manualChange]() {
            if(engine->commitPreparedGaplessTransition(item)) {
                return;
            }
            engine->loadTrack(item, manualChange);
        },
        Qt::QueuedConnection);
}

void EngineHandler::handleStateChange(Engine::PlaybackState state)
{
    switch(state) {
        case Engine::PlaybackState::Error:
        case Engine::PlaybackState::Stopped:
            clearPositionAcceptanceFloor();
            m_playerController->syncPlayStateFromEngine(Player::PlayState::Stopped);
            break;
        case Engine::PlaybackState::Paused:
            m_playerController->syncPlayStateFromEngine(Player::PlayState::Paused);
            break;
        case Engine::PlaybackState::Playing:
            m_playerController->syncPlayStateFromEngine(Player::PlayState::Playing);
            break;
    }

    Q_EMIT engineStateChanged(state);
}

void EngineHandler::handleTrackChangeRequest(const Player::TrackChangeRequest& request)
{
    const Track& track = request.track.track;
    if(!track.isValid()) {
        return;
    }

    if(request.context.reason == Player::AdvanceReason::StartupRestore && m_pendingStartupRestore.has_value()) {
        m_pendingStartupRestoreItemId = request.itemId;
    }
    else if(m_pendingStartupRestore.has_value()) {
        clearStartupRestore();
    }

    clearNextTrackReadiness();
    clearPositionAcceptanceFloor();
    clearPendingBoundaryAdvance();
    clearEngineOwnedTransition();
    m_pendingTrackChange = request;
    dispatchCommand(&AudioEngine::loadTrack, makePlaybackItem(track, request.itemId), request.context.userInitiated);
}

void EngineHandler::handleUpcomingTrackChanged(const Player::UpcomingTrack& upcomingTrack)
{
    m_upcomingTrack = upcomingTrack;
    qCDebug(ENG_HANDLER) << "Controller upcoming track changed:"
                         << "currentTrackId=" << m_playerController->currentTrack().id()
                         << "currentItemId=" << m_currentTrackItemId
                         << "upcomingTrackId=" << upcomingTrack.track.track.id()
                         << "upcomingItemId=" << upcomingTrack.itemId << "isQueueTrack=" << upcomingTrack.isQueueTrack
                         << "stopAfterCurrent=" << !m_playerController->trackEndAutoTransitionsEnabled();
    dispatchCommand(&AudioEngine::setUpcomingTrackCandidate,
                    makePlaybackItem(upcomingTrack.track.track, upcomingTrack.itemId));
}

bool EngineHandler::hasAutoTrackEndTransitionEnabled() const
{
    if(!m_playerController->trackEndAutoTransitionsEnabled()) {
        return false;
    }

    if(m_settings->value<Settings::Core::GaplessPlayback>()) {
        return true;
    }

    const auto fadingValues = m_settings->value<Settings::Core::Internal::FadingValues>().value<Engine::FadingValues>();
    if(m_settings->value<Settings::Core::Internal::EngineFading>() && fadingValues.boundary.isConfigured()) {
        return true;
    }

    const auto crossfadingValues
        = m_settings->value<Settings::Core::Internal::CrossfadingValues>().value<Engine::CrossfadingValues>();
    return m_settings->value<Settings::Core::Internal::EngineCrossfading>()
        && crossfadingValues.autoChange.isConfigured();
}

bool EngineHandler::hasDistinctUpcomingTrack() const
{
    if(!m_upcomingTrack.track.isValid()) {
        return false;
    }

    const Track currentTrack  = m_playerController->currentTrack();
    const Track upcomingTrack = m_upcomingTrack.track.track;

    if(samePlaybackItem(makePlaybackItem(upcomingTrack, m_upcomingTrack.itemId),
                        makePlaybackItem(currentTrack, m_currentTrackItemId))) {
        return false;
    }

    if((m_playerController->playMode() & Playlist::RepeatTrack) && sameTrackSegment(upcomingTrack, currentTrack)) {
        return false;
    }

    return true;
}

void EngineHandler::noteEngineOwnedTransition(const Track& track, uint64_t generation)
{
    if(!hasAutoTrackEndTransitionEnabled() || !track.isValid() || !hasDistinctUpcomingTrack()
       || !sameTrackIdentity(m_playerController->currentTrack(), track)) {
        return;
    }

    m_engineOwnedTransitionTrack  = track;
    m_engineOwnedTransitionItemId = m_upcomingTrack.itemId;
    m_engineOwnedTransitionGen    = generation;
}

void EngineHandler::handleTrackBoundaryReached(const Track& track, uint64_t generation, uint64_t remainingOutputMs,
                                               bool engineOwnsTransition)
{
    qCDebug(ENG_HANDLER) << "Engine track boundary received:" << "trackId=" << track.id() << "generation=" << generation
                         << "currentTrackId=" << m_playerController->currentTrack().id()
                         << "upcomingTrackId=" << m_upcomingTrack.track.track.id()
                         << "currentItemId=" << m_currentTrackItemId << "upcomingItemId=" << m_upcomingTrack.itemId
                         << "remainingOutputMs=" << remainingOutputMs << "engineOwnsTransition=" << engineOwnsTransition
                         << "autoTransitionEnabled=" << hasAutoTrackEndTransitionEnabled();

    if(!sameTrackIdentity(m_playerController->currentTrack(), track)) {
        qCDebug(ENG_HANDLER) << "Ignoring boundary for non-current track:" << "trackId=" << track.id()
                             << "generation=" << generation
                             << "currentTrackId=" << m_playerController->currentTrack().id();
        return;
    }

    if(engineOwnsTransition && hasAutoTrackEndTransitionEnabled() && hasDistinctUpcomingTrack()) {
        noteEngineOwnedTransition(track, generation);
        return;
    }

    if(!engineOwnsTransition && hasAutoTrackEndTransitionEnabled() && hasDistinctUpcomingTrack()) {
        qCDebug(ENG_HANDLER) << "Boundary arrived without engine-owned transition, re-submitting upcoming track:"
                             << "trackId=" << track.id() << "generation=" << generation
                             << "upcomingTrackId=" << m_upcomingTrack.track.track.id()
                             << "upcomingItemId=" << m_upcomingTrack.itemId
                             << "remainingOutputMs=" << remainingOutputMs;
        const auto upcomingItem = makePlaybackItem(m_upcomingTrack.track.track, m_upcomingTrack.itemId);
        dispatchCommand(&AudioEngine::setUpcomingTrackCandidate, upcomingItem);
        dispatchCommand(&AudioEngine::prepareNextTrack, upcomingItem, uint64_t{0});
        noteEngineOwnedTransition(track, generation);
        return;
    }

    clearEngineOwnedTransition();
    if(remainingOutputMs == 0) {
        clearPendingBoundaryAdvance();
        m_playerController->advance(Player::AdvanceReason::NaturalEnd);
        return;
    }

    m_pendingBoundaryAdvanceTrack = track;
    m_pendingBoundaryAdvanceGen   = generation;

    qCDebug(ENG_HANDLER) << "Deferring controller natural-end advance until output boundary:"
                         << "trackId=" << track.id() << "generation=" << generation
                         << "remainingOutputMs=" << remainingOutputMs;

    const auto delay = std::chrono::milliseconds{
        std::min<uint64_t>(remainingOutputMs, static_cast<uint64_t>(std::numeric_limits<int>::max()))};
    QTimer::singleShot(delay, this, [this, track, generation]() {
        if(m_pendingBoundaryAdvanceGen != generation || !sameTrackIdentity(m_pendingBoundaryAdvanceTrack, track)
           || !sameTrackIdentity(m_playerController->currentTrack(), track) || m_pendingTrackChange.has_value()) {
            return;
        }

        clearPendingBoundaryAdvance();
        m_playerController->advance(Player::AdvanceReason::NaturalEnd);
    });
}

void EngineHandler::clearPendingBoundaryAdvance()
{
    m_pendingBoundaryAdvanceTrack = {};
    m_pendingBoundaryAdvanceGen   = 0;
}

void EngineHandler::armEndAdvanceWatchdog(const Track& track, const uint64_t generation)
{
    const int armBufferLengthMs = std::max(250, m_settings->value<Settings::Core::BufferLength>());
    const int armWatchdogMs     = engineOwnedTransitionWatchdogDelayMs(armBufferLengthMs);

    QTimer::singleShot(armWatchdogMs, this, [this, track, generation]() {
        if(!m_endAdvanceSuppressed || m_engineOwnedTransitionGen != generation
           || !sameTrackIdentity(m_engineOwnedTransitionTrack, track)
           || !sameTrackIdentity(m_playerController->currentTrack(), track)) {
            return;
        }

        const auto elapsedMs     = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                        std::chrono::steady_clock::now() - m_endAdvanceSuppressedSince)
                                                        .count());
        const int bufferLengthMs = std::max(250, m_settings->value<Settings::Core::BufferLength>());
        const int watchdogMs     = engineOwnedTransitionWatchdogDelayMs(bufferLengthMs);
        const int hardLimitMs    = engineOwnedTransitionWatchdogHardLimitMs(bufferLengthMs);
        const bool transitionTargetStillCurrentUpcoming
            = m_upcomingTrack.track.isValid() && m_upcomingTrack.itemId == m_engineOwnedTransitionItemId;
        const bool transitionTargetStillReady
            = transitionTargetStillCurrentUpcoming
           && cachedNextTrackReadyFor(makePlaybackItem(m_upcomingTrack.track.track, m_upcomingTrack.itemId));

        if(transitionTargetStillReady && elapsedMs < hardLimitMs) {
            qCDebug(ENG_HANDLER) << "Engine-owned transition watchdog extended while waiting for audible handoff:"
                                 << "trackId=" << track.id() << "generation=" << generation
                                 << "upcomingTrackId=" << m_upcomingTrack.track.track.id()
                                 << "upcomingItemId=" << m_upcomingTrack.itemId << "elapsedMs=" << elapsedMs
                                 << "watchdogMs=" << watchdogMs << "hardLimitMs=" << hardLimitMs;
            armEndAdvanceWatchdog(track, generation);
            return;
        }

        qCWarning(ENG_HANDLER) << "Engine-owned transition watchdog expired, resuming controller natural-end advance:"
                               << "trackId=" << track.id() << "generation=" << generation
                               << "upcomingTrackId=" << m_upcomingTrack.track.track.id()
                               << "upcomingItemId=" << m_upcomingTrack.itemId << "elapsedMs=" << elapsedMs
                               << "watchdogMs=" << watchdogMs << "hardLimitMs=" << hardLimitMs
                               << "nextTrackStillReady=" << transitionTargetStillReady;
        clearEngineOwnedTransition();
        m_playerController->advance(Player::AdvanceReason::NaturalEnd);
    });
}

void EngineHandler::clearEngineOwnedTransition()
{
    m_engineOwnedTransitionTrack  = {};
    m_engineOwnedTransitionItemId = 0;
    m_engineOwnedTransitionGen    = 0;
    m_endAdvanceSuppressed        = false;
    m_endAdvanceSuppressedSince   = {};
}

void EngineHandler::handleEngineTrackChanged(const Track& track)
{
    if(track.isValid()) {
        m_latestTrackMetadata = track;
    }

    Q_EMIT trackChanged(track);
}

void EngineHandler::handleTrackCommitted(const Engine::TrackCommitContext& context)
{
    clearPendingBoundaryAdvance();
    m_currentTrackItemId = context.itemId;

    if(!context.track.isValid()) {
        return;
    }

    Q_EMIT trackCommitted(context);

    if(m_pendingTrackChange.has_value()
       && samePlaybackItem(makePlaybackItem(m_pendingTrackChange->track.track, m_pendingTrackChange->itemId),
                           makePlaybackItem(context.track, context.itemId))) {
        m_playerController->commitCurrentTrack(*m_pendingTrackChange);

        if(m_latestTrackMetadata.isValid()
           && sameTrackIdentity(m_latestTrackMetadata, m_playerController->currentTrack())
           && !m_latestTrackMetadata.sameDataAs(m_playerController->currentTrack())) {
            m_playerController->updateCurrentTrack(m_latestTrackMetadata);
        }

        m_pendingTrackChange.reset();
        clearEngineOwnedTransition();

        if(m_pendingStartupRestore.has_value() && m_pendingStartupRestoreItemId == context.itemId) {
            const StartupRestoreState restore = *m_pendingStartupRestore;
            clearStartupRestore();
            applyStartupRestore(restore);
        }
        return;
    }

    if(m_upcomingTrack.track.isValid()
       && samePlaybackItem(makePlaybackItem(m_upcomingTrack.track.track, m_upcomingTrack.itemId),
                           makePlaybackItem(context.track, context.itemId))) {
        if(!m_playerController->trackEndAutoTransitionsEnabled()) {
            clearEngineOwnedTransition();
            m_playerController->advance(Player::AdvanceReason::NaturalEnd);
            return;
        }

        m_playerController->commitCurrentTrack(Player::TrackChangeRequest{
            .track        = m_upcomingTrack.track,
            .context      = {.reason = Player::AdvanceReason::NaturalEnd, .userInitiated = false},
            .isQueueTrack = m_upcomingTrack.isQueueTrack,
            .itemId       = m_upcomingTrack.itemId,
        });

        if(m_latestTrackMetadata.isValid()
           && sameTrackIdentity(m_latestTrackMetadata, m_playerController->currentTrack())
           && !m_latestTrackMetadata.sameDataAs(m_playerController->currentTrack())) {
            m_playerController->updateCurrentTrack(m_latestTrackMetadata);
        }

        clearEngineOwnedTransition();
    }
}

void EngineHandler::handleTrackStatus(Engine::TrackStatus status, const Track& track, uint64_t generation,
                                      bool seekable)
{
    m_playerController->setCurrentTrackSeekable(seekable);

    switch(status) {
        case Engine::TrackStatus::NoTrack:
            clearNextTrackReadiness();
            clearPositionAcceptanceFloor();
            clearPendingBoundaryAdvance();
            clearEngineOwnedTransition();
            m_pendingTrackChange.reset();
            clearStartupRestore();
            m_playerController->syncPlayStateFromEngine(Player::PlayState::Stopped);
            break;
        case Engine::TrackStatus::End:
            clearPositionAcceptanceFloor();
            qCDebug(ENG_HANDLER) << "Engine track-end status received:" << "trackId=" << track.id()
                                 << "generation=" << generation
                                 << "currentTrackId=" << m_playerController->currentTrack().id()
                                 << "upcomingTrackId=" << m_upcomingTrack.track.track.id()
                                 << "currentItemId=" << m_currentTrackItemId
                                 << "upcomingItemId=" << m_upcomingTrack.itemId
                                 << "engineOwnedTransitionTrackId=" << m_engineOwnedTransitionTrack.id()
                                 << "engineOwnedTransitionItemId=" << m_engineOwnedTransitionItemId
                                 << "engineOwnedTransitionGen=" << m_engineOwnedTransitionGen
                                 << "pendingBoundaryAdvanceTrackId=" << m_pendingBoundaryAdvanceTrack.id()
                                 << "pendingBoundaryAdvanceGen=" << m_pendingBoundaryAdvanceGen
                                 << "endAdvanceSuppressed=" << m_endAdvanceSuppressed;
            if(sameTrackIdentity(m_playerController->currentTrack(), track)
               && sameTrackIdentity(m_engineOwnedTransitionTrack, track) && m_engineOwnedTransitionGen == generation
               && m_engineOwnedTransitionItemId != 0) {
                qCDebug(ENG_HANDLER) << "Suppressing controller natural-end advance for engine-owned transition:"
                                     << "trackId=" << track.id() << "generation=" << generation
                                     << "upcomingTrackId=" << m_upcomingTrack.track.track.id()
                                     << "upcomingItemId=" << m_upcomingTrack.itemId;
                m_endAdvanceSuppressed      = true;
                m_endAdvanceSuppressedSince = std::chrono::steady_clock::now();
                armEndAdvanceWatchdog(track, generation);
                break;
            }
            qCDebug(ENG_HANDLER) << "Track-end status left to boundary/natural-end path:"
                                 << "trackId=" << track.id() << "generation=" << generation;
            break;
        case Engine::TrackStatus::Invalid:
            clearPositionAcceptanceFloor();
            clearPendingBoundaryAdvance();
            clearEngineOwnedTransition();
            if(m_pendingTrackChange.has_value()
               && m_pendingTrackChange->context.reason == Player::AdvanceReason::StartupRestore) {
                clearStartupRestore();
            }
            if(m_pendingTrackChange.has_value() && sameTrackIdentity(m_pendingTrackChange->track.track, track)) {
                // Failed loads never emit trackCommitted(), so adopt the attempted track here to
                // clear the pending reques.
                m_playerController->commitCurrentTrack(*m_pendingTrackChange);
                m_pendingTrackChange.reset();
            }
            break;
        case Engine::TrackStatus::Loading:
        case Engine::TrackStatus::Loaded:
        case Engine::TrackStatus::Buffering:
        case Engine::TrackStatus::Buffered:
        case Engine::TrackStatus::Unreadable:
            break;
    }

    Q_EMIT trackStatusContextChanged(
        Engine::TrackStatusContext{.status = status, .track = track, .generation = generation, .seekable = seekable});
}

void EngineHandler::requestPlay() const
{
    dispatchCommand(&AudioEngine::play);
}

void EngineHandler::requestPause() const
{
    dispatchCommand(&AudioEngine::pause);
}

void EngineHandler::requestStop() const
{
    dispatchCommand(&AudioEngine::stop);
}

void EngineHandler::updateAnalysisRelays()
{
    const bool hasLevelSubscribers      = isSignalConnected(QMetaMethod::fromSignal(&EngineController::levelReady));
    const bool hasPcmSubscribers        = isSignalConnected(QMetaMethod::fromSignal(&EngineController::pcmReady));
    const bool hasVisualisationSessions = m_visualisationService && m_visualisationService->hasActiveSessions();

    Engine::AnalysisDataTypes subscriptions;

    if(hasLevelSubscribers) {
        subscriptions.setFlag(Engine::AnalysisDataType::LevelFrameData);
    }
    if(hasPcmSubscribers || hasVisualisationSessions) {
        subscriptions.setFlag(Engine::AnalysisDataType::PcmFrameData);
    }

    if(hasLevelSubscribers && !m_levelReadyRelayConnected) {
        m_levelReadyRelayConnection
            = QObject::connect(m_engine, &AudioEngine::levelReady, this, &EngineController::levelReady);
        m_levelReadyRelayConnected = true;
    }
    else if(!hasLevelSubscribers && m_levelReadyRelayConnected) {
        QObject::disconnect(m_levelReadyRelayConnection);
        m_levelReadyRelayConnection = {};
        m_levelReadyRelayConnected  = false;
    }

    if(hasPcmSubscribers && !m_pcmReadyRelayConnected) {
        m_pcmReadyRelayConnection
            = QObject::connect(m_engine, &AudioEngine::pcmReady, this, &EngineController::pcmReady);
        m_pcmReadyRelayConnected = true;
    }
    else if(!hasPcmSubscribers && m_pcmReadyRelayConnected) {
        QObject::disconnect(m_pcmReadyRelayConnection);
        m_pcmReadyRelayConnection = {};
        m_pcmReadyRelayConnected  = false;
    }

    dispatchCommand(&AudioEngine::setAnalysisDataSubscriptions, subscriptions);
}

VisualisationService* EngineHandler::visualisationService() const
{
    return m_visualisationService.get();
}

void EngineHandler::changeOutput(const QString& output)
{
    auto loadDefault = [this]() {
        if(m_outputs.empty()) {
            return;
        }
        m_currentOutput = {.name = m_outputs.cbegin()->first, .device = u"default"_s};
        Q_EMIT outputChanged(m_currentOutput.name, m_currentOutput.device);
    };

    if(output.isEmpty()) {
        if(m_outputs.empty() || !m_currentOutput.name.isEmpty()) {
            return;
        }
        loadDefault();
        return;
    }

    const auto newOutput = output.split(u"|"_s);

    if(newOutput.empty() || newOutput.size() < 2) {
        return;
    }

    const QString& newName = newOutput.at(0);
    const QString& device  = newOutput.at(1);

    if(m_outputs.empty()) {
        qCWarning(ENG_HANDLER) << "No Outputs have been registered";
        return;
    }

    if(!m_outputs.contains(newName)) {
        qCWarning(ENG_HANDLER) << "Output hasn't been registered:" << newName;
        loadDefault();
        return;
    }

    if(m_currentOutput.name != newName) {
        m_currentOutput = {.name = newName, .device = device};
        Q_EMIT outputChanged(newName, device);
    }
    else if(m_currentOutput.device != device) {
        m_currentOutput.device = device;
        Q_EMIT deviceChanged(device);
    }
}

void EngineHandler::updateVolume(double volume)
{
    dispatchCommand(&AudioEngine::setVolume, volume);
}

void EngineHandler::updateCurrentTrackMetadata(const Track& track)
{
    dispatchCommand(&AudioEngine::updateCurrentTrackMetadata, track);
}

void EngineHandler::dispatchSeek(uint64_t positionMs)
{
    if(!m_playerController->currentTrackSeekable()) {
        return;
    }

    uint64_t requestId = m_nextSeekRequestId++;

    if(requestId == 0) {
        requestId = m_nextSeekRequestId++;
    }

    if(m_nextSeekRequestId == 0) {
        m_nextSeekRequestId = 1;
    }

    m_positionAcceptanceFloor = PositionContext{
        .trackGeneration = m_positionContextWatermark.trackGeneration,
        .seekRequestId   = requestId,
        .timelineEpoch   = m_positionContextWatermark.timelineEpoch,
    };

    dispatchCommand(&AudioEngine::seekWithRequest, positionMs, requestId);
}

void EngineHandler::updatePosition(uint64_t ms) const
{
    m_playerController->setCurrentPosition(ms);
}

void EngineHandler::handlePositionSample(uint64_t positionMs, uint64_t trackGeneration, uint64_t timelineEpoch,
                                         uint64_t seekRequestId)
{
    const PositionContext sampleContext{
        .trackGeneration = trackGeneration,
        .seekRequestId   = seekRequestId,
        .timelineEpoch   = timelineEpoch,
    };

    // Reject samples that predate any already observed context
    if(contextLess(sampleContext, m_positionContextWatermark)) {
        return;
    }

    if(m_positionAcceptanceFloor.has_value() && seekRequestId < m_positionAcceptanceFloor->seekRequestId) {
        return;
    }

    if(m_positionAcceptanceFloor.has_value()) {
        m_positionAcceptanceFloor.reset();
    }

    advancePositionContextWatermark(sampleContext);
    updatePosition(positionMs);
}

void EngineHandler::handlePositionContext(uint64_t trackGeneration, uint64_t timelineEpoch, uint64_t seekRequestId)
{
    const PositionContext nextContext{
        .trackGeneration = trackGeneration,
        .seekRequestId   = seekRequestId,
        .timelineEpoch   = timelineEpoch,
    };
    advancePositionContextWatermark(nextContext);
}

void EngineHandler::handleSeekApplied(uint64_t positionMs, uint64_t requestId)
{
    if(requestId == 0) {
        return;
    }

    if(m_positionAcceptanceFloor.has_value() && requestId < m_positionAcceptanceFloor->seekRequestId) {
        return;
    }

    if(m_positionAcceptanceFloor.has_value() && requestId >= m_positionAcceptanceFloor->seekRequestId) {
        m_positionAcceptanceFloor.reset();
    }

    advancePositionContextWatermark({
        .trackGeneration = m_positionContextWatermark.trackGeneration,
        .seekRequestId   = requestId,
        .timelineEpoch   = m_positionContextWatermark.timelineEpoch,
    });
    updatePosition(positionMs);

    if(hasAutoTrackEndTransitionEnabled() && m_upcomingTrack.track.isValid()) {
        dispatchCommand(&AudioEngine::setUpcomingTrackCandidate,
                        makePlaybackItem(m_upcomingTrack.track.track, m_upcomingTrack.itemId));
    }
}

void EngineHandler::clearPositionAcceptanceFloor()
{
    m_positionAcceptanceFloor.reset();
}

bool EngineHandler::contextLess(const PositionContext& lhs, const PositionContext& rhs)
{
    if(lhs.trackGeneration != rhs.trackGeneration) {
        return lhs.trackGeneration < rhs.trackGeneration;
    }
    if(lhs.seekRequestId != rhs.seekRequestId) {
        return lhs.seekRequestId < rhs.seekRequestId;
    }
    return lhs.timelineEpoch < rhs.timelineEpoch;
}

void EngineHandler::advancePositionContextWatermark(const PositionContext& context)
{
    if(contextLess(m_positionContextWatermark, context)) {
        m_positionContextWatermark = context;
    }
}

void EngineHandler::handleNextTrackReadiness(const Engine::PlaybackItem& item, bool ready, uint64_t requestId)
{
    qCDebug(ENG_HANDLER) << "Next-track readiness updated:" << "trackId=" << item.track.id() << "ready=" << ready
                         << "itemId=" << item.itemId << "requestId=" << requestId;
    publishEvent(item, ready, requestId);

    if(!ready && m_endAdvanceSuppressed && m_upcomingTrack.track.isValid()
       && samePlaybackItem(item, makePlaybackItem(m_upcomingTrack.track.track, m_upcomingTrack.itemId))
       && sameTrackIdentity(m_playerController->currentTrack(), m_engineOwnedTransitionTrack)) {
        qCDebug(ENG_HANDLER) << "Engine-owned transition was not ready, resuming controller natural-end advance:"
                             << "currentTrackId=" << m_engineOwnedTransitionTrack.id()
                             << "nextTrackId=" << item.track.id() << "nextItemId=" << item.itemId;
        clearEngineOwnedTransition();
        m_playerController->advance(Player::AdvanceReason::NaturalEnd);
    }
}

bool EngineHandler::cachedNextTrackReadyFor(const Engine::PlaybackItem& item) const
{
    return m_lastPreparedNextTrackReady && samePlaybackItem(m_lastPreparedNextTrack, item);
}

void EngineHandler::clearNextTrackReadiness()
{
    m_lastPreparedNextTrack      = {};
    m_lastPreparedNextTrackReady = false;
}

void EngineHandler::savePlaybackState() const
{
    if(m_settings->fileValue(Settings::Core::Internal::SaveActivePlaylistState, false).toBool()) {
        const auto lastPos = static_cast<quint64>(m_playerController->currentPosition());
        PlaybackState::savePlaybackPosition(lastPos);

        if(m_playerController->currentTrack().isValid() && !m_playerController->playedThresholdReached()) {
            PlaybackState::savePlaybackTimeListened(m_playerController->currentTimeListened());
        }
        else {
            PlaybackState::clearPlaybackTimeListened();
        }
    }
    else {
        PlaybackState::clearPlaybackPosition();
        PlaybackState::clearPlaybackTimeListened();
    }

    if(m_settings->fileValue(Settings::Core::Internal::SavePlaybackState, false).toBool()) {
        PlaybackState::savePlaybackState(m_playerController->playState());
    }
    else {
        PlaybackState::clearPlaybackState();
    }
}

std::optional<EngineHandler::StartupRestoreState> EngineHandler::readStartupRestoreState() const
{
    if(!m_settings->fileValue(Settings::Core::Internal::SaveActivePlaylistState, false).toBool()) {
        return {};
    }
    if(!m_settings->fileValue(Settings::Core::Internal::SavePlaybackState, false).toBool()) {
        return {};
    }

    const auto state = PlaybackState::playbackState();
    if(!state.has_value()) {
        return {};
    }

    const auto pos = PlaybackState::playbackPosition();
    if(!pos.has_value()) {
        return {};
    }

    const auto timeListened = PlaybackState::playbackTimeListened();

    return StartupRestoreState{
        .playState      = *state,
        .positionMs     = *pos,
        .timeListenedMs = timeListened,
    };
}

void EngineHandler::applyStartupRestore(const StartupRestoreState& restore)
{
    const auto restoreProgress = [this, &restore](bool pause) {
        if(restore.timeListenedMs.has_value()) {
            m_playerController->restorePlaybackProgress(restore.positionMs, *restore.timeListenedMs);
            dispatchCommand(&AudioEngine::restorePosition, restore.positionMs, pause);
            return;
        }

        restorePosition(restore.positionMs, pause);
    };

    switch(restore.playState) {
        case Player::PlayState::Paused:
            qCDebug(ENG_HANDLER) << "Restoring paused state…";
            restoreProgress(true);
            break;
        case Player::PlayState::Playing:
            qCDebug(ENG_HANDLER) << "Restoring playing state…";
            if(restore.positionMs > 0) {
                restoreProgress(false);
            }
            m_playerController->play();
            break;
        case Player::PlayState::Stopped:
            qCDebug(ENG_HANDLER) << "Restoring stopped state…";
            break;
    }
}

void EngineHandler::clearStartupRestore()
{
    m_pendingStartupRestore.reset();
    m_pendingStartupRestoreItemId.reset();
}

void EngineHandler::setup()
{
    changeOutput(m_settings->value<Settings::Core::AudioOutput>());
}

void EngineHandler::applyOutputProfile(const Engine::OutputProfileRequest& request)
{
    if(request.output.isEmpty() || request.device.isEmpty()) {
        return;
    }

    if(!m_outputs.contains(request.output)) {
        qCWarning(ENG_HANDLER) << "Output hasn't been registered:" << request.output;
        return;
    }

    m_currentOutput = {.name = request.output, .device = request.device};
    dispatchCommand(&AudioEngine::applyOutputProfile, m_outputs.at(request.output), request.device, request.bitDepth,
                    request.dither, request.chain);
}

void EngineHandler::setDspChain(const Engine::DspChains& chain)
{
    dispatchCommand(&AudioEngine::setDspChain, chain);
}

void EngineHandler::updateLiveDspSettings(const Engine::LiveDspSettingsUpdate& update)
{
    dispatchCommand(&AudioEngine::updateLiveDspSettings, update);
}

void EngineHandler::restorePosition(uint64_t positionMs, bool pause) const
{
    m_playerController->restoreCurrentPosition(positionMs);
    dispatchCommand(&AudioEngine::restorePosition, positionMs, pause);
}

Engine::NextTrackPrepareRequest EngineHandler::prepareNextTrackForPlayback(const Track& track)
{
    return requestPrepareNextTrack(track);
}

void EngineHandler::armPreparedCrossfadeTransition(const Track& track, uint64_t generation)
{
    const auto item = sameTrackIdentity(track, m_upcomingTrack.track.track)
                        ? makePlaybackItem(track, m_upcomingTrack.itemId)
                        : makePlaybackItem(track, 0);
    requestArmPreparedCrossfadeTransition(item, generation);
}

void EngineHandler::armPreparedGaplessTransition(const Track& track, uint64_t generation)
{
    const auto item = sameTrackIdentity(track, m_upcomingTrack.track.track)
                        ? makePlaybackItem(track, m_upcomingTrack.itemId)
                        : makePlaybackItem(track, 0);
    requestArmPreparedGaplessTransition(item, generation);
}

Engine::PlaybackState EngineHandler::engineState() const
{
    return m_engine->playbackState();
}

OutputNames EngineHandler::getAllOutputs() const
{
    OutputNames outputs;
    outputs.reserve(m_outputs.size());

    for(const auto& [name, output] : m_outputs) {
        outputs.emplace_back(name);
    }

    return outputs;
}

OutputDevices EngineHandler::getOutputDevices(const QString& output) const
{
    if(!m_outputs.contains(output)) {
        qCWarning(ENG_HANDLER) << "Output" << output << "not found";
        return {};
    }

    if(auto out = m_outputs.at(output)()) {
        const bool isCurrent
            = m_engine->playbackState() != Engine::PlaybackState::Stopped && m_currentOutput.name == output;
        return out->getAllDevices(isCurrent);
    }

    return {};
}

void EngineHandler::addOutput(const QString& name, OutputCreator output)
{
    if(m_outputs.contains(name)) {
        qCWarning(ENG_HANDLER) << "Output" << name << "already registered";
        return;
    }
    m_outputs.emplace(name, std::move(output));
}
} // namespace Fooyin
