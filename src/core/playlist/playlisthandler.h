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

#include <core/playlist/playlistmanager.h>
#include <core/track.h>

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

namespace Playlist {
class Playlist;

class FYCORE_EXPORT PlaylistHandler : public PlaylistManager
{
    Q_OBJECT

public:
    explicit PlaylistHandler(DB::Database* database, Player::PlayerManager* playerManager,
                             Utils::SettingsManager* settings, QObject* parent = nullptr);
    ~PlaylistHandler() override;

    [[nodiscard]] std::optional<Playlist> playlistById(int id) const override;
    [[nodiscard]] std::optional<Playlist> playlistByIndex(int index) const override;
    [[nodiscard]] std::optional<Playlist> playlistByName(const QString& name) const override;
    [[nodiscard]] PlaylistList playlists() const override;

    std::optional<Playlist> createPlaylist(const QString& name, const TrackList& tracks = {},
                                           bool switchTo = false) override;
    void appendToPlaylist(int id, const TrackList& tracks) override;
    void createEmptyPlaylist(bool switchTo = false) override;

    // Replaces tracks and current track index in playlist with those from other
    void exchangePlaylist(Playlist& playlist, const Playlist& other) override;
    void replacePlaylistTracks(int id, const Core::TrackList& tracks) override;
    void changeActivePlaylist(int id) override;

    void renamePlaylist(int id, const QString& name) override;
    void removePlaylist(int id) override;

    [[nodiscard]] std::optional<Playlist> activePlaylist() const override;
    [[nodiscard]] int playlistCount() const override;

    void savePlaylists();

    void startPlayback(int playlistId, const Core::Track& track = {}) override;
    void startPlayback(QString playlistName, const Core::Track& track = {}) override;

    void populatePlaylists(const TrackList& tracks);
    void libraryRemoved(int id);
    void tracksUpdated(const Core::TrackList& tracks);
    void tracksRemoved(const Core::TrackList& tracks);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Playlist
} // namespace Core
} // namespace Fy
