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

#include <cstdint>

namespace Fooyin {
enum class PlaybackPhase : uint8_t
{
    Stopped = 0,
    Loading,
    Playing,
    Paused,
    Seeking,
    SeekCrossfading,
    TrackCrossfading,
    FadingToPause,
    FadingToStop,
    FadingToManualChange,
    Error
};

[[nodiscard]] constexpr bool isPlaybackActive(PlaybackPhase phase)
{
    switch(phase) {
        case PlaybackPhase::Playing:
        case PlaybackPhase::Seeking:
        case PlaybackPhase::SeekCrossfading:
        case PlaybackPhase::TrackCrossfading:
        case PlaybackPhase::FadingToPause:
        case PlaybackPhase::FadingToStop:
        case PlaybackPhase::FadingToManualChange:
            return true;
        default:
            return false;
    }
}

[[nodiscard]] constexpr bool isCrossfading(PlaybackPhase phase)
{
    return phase == PlaybackPhase::SeekCrossfading || phase == PlaybackPhase::TrackCrossfading;
}

[[nodiscard]] constexpr bool isFadingTransport(PlaybackPhase phase)
{
    return phase == PlaybackPhase::FadingToPause || phase == PlaybackPhase::FadingToStop
        || phase == PlaybackPhase::FadingToManualChange;
}
} // namespace Fooyin
