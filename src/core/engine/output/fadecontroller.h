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

#include "fycore_export.h"

#include <core/engine/enginedefs.h>
#include <core/track.h>

#include <optional>

namespace Fooyin {
class AudioPipelineFader;

enum class FadeState : uint8_t
{
    Idle = 0,
    FadingToStop,
    FadingToPause,
    FadingToManualChange
};

/*!
 * Engine-thread fade controller.
 *
 * Coordinates transport fade intent with OutputFader callbacks and returns
 * actions for AudioEngine to execute.
 */
class FYCORE_EXPORT FadeController
{
public:
    struct FadeResult
    {
        bool pauseNow{false};
        bool stopImmediateNow{false};
        bool setPauseCurve{false};
        uint64_t transportTransitionId{0};
        std::optional<Track> loadTrack;
    };

    explicit FadeController(AudioPipelineFader* pipelineFader);

    [[nodiscard]] FadeState state() const;
    void setState(FadeState state);

    [[nodiscard]] bool fadeOnNext() const;
    void setFadeOnNext(bool enabled);
    [[nodiscard]] bool hasPendingResumeFadeIn() const;

    [[nodiscard]] bool isFading() const;
    [[nodiscard]] bool hasPendingFade() const;
    void invalidateActiveFade();

    void cancelAll(Engine::FadeCurve pauseCurve);
    //! Cancel active fades while preserving reinit-safe pipeline state.
    void cancelForReinit(Engine::FadeCurve pauseCurve);

    //! Start or queue manual track-change fades.
    //! Returns true when fade handling took ownership of the transition.
    bool handleManualChangeFade(const Track& track, bool fadingEnabled,
                                const Engine::CrossfadingValues& crossfadingValues, bool isPlaying, double volume,
                                uint64_t transitionId);

    //! Apply play/resume fade-in policy based on previous transport state.
    bool applyPlayFade(Engine::PlaybackState prevState, bool fadingEnabled, const Engine::FadingValues& fadingValues,
                       double volume);

    //! Begin pause fade-out if configured.
    bool beginPauseFade(bool fadingEnabled, const Engine::FadingValues& fadingValues, double volume,
                        uint64_t transportTransitionId = 0);

    //! Begin stop fade-out if configured and currently playing.
    bool beginStopFade(bool fadingEnabled, const Engine::FadingValues& fadingValues, double volume,
                       Engine::PlaybackState playbackState, uint64_t transportTransitionId = 0);

    //! Consume pipeline fade completion and map to engine actions.
    [[nodiscard]] FadeResult handlePipelineFadeEvent(bool fadeOut, uint64_t token);

private:
    [[nodiscard]] uint64_t armActiveFade(uint64_t transitionId, bool fadeOut);

    AudioPipelineFader* m_pipelineFader;

    FadeState m_state;
    Track m_pendingManualTrack;
    int m_pendingManualFadeInMs;
    int m_manualFadeInPendingMs;
    bool m_restoreFadeCurveAfterFade;
    bool m_fadeOnNext;
    bool m_resumeFadePending;
    uint64_t m_fadingTransitionId;
    uint64_t m_activeTransitionId;
    uint64_t m_manualFadeTransitionId;
    bool m_activeFadeIsOut;
};
} // namespace Fooyin
