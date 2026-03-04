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

#include <QObject>

#include <cstdint>

namespace Fooyin::Playback {
Q_NAMESPACE

enum class Phase : uint8_t
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
Q_ENUM_NS(Phase)

[[nodiscard]] constexpr bool isPlaybackActive(Phase phase)
{
    switch(phase) {
        case Phase::Playing:
        case Phase::Seeking:
        case Phase::SeekCrossfading:
        case Phase::TrackCrossfading:
        case Phase::FadingToPause:
        case Phase::FadingToStop:
        case Phase::FadingToManualChange:
            return true;
        default:
            return false;
    }
}

[[nodiscard]] constexpr bool isCrossfading(Phase phase)
{
    return phase == Phase::SeekCrossfading || phase == Phase::TrackCrossfading;
}

[[nodiscard]] constexpr bool isFadingTransport(Phase phase)
{
    return phase == Phase::FadingToPause || phase == Phase::FadingToStop || phase == Phase::FadingToManualChange;
}
} // namespace Fooyin::Playback
