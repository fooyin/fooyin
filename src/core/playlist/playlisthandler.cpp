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

#include "core/coresettings.h"
#include "core/database/database.h"
#include "core/database/playlistdatabase.h"
#include "core/library/musiclibrary.h"
#include "core/player/playermanager.h"
#include "playlist.h"

#include <utils/helpers.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

namespace Fy::Core::Playlist {
PlaylistHandler::PlaylistHandler(DB::Database* database, Player::PlayerManager* playerManager,
                                 Core::Library::MusicLibrary* library, Utils::SettingsManager* settings,
                                 QObject* parent)
    : QObject{parent}
    , m_database{database}
    , m_playerManager{playerManager}
    , m_library{library}
    , m_settings{settings}
    , m_playlistConnector{database->playlistConnector()}
    , m_currentPlaylist{nullptr}
{
    m_playlists.clear();
    m_playlistConnector->getAllPlaylists(m_playlists);

    if(m_playlists.empty()) {
        createPlaylist("Default", {});
    }

    const int lastIndex = m_settings->value<Settings::LastPlaylistIndex>();
    if(lastIndex >= 0 && validIndex(lastIndex)) {
        changeCurrentPlaylist(lastIndex);
    }

    connect(m_library, &Core::Library::MusicLibrary::tracksLoaded, this, &PlaylistHandler::populatePlaylists);
    connect(m_library, &Core::Library::MusicLibrary::libraryRemoved, this, &PlaylistHandler::libraryRemoved);

    connect(m_playerManager, &Player::PlayerManager::nextTrack, this, &PlaylistHandler::next);
    connect(m_playerManager, &Player::PlayerManager::previousTrack, this, &PlaylistHandler::previous);
}

PlaylistHandler::~PlaylistHandler()
{
    m_playlists.clear();
}

Playlist* PlaylistHandler::playlist(int id) const
{
    auto it = std::find_if(m_playlists.cbegin(), m_playlists.cend(), [id](const auto& playlist) {
        return playlist->id() == id;
    });
    if(it != m_playlists.cend()) {
        return it->get();
    }
    return nullptr;
}

Playlist* PlaylistHandler::playlistByIndex(int index) const
{
    auto it = std::find_if(m_playlists.cbegin(), m_playlists.cend(), [index](const auto& playlist) {
        return playlist->index() == index;
    });
    if(it != m_playlists.cend()) {
        return it->get();
    }
    return nullptr;
}

const PlaylistList& PlaylistHandler::playlists() const
{
    return m_playlists;
}

void PlaylistHandler::createPlaylist(const QString& name, const TrackList& tracks, bool switchTo)
{
    Playlist* playlist = addNewPlaylist(name);
    playlist->replaceTracks(tracks);
    if(switchTo) {
        m_currentPlaylist = playlist;
    }
    emit currentPlaylistChanged(m_currentPlaylist);
}

void PlaylistHandler::createEmptyPlaylist()
{
    createPlaylist("Playlist", {});
}

void PlaylistHandler::changeCurrentPlaylist(int index)
{
    auto it = std::find_if(m_playlists.cbegin(), m_playlists.cend(), [index](const auto& playlist) {
        return playlist->index() == index;
    });
    if(it != m_playlists.cend()) {
        m_currentPlaylist = it->get();
        emit currentPlaylistChanged(m_currentPlaylist);
    }
}

void PlaylistHandler::renamePlaylist(int index, const QString& name)
{
    auto it = std::find_if(m_playlists.cbegin(), m_playlists.cend(), [index](const auto& playlist) {
        return playlist->index() == index;
    });
    if(it != m_playlists.cend()) {
        QString newName = name;
        if(exists(newName)) {
            const int count = exists(newName + " (") + 1;
            newName += QString{" (%1)"}.arg(count);
        }
        it->get()->setName(newName);
        emit playlistRenamed(it->get());
    }
}

void PlaylistHandler::removePlaylist(int index)
{
    if(playlistCount() <= 1) {
        return;
    }
    auto it = std::find_if(m_playlists.cbegin(), m_playlists.cend(), [index](const auto& playlist) {
        return playlist->index() == index;
    });
    if(it != m_playlists.cend()) {
        const auto index = static_cast<int>(std::distance(m_playlists.cbegin(), it));
        m_playlists.erase(it);
        if(!m_playlists.empty()) {
            m_currentPlaylist = m_playlists.at(index == 0 ? index : index - 1).get();
            emit playlistRemoved(index);
        }
        else {
            m_currentPlaylist = nullptr;
        }
        emit currentPlaylistChanged(m_currentPlaylist);
    }
}

Playlist* PlaylistHandler::activePlaylist() const
{
    return m_currentPlaylist;
}

int PlaylistHandler::playlistCount() const
{
    return static_cast<int>(m_playlists.size());
}

void PlaylistHandler::savePlaylists()
{
    for(const auto& playlist : m_playlists) {
        if(playlist->wasModified()) {
            m_playlistConnector->insertPlaylistTracks(playlist->id(), playlist->tracks());
        }
    }
    m_settings->set<Settings::LastPlaylistIndex>(m_currentPlaylist->index());
}

void PlaylistHandler::changeCurrentTrack(const Core::Track& track) const
{
    m_currentPlaylist->changeCurrentTrack(track);
    m_playerManager->changeCurrentTrack(m_currentPlaylist->currentTrack());
}

void PlaylistHandler::next()
{
    if(!m_currentPlaylist) {
        return;
    }
    int index = m_currentPlaylist->currentTrackIndex();
    if(index < 0) {
        return m_playerManager->stop();
    }
    const bool isLastTrack      = index == m_currentPlaylist->trackCount() - 1;
    const Player::PlayMode mode = m_playerManager->playMode();
    switch(mode) {
        case(Player::Shuffle):
            // TODO: Implement full shuffle functionality
            index = Utils::randomNumber(0, static_cast<int>(m_currentPlaylist->trackCount() - 1));
            break;
        case(Player::Repeat):
            break;
        case(Player::RepeatAll):
            ++index;
            if(isLastTrack) {
                index = 0;
            }
            break;
        case(Player::Default):
            index = isLastTrack ? -1 : index + 1;
    }
    if(index < 0) {
        return m_playerManager->stop();
    }
    m_currentPlaylist->changeCurrentTrack(index);
    m_playerManager->changeCurrentTrack(m_currentPlaylist->currentTrack());
}

void PlaylistHandler::previous() const
{
    if(!m_currentPlaylist) {
        return;
    }
    if(m_playerManager->currentPosition() > 5000) {
        m_playerManager->changePosition(0);
    }
    int index = m_currentPlaylist->currentTrackIndex();
    if(index < 0) {
        return m_playerManager->stop();
    }
    const bool isFirstTrack     = index == 0;
    const Player::PlayMode mode = m_playerManager->playMode();
    switch(mode) {
        case(Player::Shuffle):
            // TODO: Implement full shuffle functionality
            index = Utils::randomNumber(0, static_cast<int>(m_currentPlaylist->trackCount() - 1));
            break;
        case(Player::Repeat):
            break;
        case(Player::RepeatAll):
            --index;
            if(isFirstTrack) {
                index = m_currentPlaylist->trackCount() - 1;
            }
            break;
        case(Player::Default):
            index = isFirstTrack ? -1 : index - 1;
    }
    if(index < 0) {
        return m_playerManager->stop();
    }
    m_currentPlaylist->changeCurrentTrack(index);
    m_playerManager->changeCurrentTrack(m_currentPlaylist->currentTrack());
}

int PlaylistHandler::exists(const QString& name) const
{
    if(name.isEmpty()) {
        return -1;
    }
    auto it = std::find_if(m_playlists.cbegin(), m_playlists.cend(), [&](const auto& playlist) {
        return (playlist->name().compare(name, Qt::CaseInsensitive) == 0);
    });
    if(it == m_playlists.end()) {
        return -1;
    }
    return static_cast<int>(std::distance(m_playlists.cbegin(), it));
}

bool PlaylistHandler::validIndex(int index) const
{
    return (index >= 0 && index < playlistCount());
}

Playlist* PlaylistHandler::addNewPlaylist(const QString& name)
{
    auto index = exists(name);
    if(index >= 0) {
        return m_playlists.at(index).get();
    }
    index        = playlistCount();
    const int id = m_playlistConnector->insertPlaylist(name, index);
    if(id >= 0) {
        auto* playlist = m_playlists.emplace_back(std::make_unique<Playlist>(name, index, id)).get();
        emit playlistAdded(playlist);
        return playlist;
    }
    return nullptr;
}

void PlaylistHandler::populatePlaylists(const TrackList& tracks)
{
    TrackIdMap idTracks;
    for(const Track& track : tracks) {
        idTracks.emplace(track.id(), track);
    }

    m_database->transaction();

    for(const auto& playlist : m_playlists) {
        std::vector<int> trackIds;
        m_playlistConnector->getPlaylistTracks(playlist->id(), trackIds);
        TrackList playlistTracks;
        for(int id : trackIds) {
            if(idTracks.count(id)) {
                playlistTracks.emplace_back(idTracks.at(id));
            }
        }
        playlist->appendTracks(playlistTracks);
    }

    m_database->commit();
}

void PlaylistHandler::libraryRemoved(int id)
{
    const auto& pLists = playlists();
    for(const auto& playlist : pLists) {
        TrackList tracks = playlist->tracks();
        for(Track& track : tracks) {
            if(track.libraryId() == id) {
                track.setEnabled(false);
            }
        }
        playlist->replaceTracks(tracks);
    }
}
} // namespace Fy::Core::Playlist
