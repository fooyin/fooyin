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

#include <cstdint>
#include <optional>

namespace Fooyin {
/*!
 * Central coordinator for end-of-track, seek, and overlap-transition state.
 */
class FYCORE_EXPORT PlaybackTransitionCoordinator
{
public:
    PlaybackTransitionCoordinator();

    enum class AutoTransitionMode : uint8_t
    {
        None = 0,
        Gapless,
        Crossfade
    };

    struct CrossfadeParams
    {
        uint64_t seekPositionMs{0};
        int fadeOutDurationMs{0};
        int fadeInDurationMs{0};
        uint64_t transitionId{0};
    };

    struct TrackEndingResult
    {
        bool aboutToFinish{false};
        bool readyToSwitch{false};
        bool endReached{false};
    };

    struct TrackEndingInput
    {
        uint64_t positionMs{0};
        uint64_t durationMs{0};
        uint64_t remainingOutputMs{0};
        bool endOfInput{false};
        bool bufferEmpty{false};
        bool autoCrossfadeEnabled{false};
        bool gaplessEnabled{false};
        int autoFadeOutMs{0};
        int autoFadeInMs{0};
        uint64_t gaplessPrepareWindowMs{0};
    };

    [[nodiscard]] TrackEndingResult evaluateTrackEnding(const TrackEndingInput& input);

    //! Queue a seek transition that waits until buffered audio is sufficient.
    void beginPendingSeek(uint64_t positionMs, int fadeOutDurationMs, int fadeInDurationMs, uint64_t transitionId = 0);
    void cancelPendingSeek();
    [[nodiscard]] bool hasPendingSeek() const;
    //! Pending seek params when buffered duration is enough to start fade-out.
    [[nodiscard]] std::optional<CrossfadeParams> pendingSeekIfBuffered(uint64_t bufferedMs) const;

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
    void queueInitialSeek(uint64_t positionMs, int trackId);
    [[nodiscard]] bool hasPendingInitialSeekForTrack(int trackId) const;
    [[nodiscard]] std::optional<uint64_t> takePendingInitialSeekForTrack(int trackId);

private:
    bool shouldSignalTrackEnding(const TrackEndingInput& input);
    [[nodiscard]] bool shouldSignalReadyToSwitch(const TrackEndingInput& input) const;
    [[nodiscard]] static bool inTimelineWindow(const TrackEndingInput& input, uint64_t windowMs);

    AutoTransitionMode m_autoTransitionMode;
    CrossfadeParams m_crossfadeParams;
    bool m_trackEnding;
    bool m_switchReady;
    bool m_seekInProgress;
    bool m_pendingSeekActive;

    bool m_pendingInitialSeekActive;
    uint64_t m_pendingInitialSeekPositionMs;
    int m_pendingInitialSeekTrackId;
};
} // namespace Fooyin
