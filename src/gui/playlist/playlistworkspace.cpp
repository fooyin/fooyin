/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
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

#include "playlistworkspace.h"

#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>

#include <utility>

namespace Fooyin {
PlaylistWorkspace::PlaylistWorkspace(SettingsManager* settings)
    : m_loaded{false}
    , m_currentPlaylist{nullptr}
    , m_state{settings}
{ }

PlaylistWorkspace::~PlaylistWorkspace() = default;

void PlaylistWorkspace::restorePlaylistStates(PlaylistHandler* handler)
{
    m_state.restorePlaylistStates(handler);
}

void PlaylistWorkspace::restoreLastPlaylist(PlaylistHandler* handler, PlayerController* playerController)
{
    if(!handler || !playerController) {
        return;
    }

    const int lastId = m_state.lastPlaylistDbId();

    if(lastId >= 0) {
        m_currentPlaylist = handler->playlistByDbId(lastId);
        if(!m_currentPlaylist) {
            m_currentPlaylist = handler->playlistByIndex(0);
        }
    }

    if(m_currentPlaylist && !handler->activePlaylist() && playerController->playState() == Player::PlayState::Stopped) {
        handler->changeActivePlaylist(m_currentPlaylist);
    }

    m_loaded = true;
}

void PlaylistWorkspace::savePersistentState() const
{
    if(m_currentPlaylist) {
        m_state.savePersistentState(m_currentPlaylist);
    }
}

bool PlaylistWorkspace::playlistsHaveLoaded() const
{
    return m_loaded;
}

Playlist* PlaylistWorkspace::currentPlaylist() const
{
    return m_currentPlaylist;
}

UId PlaylistWorkspace::currentPlaylistId() const
{
    return m_currentPlaylist ? m_currentPlaylist->id() : UId{};
}

bool PlaylistWorkspace::currentIsActive(const PlaylistHandler* handler) const
{
    return handler && m_currentPlaylist == handler->activePlaylist();
}

bool PlaylistWorkspace::currentIsAuto() const
{
    return m_currentPlaylist && m_currentPlaylist->isAutoPlaylist();
}

bool PlaylistWorkspace::isCurrentPlaylist(const Playlist* playlist) const
{
    return playlist == m_currentPlaylist;
}

Playlist* PlaylistWorkspace::changeCurrentPlaylist(PlaylistHandler* handler, PlayerController* playerController,
                                                   Playlist* playlist)
{
    if(!handler || !playerController || !playlist) {
        return nullptr;
    }

    if(!handler->activePlaylist() && playerController->playState() == Player::PlayState::Stopped) {
        handler->changeActivePlaylist(playlist);
    }

    return std::exchange(m_currentPlaylist, playlist);
}

void PlaylistWorkspace::resetPlaylistSessionState(Playlist* playlist)
{
    m_state.resetPlaylistSessionState(playlist);
}

void PlaylistWorkspace::resetCurrentPlaylistSessionState()
{
    m_state.resetPlaylistSessionState(m_currentPlaylist);
}

void PlaylistWorkspace::removePlaylist(Playlist* playlist)
{
    m_state.removePlaylist(playlist);
}

QString PlaylistWorkspace::currentSearch(Playlist* playlist) const
{
    return m_state.currentSearch(playlist);
}

void PlaylistWorkspace::setCurrentSearch(Playlist* playlist, const QString& search)
{
    m_state.setCurrentSearch(playlist, search);
}

std::optional<PlaylistViewState> PlaylistWorkspace::playlistState(Playlist* playlist) const
{
    return m_state.playlistState(playlist);
}

void PlaylistWorkspace::savePlaylistState(Playlist* playlist, const PlaylistViewState& state)
{
    m_state.savePlaylistState(playlist, state);
}

void PlaylistWorkspace::addToHistory(QUndoCommand* command)
{
    m_state.addToHistory(m_currentPlaylist, command);
}

bool PlaylistWorkspace::canUndo() const
{
    return m_state.canUndo(m_currentPlaylist);
}

bool PlaylistWorkspace::canRedo() const
{
    return m_state.canRedo(m_currentPlaylist);
}

void PlaylistWorkspace::undo()
{
    m_state.undo(m_currentPlaylist);
}

void PlaylistWorkspace::redo()
{
    m_state.redo(m_currentPlaylist);
}

void PlaylistWorkspace::clearHistory()
{
    m_state.clearHistory();
}

bool PlaylistWorkspace::clipboardEmpty() const
{
    return m_state.clipboardEmpty();
}

TrackList PlaylistWorkspace::clipboard() const
{
    return m_state.clipboard();
}

void PlaylistWorkspace::setClipboard(const TrackList& tracks)
{
    m_state.setClipboard(tracks);
}
} // namespace Fooyin
