/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <core/playlist/playlist.h>

#include <map>
#include <optional>

namespace Fooyin {
// Queue positions for each playlist
using PlaylistIndexes = std::map<UId, std::vector<int>>;
// Queue positions of a playlist track index
using PlaylistTrackIndexes = std::map<int, std::vector<int>>;
using QueueTracks          = std::vector<PlaylistTrack>;

class FYCORE_EXPORT PlaybackQueue
{
public:
    virtual ~PlaybackQueue() = default;

    [[nodiscard]] bool empty() const;

    [[nodiscard]] const QueueTracks& tracks() const;
    [[nodiscard]] PlaylistTrack track(int index) const;
    [[nodiscard]] int trackCount() const;

    [[nodiscard]] PlaylistIndexes playlistIndexes() const;
    [[nodiscard]] PlaylistTrackIndexes indexesForPlaylist(const UId& id) const;
    [[nodiscard]] std::vector<int> indexesForTrack(const UId& playlistId, int playlistTrackIndex) const;

    [[nodiscard]] PlaylistTrack nextTrack() const;
    PlaylistTrack nextTrackChange();

    [[nodiscard]] int getTrackIndex(const PlaylistTrack& track) const;
    [[nodiscard]] bool containsTrack(const PlaylistTrack& track) const;
    void addTracks(const QueueTracks& tracks, int index = -1);
    void replaceTracks(const QueueTracks& tracks);
    std::optional<PlaylistTrack> removeFirstMatchingTrack(const PlaylistTrack& track);
    QueueTracks removeTracks(const QueueTracks& tracks);
    QueueTracks removePlaylistTracks(const UId& playlistId);

    void clear();

private:
    QueueTracks m_tracks;
};
} // namespace Fooyin
