/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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
#include <core/track.h>

#include <QObject>

namespace Fy::Core::Playlist {
class FYCORE_EXPORT PlaylistManager : public QObject
{
    Q_OBJECT

public:
    explicit PlaylistManager(QObject* parent = nullptr)
        : QObject{parent} {};

    [[nodiscard]] virtual Playlist* playlistById(int id) const                = 0;
    [[nodiscard]] virtual Playlist* playlistByIndex(int index) const          = 0;
    [[nodiscard]] virtual Playlist* playlistByName(const QString& name) const = 0;
    [[nodiscard]] virtual const PlaylistList& playlists() const               = 0;

    virtual void createEmptyPlaylist()                                                  = 0;
    virtual Playlist* createPlaylist(const QString& name, const TrackList& tracks = {}) = 0;
    virtual void appendToPlaylist(int id, const TrackList& tracks)                      = 0;

    virtual void changePlaylistIndex(int id, int index)                   = 0;
    virtual void changeActivePlaylist(int id)                             = 0;
    virtual void changeActivePlaylist(Core::Playlist::Playlist* playlist) = 0;
    virtual void schedulePlaylist(int id)                                 = 0;
    virtual void schedulePlaylist(Core::Playlist::Playlist* playlist)     = 0;
    virtual void clearSchedulePlaylist()                                  = 0;

    virtual void renamePlaylist(int id, const QString& name) = 0;
    virtual void removePlaylist(int id)                      = 0;

    [[nodiscard]] virtual Playlist* activePlaylist() const = 0;
    [[nodiscard]] virtual int playlistCount() const        = 0;

    virtual void startPlayback(int playlistId) = 0;

signals:
    void playlistsPopulated();
    void playlistAdded(Core::Playlist::Playlist* playlist);
    void playlistTracksChanged(Core::Playlist::Playlist* playlist);
    void playlistRemoved(Core::Playlist::Playlist* playlist);
    void playlistRenamed(Core::Playlist::Playlist* playlist);
    void activePlaylistChanged(Core::Playlist::Playlist* playlist);
    void activeTrackChanged(const Track& track, int index);
};
} // namespace Fy::Core::Playlist
