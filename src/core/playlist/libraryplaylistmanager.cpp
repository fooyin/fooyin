/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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
#include "playlisthandler.h"

LibraryPlaylistManager::LibraryPlaylistManager(Playlist::PlaylistHandler* playlistHandler)
    : m_playlistHandler(playlistHandler)
{ }

LibraryPlaylistManager::~LibraryPlaylistManager() = default;

void LibraryPlaylistManager::createPlaylist(const TrackPtrList& tracks, const int id)
{
    const QString name = "Playlist";
    m_playlistHandler->createPlaylist(tracks, name);
    activatePlaylist(m_playlistHandler, id);
}

void LibraryPlaylistManager::append(const TrackPtrList& tracks)
{
    auto* playlist = m_playlistHandler->activePlaylist();
    playlist->appendTracks(tracks);
}

void LibraryPlaylistManager::activatePlaylist(Playlist::PlaylistHandler* playlistHandler, int id)
{
    const auto currentIndex = playlistHandler->currentIndex();
    auto* currentPlaylist = playlistHandler->playlist(currentIndex);

    currentPlaylist->changeTrack(id);
}
