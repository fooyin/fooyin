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

#include "libraryplaylistinterface.h"

namespace Fy::Core {

namespace Library {
class MusicLibrary;
}

namespace Playlist {
class PlaylistManager;

class LibraryPlaylistManager : public LibraryPlaylistInterface
{
public:
    explicit LibraryPlaylistManager(Library::MusicLibrary* library, PlaylistManager* playlistHandler);

    void createPlaylist(const TrackList& tracks, int startIndex) override;
    void append(const TrackList& tracks) override;

private:
    void activatePlaylist(int startIndex);

    Library::MusicLibrary* m_library;
    Playlist::PlaylistManager* m_playlistHandler;
};
} // namespace Playlist
} // namespace Fy::Core
