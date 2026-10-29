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

#include <QObject>

#include <cstdint>
#include <optional>

namespace Fooyin {
/*!
 * Central coordinator for end-of-track, seek, and overlap-transition state.
 */
class FYCORE_EXPORT PlaybackTransitionCoordinator
{
    Q_GADGET

public:
    PlaybackTransitionCoordinator();

    enum class AutoTransitionMode : uint8_t
    {
        None = 0,
        Gapless,
        Crossfade,
        BoundaryFade
    };
    Q_ENUM(AutoTransitionMode)

    struct InitialSeekRequest
    {
        uint64_t positionMs{0};
        uint64_t requestId{0};
    };

    struct TrackEndingResult
    {
        bool aboutToFinish{false};
        bool readyToSwitch{false};
        bool boundaryReached{false};
        bool endReached{false};
        uint64_t remainingOutputMs{0};
    };

    struct TrackEndingInput
    {
        uint64_t positionMs{0};
        uint64_t durationMs{0};
        bool durationBoundaryEnabled{true};
        bool predictiveTimelineHintsEnabled{true};
        uint64_t remainingOutputMs{0};
        bool endOfInput{false};
        bool bufferEmpty{false};
        bool autoCrossfadeEnabled{false};
        bool boundaryFadeEnabled{false};
        bool gaplessEnabled{false};
        int autoFadeOutMs{0};
        int autoFadeInMs{0};
        int boundaryFadeOutMs{0};
        uint64_t gaplessPrepareWindowMs{0};
        uint64_t timelineDelayMs{0};
    };

    [[nodiscard]] TrackEndingResult evaluateTrackEnding(const TrackEndingInput& input);

    [[nodiscard]] bool isReadyForAutoTransition() const;
    [[nodiscard]] AutoTransitionMode autoTransitionMode() const;
    void clearAutoTransitionReadiness();

    [[nodiscard]] bool isReadyForAutoCrossfade() const;
    [[nodiscard]] bool isReadyForGaplessHandoff() const;

    //! Reset transition/end-of-track state on full stop.
    void clearForStop();
    void clearTrackEnding();

    [[nodiscard]] bool isSeekInProgress() const;
    void setSeekInProgress(bool inProgress);

    //! Queue initial seek to apply once matching track becomes active.
    void queueInitialSeek(uint64_t positionMs, int trackId, uint64_t requestId = 0);
    [[nodiscard]] bool hasPendingInitialSeekForTrack(int trackId) const;
    //! Consume the pending request, discarding it if the track does not match.
    [[nodiscard]] std::optional<InitialSeekRequest> takePendingInitialSeekForTrack(int trackId);

private:
    bool shouldSignalTrackEnding(const TrackEndingInput& input);
    [[nodiscard]] bool shouldSignalReadyToSwitch(const TrackEndingInput& input) const;
    static bool inTimelineWindow(const TrackEndingInput& input, uint64_t windowMs);
    static bool inTransitionWindow(const TrackEndingInput& input, uint64_t leadMs, bool timelineEnabled);

    AutoTransitionMode m_autoTransitionMode;
    bool m_trackEnding;
    bool m_switchReady;
    bool m_boundaryReached;
    bool m_seekInProgress;

    struct PendingInitialSeek
    {
        InitialSeekRequest request;
        int trackId{-1};
    };
    std::optional<PendingInitialSeek> m_pendingInitialSeek;
};
} // namespace Fooyin
