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

#include <QObject>

#include <optional>

namespace Fooyin {
class EngineHandler;
class PlayerController;
class PlaylistHandler;
class SettingsManager;
class Track;

class FYCORE_EXPORT PlaybackCoordinator : public QObject
{
    Q_OBJECT

public:
    enum class EndAction : uint8_t
    {
        Advance = 0,
        Suppress,
        IgnoreStale,
    };

    enum class SwitchAnchor : uint8_t
    {
        Unknown = 0,
        ReadySignal,
        BoundarySignal,
    };
    Q_ENUM(SwitchAnchor)

    enum class PreparedTransition : uint8_t
    {
        None = 0,
        Gapless,
        Crossfade,
    };
    Q_ENUM(PreparedTransition)

    enum class BoundaryTransitionReason : uint8_t
    {
        Unknown = 0,
        AboutToFinishQueued,
        ReadySignalAnchorSet,
        BoundarySignalAnchorSet,
        ReadinessUpdated,
        CrossfadeArmed,
        GaplessArmed,
        AdvancingPreparedTransition,
        WatchdogArmed,
        WatchdogRetryAdvance,
        WatchdogForcedFallbackAdvance,
        ClearFromTrackEnd,
        ClearFromNoTrack,
        ClearTrackMismatch,
        ClearNextMismatch,
        ClearFromSegmentTimer,
    };
    Q_ENUM(BoundaryTransitionReason)

    explicit PlaybackCoordinator(EngineHandler* engine, PlayerController* playerController,
                                 PlaylistHandler* playlistHandler, SettingsManager* settings,
                                 QObject* parent = nullptr);

    [[nodiscard]] static EndAction evaluateEndForAutoAdvance(const Track& currentTrack, const Track& endedTrack,
                                                             uint64_t generation,
                                                             std::optional<uint64_t>& suppressedGeneration);
    [[nodiscard]] static bool isPreparedArmResultRelevant(std::optional<uint64_t> pendingGeneration,
                                                          const Track& expectedNextTrack, bool armInFlight,
                                                          const Track& resultTrack, uint64_t resultGeneration);

private:
    void handleTrackAboutToFinish(const Engine::AboutToFinishContext& context);
    void handleTrackReadyToSwitch(const Engine::AboutToFinishContext& context);
    void handleTrackBoundaryReached(const Engine::AboutToFinishContext& context);
    void handleTrackStatusContext(const Engine::TrackStatusContext& context);
    void handleNextTrackReadiness(const Track& track, bool ready, uint64_t requestId);
    void handlePreparedCrossfadeArmResult(const Track& track, uint64_t generation, bool armed);
    void handlePreparedGaplessArmResult(const Track& track, uint64_t generation, bool armed);
    void advancePreparedAtBoundary(uint64_t generation, const Track& expectedTrack, const Track& expectedNextTrack);
    void tryAdvancePreparedBoundary();
    void armBoundaryCommitWatchdog();
    void handleBoundaryCommitWatchdogTimeout(uint64_t generation, const Track& expectedTrack,
                                             const Track& expectedNextTrack);
    void clearPendingBoundaryAdvance(BoundaryTransitionReason reason);
    void markBoundaryTransition(BoundaryTransitionReason reason);

    EngineHandler* m_engine;
    PlayerController* m_playerController;
    PlaylistHandler* m_playlistHandler;

    std::optional<uint64_t> m_suppressedEndGeneration;

    std::optional<uint64_t> m_pendingBoundaryAdvanceGeneration;
    std::optional<uint64_t> m_pendingBoundaryPrepareRequestId;
    Track m_pendingBoundaryExpectedTrack;
    Track m_pendingBoundaryExpectedNextTrack;

    Engine::CrossfadeSwitchPolicy m_crossfadeSwitchPolicy;

    bool m_pendingBoundaryReady;
    bool m_pendingBoundarySwitchGateOpen;
    bool m_pendingBoundaryCrossfadeArmAttempted;
    bool m_pendingBoundaryCrossfadeArmInFlight;
    bool m_pendingBoundaryGaplessArmAttempted;
    bool m_pendingBoundaryGaplessArmInFlight;

    PreparedTransition m_pendingBoundaryPreparedTransition;
    SwitchAnchor m_pendingBoundarySwitchAnchor;
    std::optional<uint64_t> m_pendingBoundaryWatchdogGeneration;
    BoundaryTransitionReason m_pendingBoundaryLastReason;
};
} // namespace Fooyin
