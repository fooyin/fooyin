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

#include <QObject>

#include <core/playlist/playlist.h>

namespace Fy {

namespace Utils {
class SettingsManager;
}

namespace Core {
class Track;

namespace Library {
class SortingRegistry;
} // namespace Library

namespace Playlist {
class Playlist;
class PlaylistManager;
} // namespace Playlist
} // namespace Core

namespace Gui::Widgets::Playlist {
class PresetRegistry;
struct PlaylistPreset;

class PlaylistController : public QObject
{
    Q_OBJECT

public:
    PlaylistController(Core::Playlist::PlaylistManager* handler, PresetRegistry* presetRegistry,
                       Core::Library::SortingRegistry* sortRegistry, Utils::SettingsManager* settings,
                       QObject* parent = nullptr);
    ~PlaylistController() override;

    Core::Playlist::PlaylistManager* playlistHandler() const;
    PresetRegistry* presetRegistry() const;
    Core::Library::SortingRegistry* sortRegistry() const;

    const Core::Playlist::PlaylistList& playlists() const;

    void startPlayback() const;

    [[nodiscard]] bool currentIsActive() const;
    [[nodiscard]] Core::Playlist::Playlist* currentPlaylist() const;

    void changeCurrentPlaylist(Core::Playlist::Playlist* playlist);
    void changeCurrentPlaylist(int id);
    void changePlaylistIndex(int playlistId, int index);

signals:
    void currentPlaylistChanged(Core::Playlist::Playlist* playlist);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Gui::Widgets::Playlist
} // namespace Fy
