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

    explicit PlaybackCoordinator(EngineHandler* engine, PlayerController* playerController,
                                 PlaylistHandler* playlistHandler, QObject* parent = nullptr);

    [[nodiscard]] static EndAction evaluateEndForAutoAdvance(const Track& currentTrack, const Track& endedTrack,
                                                             uint64_t generation,
                                                             std::optional<uint64_t>& suppressedGeneration);

private:
    void handleTrackAboutToFinish(const Engine::AboutToFinishContext& context);
    void handleTrackReadyToSwitch(const Engine::AboutToFinishContext& context);
    void handleTrackStatusContext(const Engine::TrackStatusContext& context);
    void handleNextTrackReadiness(const Track& track, bool ready, uint64_t requestId);
    void advancePreparedAtBoundary(uint64_t generation, const Track& expectedTrack, const Track& expectedNextTrack);
    void tryAdvancePreparedBoundary();
    void clearPendingBoundaryAdvance();

    EngineHandler* m_engine;
    PlayerController* m_playerController;
    PlaylistHandler* m_playlistHandler;

    std::optional<uint64_t> m_suppressedEndGeneration;
    std::optional<uint64_t> m_pendingBoundaryAdvanceGeneration;
    std::optional<uint64_t> m_pendingBoundaryPrepareRequestId;
    Track m_pendingBoundaryExpectedTrack;
    Track m_pendingBoundaryExpectedNextTrack;
    bool m_pendingBoundaryReady;
    bool m_pendingBoundarySwitchGateOpen;
};
} // namespace Fooyin
