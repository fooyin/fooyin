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

#include "fycore_export.h"

#include <core/playlist/playlist.h>
#include <core/trackfwd.h>

#include <QObject>

namespace Fooyin {
class FYCORE_EXPORT PlaylistManager : public QObject
{
    Q_OBJECT

public:
    explicit PlaylistManager(QObject* parent = nullptr)
        : QObject{parent} {};

    /** Returns the playlist with the @p id if it exists, otherwise nullptr. */
    [[nodiscard]] virtual Playlist* playlistById(int id) const = 0;
    /** Returns the playlist with the @p index if it exists, otherwise nullptr. */
    [[nodiscard]] virtual Playlist* playlistByIndex(int index) const = 0;
    /** Returns the playlist with the @p name if it exists, otherwise nullptr. */
    [[nodiscard]] virtual Playlist* playlistByName(const QString& name) const = 0;

    [[nodiscard]] virtual PlaylistList playlists() const = 0;

    /** Creates and returns an empty playlist with a default name. */
    virtual Playlist* createEmptyPlaylist() = 0;
    /** Returns the playlist called @p name if it exists, otherwise creates it. */
    virtual Playlist* createPlaylist(const QString& name) = 0;
    /** Returns the playlist called @p name if it exists, otherwise creates it using @p tracks. */
    virtual Playlist* createPlaylist(const QString& name, const TrackList& tracks) = 0;
    /** Adds @p tracks to the end of the playlist with @p id if found. */
    virtual void appendToPlaylist(int id, const TrackList& tracks) = 0;

    virtual void changePlaylistIndex(int id, int index)   = 0;
    virtual void changeActivePlaylist(int id)             = 0;
    virtual void changeActivePlaylist(Playlist* playlist) = 0;

    /** Schedules the playlist with @p id to be played once the current track is finished. */
    virtual void schedulePlaylist(int id) = 0;
    /** Schedules @p playlist to be played once the current track is finished. */
    virtual void schedulePlaylist(Playlist* playlist) = 0;
    /** Clears any scheduled playlist. */
    virtual void clearSchedulePlaylist() = 0;

    virtual void renamePlaylist(int id, const QString& name) = 0;
    virtual void removePlaylist(int id)                      = 0;

    /** Returns the playlist currently being played (nullptr if not playing) */
    [[nodiscard]] virtual Playlist* activePlaylist() const = 0;

    [[nodiscard]] virtual int playlistCount() const = 0;

    /** Changes the active playlist to @ playlistId and starts playback */
    virtual void startPlayback(int playlistId) = 0;

signals:
    void playlistsPopulated();
    void playlistAdded(Playlist* playlist);
    void playlistTracksAdded(Playlist* playlist, const TrackList& tracks, int index);
    void playlistTracksChanged(Playlist* playlist, const std::vector<int>& indexes);
    void playlistRemoved(Playlist* playlist);
    void playlistRenamed(Playlist* playlist);
    void activePlaylistChanged(Playlist* playlist);
};
} // namespace Fooyin
