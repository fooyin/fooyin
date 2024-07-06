/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <core/track.h>
#include <utils/id.h>

namespace Fooyin {
enum class PlayState
{
    Playing = 0,
    Paused,
    Stopped,
};

struct PlaylistTrack
{
    Track track;
    UId playlistId;
    int indexInPlaylist{-1};

    [[nodiscard]] bool isValid() const
    {
        return track.isValid();
    }

    [[nodiscard]] bool isInPlaylist() const
    {
        return playlistId.isValid();
    }

    bool operator==(const PlaylistTrack& other) const
    {
        return std::tie(track, playlistId, indexInPlaylist)
            == std::tie(other.track, other.playlistId, other.indexInPlaylist);
    }

    bool operator!=(const PlaylistTrack& other) const
    {
        return !(*this == other);
    }

    bool operator<(const PlaylistTrack& other) const
    {
        return std::tie(track, playlistId, indexInPlaylist)
             < std::tie(other.track, other.playlistId, other.indexInPlaylist);
    }
};
} // namespace Fooyin
