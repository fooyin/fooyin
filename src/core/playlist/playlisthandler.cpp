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

#include "playlisthandler.h"

#include "core/player/playermanager.h"
#include "playlist.h"

#include <utils/helpers.h>

namespace Fy::Core::Playlist {
PlaylistHandler::PlaylistHandler(Player::PlayerManager* playerManager, QObject* parent)
    : QObject{parent}
    , m_playerManager{playerManager}
    , m_currentPlaylistIndex{-1}
{
    connect(m_playerManager, &Player::PlayerManager::nextTrack, this, &PlaylistHandler::next);
    connect(m_playerManager, &Player::PlayerManager::previousTrack, this, &PlaylistHandler::previous);
}

PlaylistHandler::~PlaylistHandler()
{
    m_playlists.clear();
}

Playlist* PlaylistHandler::playlist(int id)
{
    if(m_playlists.count(id)) {
        return m_playlists.at(id);
    }
    return {};
}

int PlaylistHandler::createPlaylist(const TrackPtrList& tracks, const QString& name)
{
    const auto index = addNewPlaylist(name);

    auto& playlist = m_playlists[index];
    playlist->createPlaylist(tracks);

    setCurrentIndex(index);
    return m_currentPlaylistIndex;
}

int PlaylistHandler::createEmptyPlaylist()
{
    const QString name = "Playlist";

    return createPlaylist({}, name);
}

int PlaylistHandler::activeIndex() const
{
    return currentIndex();
}

Playlist* PlaylistHandler::activePlaylist() const
{
    return m_playlists.at(activeIndex());
}

int PlaylistHandler::currentIndex() const
{
    return m_currentPlaylistIndex;
}

void PlaylistHandler::setCurrentIndex(int playlistIndex)
{
    if(m_currentPlaylistIndex != playlistIndex) {
        m_currentPlaylistIndex = playlistIndex;
    }
}

int PlaylistHandler::count() const
{
    return static_cast<int>(m_playlists.size());
}

void PlaylistHandler::next()
{
    auto* playlist = activePlaylist();
    if(playlist) {
        const int index = playlist->next();
        if(index < 0) {
            m_playerManager->stop();
        }
    }
}

void PlaylistHandler::previous() const
{
    activePlaylist()->previous();
}

int PlaylistHandler::exists(const QString& name) const
{
    if(name.isEmpty() && validIndex(m_currentPlaylistIndex)) {
        return m_currentPlaylistIndex;
    }

    auto it = std::find_if(m_playlists.cbegin(), m_playlists.cend(), [&](const auto& playlist) {
        return (playlist.second->name().compare(name, Qt::CaseInsensitive) == 0);
    });
    if(it == m_playlists.end()) {
        return -1;
    }
    return static_cast<int>(std::distance(m_playlists.cbegin(), it));
}

bool PlaylistHandler::validIndex(int index) const
{
    return (index >= 0 && index < count());
}

int PlaylistHandler::addNewPlaylist(const QString& name)
{
    const auto index = exists(name);
    if(index >= 0) {
        m_playlists.at(index)->clear();
        return index;
    }

    const auto count = static_cast<int>(m_playlists.size());
    auto* playlist   = new Playlist(m_playerManager, count, name, this);
    m_playlists.emplace(playlist->index(), playlist);

    return playlist->index();
}
} // namespace Fy::Core::Playlist
