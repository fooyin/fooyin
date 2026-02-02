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

#include "fadecontroller.h"

#include "core/engine/pipeline/audiopipelinefader.h"

namespace Fooyin {
FadeController::FadeController(AudioPipelineFader* pipelineFader)
    : m_pipelineFader{pipelineFader}
    , m_state{FadeState::Idle}
    , m_pendingManualFadeInMs{0}
    , m_manualFadeInPendingMs{0}
    , m_restoreFadeCurveAfterFade{false}
    , m_fadeOnNext{false}
    , m_resumeFadePending{false}
    , m_fadingTransitionId{0}
    , m_activeTransitionId{0}
    , m_manualFadeTransitionId{0}
    , m_activeFadeIsOut{false}
{ }

FadeState FadeController::state() const
{
    return m_state;
}

void FadeController::setState(FadeState state)
{
    m_state = state;
}

bool FadeController::fadeOnNext() const
{
    return m_fadeOnNext;
}

void FadeController::setFadeOnNext(bool enabled)
{
    m_fadeOnNext = enabled;
}

bool FadeController::hasPendingResumeFadeIn() const
{
    return m_resumeFadePending;
}

bool FadeController::isFading() const
{
    return m_pipelineFader->faderIsFading();
}

bool FadeController::hasPendingFade() const
{
    return m_state == FadeState::FadingToPause || m_state == FadeState::FadingToStop;
}

void FadeController::invalidateActiveFade()
{
    m_activeTransitionId     = 0;
    m_manualFadeTransitionId = 0;
    m_activeFadeIsOut        = false;
}

uint64_t FadeController::armActiveFade(const uint64_t transitionId, const bool fadeOut)
{
    if(transitionId == 0) {
        return 0;
    }

    m_activeTransitionId = transitionId;
    m_activeFadeIsOut    = fadeOut;
    return transitionId;
}

void FadeController::cancelAll(const Engine::FadeCurve pauseCurve)
{
    m_state                     = FadeState::Idle;
    m_pendingManualTrack        = {};
    m_pendingManualFadeInMs     = 0;
    m_manualFadeInPendingMs     = 0;
    m_restoreFadeCurveAfterFade = false;
    m_resumeFadePending         = false;
    m_fadingTransitionId        = 0;
    invalidateActiveFade();

    m_pipelineFader->faderStop();
    m_pipelineFader->faderReset();
    m_pipelineFader->setFaderCurve(pauseCurve);
}

void FadeController::cancelForReinit(const Engine::FadeCurve pauseCurve)
{
    if(m_state == FadeState::Idle && !m_pipelineFader->faderIsFading()) {
        return;
    }

    cancelAll(pauseCurve);
}

bool FadeController::handleManualChangeFade(const Track& track, bool fadingEnabled,
                                            const Engine::CrossfadingValues& crossfadingValues, bool isPlaying,
                                            const double volume, const uint64_t transitionId)
{
    if(!fadingEnabled) {
        return false;
    }

    if(crossfadingValues.manualChange.out <= 0 && crossfadingValues.manualChange.in <= 0) {
        return false;
    }

    if(isPlaying && crossfadingValues.manualChange.out > 0 && m_state != FadeState::FadingToManualChange) {
        m_pendingManualTrack        = track;
        m_pendingManualFadeInMs     = crossfadingValues.manualChange.in;
        m_state                     = FadeState::FadingToManualChange;
        m_restoreFadeCurveAfterFade = false;
        m_manualFadeTransitionId    = transitionId;
        m_pipelineFader->faderFadeOut(crossfadingValues.manualChange.out, volume, armActiveFade(transitionId, true));
        return true;
    }

    if(crossfadingValues.manualChange.in > 0) {
        m_manualFadeInPendingMs  = crossfadingValues.manualChange.in;
        m_manualFadeTransitionId = transitionId;
    }

    return false;
}

bool FadeController::applyPlayFade(Engine::PlaybackState prevState, bool fadingEnabled,
                                   const Engine::FadingValues& fadingValues, double volume)
{
    const bool wasFadingToPause = (m_state == FadeState::FadingToPause);
    const bool wasFadingToStop  = (m_state == FadeState::FadingToStop);
    const bool wasFadingOut     = (wasFadingToPause || wasFadingToStop);
    const bool fromStopped      = (prevState == Engine::PlaybackState::Stopped);

    if(m_state == FadeState::FadingToPause || m_state == FadeState::FadingToStop) {
        m_state = FadeState::Idle;
    }

    if(m_manualFadeInPendingMs > 0 && fadingEnabled) {
        const int fadeInMs          = std::exchange(m_manualFadeInPendingMs, 0);
        m_restoreFadeCurveAfterFade = true;
        m_pipelineFader->faderFadeIn(fadeInMs, volume, armActiveFade(m_manualFadeTransitionId, false));
        return true;
    }

    if(m_resumeFadePending) {
        m_resumeFadePending = false;
        if(fadingEnabled && fadingValues.pause.in > 0) {
            m_pipelineFader->setFaderCurve(fadingValues.pause.curve);
            m_pipelineFader->faderFadeIn(fadingValues.pause.in, volume, 0);
            return true;
        }
    }

    const auto& fadeInSpec = (wasFadingToStop || fromStopped) ? fadingValues.stop : fadingValues.pause;
    const bool shouldFadeIn
        = ((prevState == Engine::PlaybackState::Paused) || fromStopped || wasFadingOut || isFading()) && fadingEnabled
       && fadeInSpec.in > 0;

    if(shouldFadeIn) {
        m_pipelineFader->setFaderCurve(fadeInSpec.curve);
        m_pipelineFader->faderFadeIn(fadeInSpec.in, volume, 0);
        return true;
    }

    if(fromStopped) {
        m_pipelineFader->faderStop();
        m_pipelineFader->faderReset();
        m_resumeFadePending  = false;
        m_activeTransitionId = 0;
        m_activeFadeIsOut    = false;
    }

    return false;
}

bool FadeController::beginPauseFade(bool fadingEnabled, const Engine::FadingValues& fadingValues, double volume,
                                    uint64_t transportTransitionId)
{
    if(!fadingEnabled || fadingValues.pause.out <= 0) {
        return false;
    }

    if(m_state == FadeState::FadingToPause || m_state == FadeState::FadingToStop) {
        m_fadingTransitionId = transportTransitionId;
        return true;
    }

    m_state              = FadeState::FadingToPause;
    m_fadingTransitionId = transportTransitionId;
    m_pipelineFader->setFaderCurve(fadingValues.pause.curve);
    m_pipelineFader->faderFadeOut(fadingValues.pause.out, volume, armActiveFade(transportTransitionId, true));
    return true;
}

bool FadeController::beginStopFade(bool fadingEnabled, const Engine::FadingValues& fadingValues, double volume,
                                   Engine::PlaybackState playbackState, uint64_t transportTransitionId)
{
    if(!fadingEnabled || fadingValues.stop.out <= 0) {
        return false;
    }

    if(playbackState != Engine::PlaybackState::Playing) {
        return false;
    }

    if(m_state == FadeState::FadingToStop) {
        m_fadingTransitionId = transportTransitionId;
        return true;
    }

    if(m_state == FadeState::FadingToPause) {
        m_state              = FadeState::FadingToStop;
        m_fadingTransitionId = transportTransitionId;
        return true;
    }

    m_state              = FadeState::FadingToStop;
    m_fadingTransitionId = transportTransitionId;
    m_pipelineFader->setFaderCurve(fadingValues.stop.curve);
    m_pipelineFader->faderFadeOut(fadingValues.stop.out, volume, armActiveFade(transportTransitionId, true));
    return true;
}

FadeController::FadeResult FadeController::handlePipelineFadeEvent(bool fadeOut, uint64_t token)
{
    FadeResult result;

    if(token == 0 || token != m_activeTransitionId || fadeOut != m_activeFadeIsOut) {
        return result;
    }

    m_activeTransitionId = 0;
    m_activeFadeIsOut    = false;

    if(fadeOut) {
        m_fadeOnNext = false;

        const FadeState currentState = m_state;
        m_state                      = FadeState::Idle;

        switch(currentState) {
            case FadeState::FadingToStop:
                m_resumeFadePending          = false;
                result.stopImmediateNow      = true;
                result.transportTransitionId = m_fadingTransitionId;
                break;

            case FadeState::FadingToManualChange: {
                const Track pendingTrack = std::exchange(m_pendingManualTrack, {});
                const int pendingFadeIn  = std::exchange(m_pendingManualFadeInMs, 0);
                if(pendingTrack.isValid()) {
                    if(pendingFadeIn > 0) {
                        m_manualFadeInPendingMs = pendingFadeIn;
                    }
                    else {
                        result.setPauseCurve = true;
                    }
                    result.loadTrack = pendingTrack;
                }
                else {
                    m_restoreFadeCurveAfterFade = true;
                }
                break;
            }

            case FadeState::FadingToPause:
                m_resumeFadePending          = true;
                result.pauseNow              = true;
                result.transportTransitionId = m_fadingTransitionId;
                break;

            case FadeState::Idle:
                break;
        }
    }
    else {
        if(m_restoreFadeCurveAfterFade) {
            result.setPauseCurve        = true;
            m_restoreFadeCurveAfterFade = false;
        }
    }
    if(result.stopImmediateNow || result.pauseNow) {
        m_fadingTransitionId = 0;
    }

    return result;
}
} // namespace Fooyin
