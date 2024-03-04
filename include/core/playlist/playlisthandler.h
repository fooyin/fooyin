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
class SettingsManager;
class Database;
class PlayerController;

class FYCORE_EXPORT PlaylistHandler : public QObject
{
    Q_OBJECT

public:
    explicit PlaylistHandler(Database* database, PlayerController* playerController, SettingsManager* settings,
                             QObject* parent = nullptr);
    ~PlaylistHandler() override;

    /** Returns the playlist with the @p id if it exists, otherwise nullptr. */
    [[nodiscard]] Playlist* playlistById(int id) const;
    /** Returns the playlist with the @p index if it exists, otherwise nullptr. */
    [[nodiscard]] Playlist* playlistByIndex(int index) const;
    /** Returns the playlist with the @p name if it exists, otherwise nullptr. */
    [[nodiscard]] Playlist* playlistByName(const QString& name) const;

    [[nodiscard]] PlaylistList playlists() const;

    /** Creates and returns an empty playlist with a default name. */
    Playlist* createEmptyPlaylist();
    /** Returns the playlist called @p name if it exists, otherwise creates it. */
    Playlist* createPlaylist(const QString& name);
    /** Returns the playlist called @p name if it exists, otherwise creates it using @p tracks. */
    Playlist* createPlaylist(const QString& name, const TrackList& tracks);
    /** Adds @p tracks to the end of the playlist with @p id if found. */
    void appendToPlaylist(int id, const TrackList& tracks);
    /** Replaces the @p tracks of the playlist with @p id if found. */
    void replacePlaylistTracks(int id, const TrackList& tracks);
    /** Removes the tracks at @p indexes of the playlist with @p id if found. */
    void removePlaylistTracks(int id, const std::vector<int>& indexes);
    /** Clears all tracks of the playlist with @p id if found. */
    void clearPlaylistTracks(int id);

    void changePlaylistIndex(int id, int index);
    void changeActivePlaylist(int id);
    void changeActivePlaylist(Playlist* playlist);

    /** Schedules the playlist with @p id to be played once the current track is finished. */
    void schedulePlaylist(int id);
    /** Schedules @p playlist to be played once the current track is finished. */
    void schedulePlaylist(Playlist* playlist);
    /** Clears any scheduled playlist. */
    void clearSchedulePlaylist();

    void renamePlaylist(int id, const QString& name);
    void removePlaylist(int id);

    /** Returns the playlist currently being played (nullptr if not playing) */
    [[nodiscard]] Playlist* activePlaylist() const;

    [[nodiscard]] int playlistCount() const;

    /** Changes the active playlist to @ playlistId and starts playback */
    void startPlayback(int playlistId);

    void savePlaylists();
    void savePlaylist(int id);

signals:
    void playlistsPopulated();
    void playlistAdded(Playlist* playlist);
    void playlistTracksAdded(Playlist* playlist, const TrackList& tracks, int index);
    void playlistTracksChanged(Playlist* playlist, const std::vector<int>& indexes);
    void playlistTracksRemoved(Playlist* playlist, const std::vector<int>& indexes);
    void playlistRemoved(Playlist* playlist);
    void playlistRenamed(Playlist* playlist);
    void activePlaylistChanged(Playlist* playlist);

public slots:
    void populatePlaylists(const TrackList& tracks);
    void tracksUpdated(const TrackList& tracks);
    void tracksRemoved(const TrackList& tracks);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
