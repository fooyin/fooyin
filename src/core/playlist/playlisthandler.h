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
class Playlist;
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

    Playlist* playlistById(int id) const;
    Playlist* playlistByIndex(int index) const;
    const PlaylistList& playlists() const;

    void createPlaylist(const QString& name, const TrackList& tracks = {}, bool switchTo = false);
    void createEmptyPlaylist();

    void changeCurrentPlaylist(int id);

    void renamePlaylist(int id, const QString& name);
    void removePlaylist(int id);

    [[nodiscard]] Playlist* activePlaylist() const;
    [[nodiscard]] int playlistCount() const;

    void savePlaylists();

    void changeCurrentTrack(const Core::Track& track) const;

signals:
    void playlistAdded(Core::Playlist::Playlist* playlist);
    void playlistRemoved(int id);
    void playlistRenamed(Core::Playlist::Playlist* playlist);
    void currentPlaylistChanged(Core::Playlist::Playlist* playlist);

private:
    void next();
    void previous() const;

    void updateIndexes();

    int nameCount(const QString& name) const;
    QString findUniqueName(const QString& name) const;
    [[nodiscard]] int exists(const QString& name) const;
    [[nodiscard]] bool validIndex(int index) const;

    Playlist* addNewPlaylist(const QString& name);
    void populatePlaylists(const TrackList& tracks);
    void libraryRemoved(int id);

    DB::Database* m_database;
    Player::PlayerManager* m_playerManager;
    Core::Library::MusicLibrary* m_library;
    Utils::SettingsManager* m_settings;
    DB::Playlist* m_playlistConnector;

    PlaylistList m_playlists;
    Playlist* m_currentPlaylist;
};
} // namespace Playlist
} // namespace Core
} // namespace Fy
