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
#include <core/trackfwd.h>

#include <QObject>

namespace Fooyin {
class SettingsManager;
class Database;
class PlayerManager;
class Playlist;

class FYCORE_EXPORT PlaylistHandler : public PlaylistManager
{
    Q_OBJECT

public:
    explicit PlaylistHandler(Database* database, PlayerManager* playerManager, SettingsManager* settings,
                             QObject* parent = nullptr);
    ~PlaylistHandler() override;

    [[nodiscard]] Playlist* playlistById(int id) const override;
    [[nodiscard]] Playlist* playlistByIndex(int index) const override;
    [[nodiscard]] Playlist* playlistByName(const QString& name) const override;
    [[nodiscard]] PlaylistList playlists() const override;

    void createEmptyPlaylist() override;
    Playlist* createPlaylist(const QString& name) override;
    Playlist* createPlaylist(const QString& name, const TrackList& tracks) override;
    void appendToPlaylist(int id, const TrackList& tracks) override;

    void changePlaylistIndex(int id, int index) override;
    void changeActivePlaylist(int id) override;
    void changeActivePlaylist(Playlist* playlist) override;
    void schedulePlaylist(int id) override;
    void schedulePlaylist(Playlist* playlist) override;
    void clearSchedulePlaylist() override;

    void renamePlaylist(int id, const QString& name) override;
    void removePlaylist(int id) override;

    [[nodiscard]] Playlist* activePlaylist() const override;
    [[nodiscard]] int playlistCount() const override;

    void savePlaylists();

    void startPlayback(int playlistId) override;

public slots:
    void populatePlaylists(const TrackList& tracks);
    void libraryRemoved(int id);
    void tracksUpdated(const TrackList& tracks);
    void tracksRemoved(const TrackList& tracks);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
