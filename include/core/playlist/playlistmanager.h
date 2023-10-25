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

#include "core/track.h"

#include <QObject>

namespace Fy::Core::Playlist {
class Playlist;
using PlaylistList = std::vector<Playlist>;

class FYCORE_EXPORT PlaylistManager : public QObject
{
    Q_OBJECT

public:
    explicit PlaylistManager(QObject* parent = nullptr)
        : QObject{parent} {};

    [[nodiscard]] virtual std::optional<Playlist> playlistById(int id) const                = 0;
    [[nodiscard]] virtual std::optional<Playlist> playlistByIndex(int index) const          = 0;
    [[nodiscard]] virtual std::optional<Playlist> playlistByName(const QString& name) const = 0;
    [[nodiscard]] virtual PlaylistList playlists() const                                    = 0;

    virtual std::optional<Playlist> createPlaylist(const QString& name, const TrackList& tracks = {},
                                                   bool switchTo = false)
        = 0;
    virtual void appendToPlaylist(int id, const TrackList& tracks) = 0;
    virtual void createEmptyPlaylist(bool switchTo = false)        = 0;

    virtual void exchangePlaylist(Playlist& playlist, const Playlist& other)  = 0;
    virtual void replacePlaylistTracks(int id, const Core::TrackList& tracks) = 0;
    virtual void changeActivePlaylist(int id)                                 = 0;

    virtual void renamePlaylist(int id, const QString& name) = 0;
    virtual void removePlaylist(int id)                      = 0;

    [[nodiscard]] virtual std::optional<Playlist> activePlaylist() const = 0;
    [[nodiscard]] virtual int playlistCount() const                      = 0;

    virtual void startPlayback(int playlistId, const Core::Track& track = {})       = 0;
    virtual void startPlayback(QString playlistName, const Core::Track& track = {}) = 0;

signals:
    void playlistsPopulated();
    void playlistAdded(const Core::Playlist::Playlist& playlist, bool switchTo = false);
    void playlistTracksChanged(const Core::Playlist::Playlist& playlist, bool switchTo = false);
    void playlistRemoved(const Core::Playlist::Playlist& playlist);
    void playlistRenamed(const Core::Playlist::Playlist& playlist);
    void activePlaylistChanged(const Core::Playlist::Playlist& playlist);
};
} // namespace Fy::Core::Playlist
