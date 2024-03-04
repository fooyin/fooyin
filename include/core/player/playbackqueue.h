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

#include "fycore_export.h"

#include <core/player/playerdefs.h>
#include <core/track.h>

#include <map>
#include <set>

namespace Fooyin {
using PlaylistIndexes = std::map<int, std::vector<int>>;
using QueueTracks     = std::vector<PlaylistTrack>;

class FYCORE_EXPORT PlaybackQueue
{
public:
    virtual ~PlaybackQueue() = default;

    [[nodiscard]] bool empty() const;

    [[nodiscard]] QueueTracks tracks() const;
    [[nodiscard]] PlaylistTrack track(int index) const;
    [[nodiscard]] int trackCount() const;
    [[nodiscard]] PlaylistIndexes indexesForPlaylist(int id) const;

    PlaylistTrack nextTrack();

    void addTracks(const QueueTracks& tracks);
    QueueTracks removeTracks(const QueueTracks& tracks);
    QueueTracks removePlaylistTracks(int playlistId);

    void clear();

private:
    QueueTracks m_tracks;
};
} // namespace Fooyin
