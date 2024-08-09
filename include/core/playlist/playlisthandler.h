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

#include <core/engine/audioloader.h>
#include <core/playlist/playlist.h>
#include <core/track.h>
#include <utils/database/dbconnectionpool.h>

#include <QObject>

namespace Fooyin {
class PlayerController;
class PlaylistHandlerPrivate;
class SettingsManager;

class FYCORE_EXPORT PlaylistHandler : public QObject
{
    Q_OBJECT

public:
    explicit PlaylistHandler(DbConnectionPoolPtr dbPool, std::shared_ptr<AudioLoader> audioLoader,
                             PlayerController* playerController, SettingsManager* settings, QObject* parent = nullptr);
    ~PlaylistHandler() override;

    /** Returns the playlist with the @p id if it exists, otherwise nullptr. */
    [[nodiscard]] Playlist* playlistById(const UId& id) const;
    /** Returns the playlist with the db @p id if it exists, otherwise nullptr. */
    [[nodiscard]] Playlist* playlistByDbId(int id) const;
    /** Returns the playlist with the @p index if it exists, otherwise nullptr. */
    [[nodiscard]] Playlist* playlistByIndex(int index) const;
    /** Returns the playlist with the @p name if it exists, otherwise nullptr. */
    [[nodiscard]] Playlist* playlistByName(const QString& name) const;

    [[nodiscard]] PlaylistList playlists() const;
    [[nodiscard]] PlaylistList removedPlaylists() const;

    /** Creates and returns an empty playlist with a default name. */
    Playlist* createEmptyPlaylist();
    /** Creates and returns an empty hidden playlist with a default name. */
    Playlist* createTempEmptyPlaylist();
    /** Returns the playlist called @p name if it exists, otherwise creates it. */
    Playlist* createPlaylist(const QString& name);
    /** Returns the temporary playlist called @p name if it exists, otherwise creates it. */
    Playlist* createTempPlaylist(const QString& name);
    /** Returns the playlist called @p name and replaces it's tracks if it exists, otherwise creates it. */
    Playlist* createPlaylist(const QString& name, const TrackList& tracks);
    /** Returns the temporary playlist called @p name and replaces it's tracks if it exists, otherwise creates it. */
    Playlist* createTempPlaylist(const QString& name, const TrackList& tracks);
    /** Creates and returns a playlist called @p name. If it already exists, the name will be changed. */
    Playlist* createNewPlaylist(const QString& name);
    /** Creates and returns a temporary playlist called @p name. If it already exists, the name will be changed. */
    Playlist* createNewTempPlaylist(const QString& name);
    /** Creates and returns a playlist called @p name with the provided tracks. If it already exists, the name will be
     * changed. */
    Playlist* createNewPlaylist(const QString& name, const TrackList& tracks);
    /** Creates and returns a temporary playlist called @p name with the provided tracks. If it already exists, the name
     * will be changed. */
    Playlist* createNewTempPlaylist(const QString& name, const TrackList& tracks);

    /** Adds @p tracks to the end of the playlist with @p id if found. */
    void appendToPlaylist(const UId& id, const TrackList& tracks);
    /** Replaces the @p tracks of the playlist with @p id if found. */
    void replacePlaylistTracks(const UId& id, const TrackList& tracks);
    /** Moves the tracks of the playlist with @p id to the playlist with @p replaceId. */
    void movePlaylistTracks(const UId& id, const UId& replaceId);
    /** Removes the tracks at @p indexes of the playlist with @p id if found. */
    void removePlaylistTracks(const UId& id, const std::vector<int>& indexes);
    /** Clears all tracks of the playlist with @p id if found. */
    void clearPlaylistTracks(const UId& id);

    void changePlaylistIndex(const UId& id, int index);
    void changeActivePlaylist(const UId& id);
    void changeActivePlaylist(Playlist* playlist);

    /** Schedules the playlist with @p id to be played once the current track is finished. */
    void schedulePlaylist(const UId& id);
    /** Schedules @p playlist to be played once the current track is finished. */
    void schedulePlaylist(Playlist* playlist);
    /** Clears any scheduled playlist. */
    void clearSchedulePlaylist();
    /** Returns the next track to be played, or an invalid track if the playlist will end. */
    Track nextTrack();
    /** Returns the previous track to be played, or an invalid track if the playlist will end. */
    Track previousTrack();

    void renamePlaylist(const UId& id, const QString& name);
    void removePlaylist(const UId& id);

    /** Returns the playlist currently being played (nullptr if not playing) */
    [[nodiscard]] Playlist* activePlaylist() const;

    [[nodiscard]] int playlistCount() const;

    /** Changes the active playlist to the playlist with @p playlistId and starts playback. */
    void startPlayback(const UId& id);
    /** Changes the active playlist to @p playlist and starts playback. */
    void startPlayback(Playlist* playlist);

    void savePlaylists();
    void savePlaylist(const UId& id);

signals:
    void playlistsPopulated();
    void playlistAdded(Fooyin::Playlist* playlist);
    void playlistRemoved(Fooyin::Playlist* playlist);
    void playlistRenamed(Fooyin::Playlist* playlist);
    void activePlaylistChanged(Fooyin::Playlist* playlist);

    void tracksAdded(Fooyin::Playlist* playlist, const Fooyin::TrackList& tracks, int index);
    void tracksChanged(Fooyin::Playlist* playlist, const std::vector<int>& indexes);
    void tracksUpdated(Fooyin::Playlist* playlist, const std::vector<int>& indexes);
    void tracksRemoved(Fooyin::Playlist* playlist, const std::vector<int>& indexes);

public slots:
    void populatePlaylists(const Fooyin::TrackList& tracks);
    void handleTracksChanged(const Fooyin::TrackList& tracks);
    void handleTracksUpdated(const Fooyin::TrackList& tracks);
    void handleTracksRemoved(const Fooyin::TrackList& tracks);
    void trackAboutToFinish();

private:
    std::unique_ptr<PlaylistHandlerPrivate> p;
};
} // namespace Fooyin
