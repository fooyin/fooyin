/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/playlist/playlist.h>
#include <core/track.h>
#include <utils/database/dbmodule.h>

namespace Fooyin {
struct PlaylistInfo
{
    int dbId{-1};
    QString name;
    int index{-1};
    bool isAutoPlaylist{false};
    QString query;
};

class PlaylistDatabase : public DbModule
{
public:
    std::vector<PlaylistInfo> getAllPlaylists();
    TrackList getPlaylistTracks(const Playlist& playlist, const std::unordered_map<int, Track>& tracks);

    int insertPlaylist(const QString& name, int index, bool isAutoPlaylist, const QString& autoQuery);

    bool savePlaylist(Playlist& playlist);
    bool saveModifiedPlaylists(const PlaylistList& playlists);
    bool removePlaylist(int id);
    bool renamePlaylist(int id, const QString& name);

private:
    bool insertPlaylistTrack(int playlistId, const Fooyin::Track& track, int index);
    bool insertPlaylistTracks(int playlistId, const TrackList& tracks);
    TrackList populatePlaylistTracks(const Playlist& playlist, const std::unordered_map<int, Track>& tracks);
};
} // namespace Fooyin
