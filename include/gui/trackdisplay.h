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

#include <core/player/playerdefs.h>

#include <cstdint>

namespace Fooyin {
enum class TrackDisplayPreference : uint8_t
{
    PlayingTrack = 0,
    SelectedTrack,
    PlayingTrackSelectedWhenStopped,
    PlayingTrackBlankAtStartup,
    PlayingTrackBlankWhenStopped,
};

enum class PreferredTrackSource : uint8_t
{
    Playing = 0,
    Selected,
    None,
};

constexpr PreferredTrackSource preferredTrackSource(TrackDisplayPreference preference, Player::PlayState playState,
                                                    bool playbackStarted)
{
    switch(preference) {
        case TrackDisplayPreference::SelectedTrack:
            return PreferredTrackSource::Selected;
        case TrackDisplayPreference::PlayingTrackSelectedWhenStopped:
            return playState == Player::PlayState::Stopped ? PreferredTrackSource::Selected
                                                           : PreferredTrackSource::Playing;
        case TrackDisplayPreference::PlayingTrackBlankAtStartup:
            return playbackStarted ? PreferredTrackSource::Playing : PreferredTrackSource::None;
        case TrackDisplayPreference::PlayingTrackBlankWhenStopped:
            return playState == Player::PlayState::Stopped ? PreferredTrackSource::None : PreferredTrackSource::Playing;
        case TrackDisplayPreference::PlayingTrack:
        default:
            return PreferredTrackSource::Playing;
    }
}
} // namespace Fooyin
