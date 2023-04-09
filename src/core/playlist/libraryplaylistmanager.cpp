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

#include "libraryplaylistmanager.h"

#include "playlist.h"
#include "playlistmanager.h"

namespace Fy::Core::Playlist {
LibraryPlaylistManager::LibraryPlaylistManager(Library::MusicLibrary* library, PlaylistManager* playlistHandler)
    : m_library{library}
    , m_playlistHandler{playlistHandler}
{ }

void LibraryPlaylistManager::createPlaylist(const TrackList& tracks, int startIndex)
{
    const QString name = "Playlist";
    m_playlistHandler->createPlaylist(tracks, name);
    activatePlaylist(startIndex);
}

void LibraryPlaylistManager::append(const TrackList& tracks)
{
    auto* playlist = m_playlistHandler->activePlaylist();
    playlist->appendTracks(tracks);
}

void LibraryPlaylistManager::activatePlaylist(int startIndex)
{
    const auto currentIndex = m_playlistHandler->currentIndex();
    auto* currentPlaylist   = m_playlistHandler->playlist(currentIndex);

    currentPlaylist->changeTrack(startIndex);
}
} // namespace Fy::Core::Playlist
