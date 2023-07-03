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

#include "playlist.h"

#include <QObject>

namespace Fy {

namespace Utils {
class SettingsManager;
} // namespace Utils

namespace Core {
namespace DB {
class Database;
} // namespace DB

namespace Player {
class PlayerManager;
}

namespace Library {
class MusicLibrary;
}

namespace Playlist {
class PlaylistHandler : public QObject
{
    Q_OBJECT

public:
    explicit PlaylistHandler(DB::Database* database, Player::PlayerManager* playerManager,
                             Core::Library::MusicLibrary* library, Utils::SettingsManager* settings,
                             QObject* parent = nullptr);
    ~PlaylistHandler() override;

    [[nodiscard]] std::optional<Playlist> playlistById(int id) const;
    [[nodiscard]] std::optional<Playlist> playlistByIndex(int index) const;
    [[nodiscard]] std::optional<Playlist> playlistByName(const QString& name) const;
    [[nodiscard]] PlaylistList playlists() const;

    std::optional<Playlist> createPlaylist(const QString& name, const TrackList& tracks = {}, bool switchTo = false);
    void appendToPlaylist(int id, const TrackList& tracks);
    void createEmptyPlaylist(bool switchTo = false);

    // Replaces tracks and current track index in playlist with those from other
    void exchangePlaylist(Playlist& playlist, const Playlist& other);
    void changeActivePlaylist(int id);

    void renamePlaylist(int id, const QString& name);
    void removePlaylist(int id);

    [[nodiscard]] std::optional<Playlist> activePlaylist() const;
    [[nodiscard]] int playlistCount() const;

    void savePlaylists();

    void startPlayback(int playlistId, const Core::Track& track = {});
    void startPlayback(QString playlistName, const Core::Track& track = {});

signals:
    void playlistsPopulated();
    void playlistAdded(const Core::Playlist::Playlist& playlist, bool switchTo = false);
    void playlistTracksChanged(const Core::Playlist::Playlist& playlist, bool switchTo = false);
    void playlistRemoved(int id);
    void playlistRenamed(const Core::Playlist::Playlist& playlist);
    void activePlaylistChanged(const Core::Playlist::Playlist& playlist = {});

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Playlist
} // namespace Core
} // namespace Fy
