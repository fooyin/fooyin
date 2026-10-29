/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include <core/coresettings.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>

#include <QDataStream>
#include <QIODevice>
#include <QUndoCommand>
#include <QUndoStack>

constexpr auto LastPlaylistId = "PlaylistWidget/LastPlaylistId";
constexpr auto PlaylistStates = "PlaylistWidget/PlaylistStates";

namespace Fooyin {
PlaylistWorkspace::PlaylistWorkspace()
    : m_loaded{false}
    , m_currentPlaylist{nullptr}
{ }

PlaylistWorkspace::~PlaylistWorkspace() = default;

void PlaylistWorkspace::restorePlaylistStates(PlaylistHandler* handler)
{
    if(!handler) {
        return;
    }

    const FyStateSettings stateSettings;
    QByteArray in = stateSettings.value(PlaylistStates).toByteArray();

    if(in.isEmpty()) {
        return;
    }

    in = qUncompress(in);

    QDataStream stream{&in, QIODevice::ReadOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    qint32 size;
    stream >> size;
    m_states.clear();

    for(qint32 i{0}; i < size; ++i) {
        int dbId;
        stream >> dbId;

        PlaylistViewState value;
        stream >> value.topIndex;
        stream >> value.scrollPos;

        if(auto* playlist = handler->playlistByDbId(dbId)) {
            m_states[playlist] = value;
        }
    }
}

void PlaylistWorkspace::restoreLastPlaylist(PlaylistHandler* handler, PlayerController* playerController)
{
    if(!handler || !playerController) {
        return;
    }

    const FyStateSettings stateSettings;
    const int lastId = stateSettings.value(LastPlaylistId, -1).toInt();

    if(lastId >= 0) {
        m_currentPlaylist = handler->playlistByDbId(lastId);
    }

    if(!m_currentPlaylist) {
        m_currentPlaylist = handler->playlistByIndex(0);
    }

    if(m_currentPlaylist && !handler->activePlaylist() && playerController->playState() == Player::PlayState::Stopped) {
        handler->changeActivePlaylist(m_currentPlaylist);
    }

    m_loaded = true;
}

void PlaylistWorkspace::savePersistentState() const
{
    if(!m_currentPlaylist) {
        return;
    }

    QByteArray out;
    QDataStream stream{&out, QIODevice::WriteOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    stream << static_cast<qint32>(m_states.size());
    for(const auto& [playlist, state] : m_states) {
        if(playlist) {
            stream << playlist->dbId();
            stream << state.topIndex;
            stream << state.scrollPos;
        }
    }

    out = qCompress(out, 9);

    FyStateSettings stateSettings;
    stateSettings.setValue(PlaylistStates, out);
    stateSettings.setValue(LastPlaylistId, m_currentPlaylist->dbId());
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

void PlaylistWorkspace::resetPlaylistViewState(Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    m_states.erase(playlist);
}

void PlaylistWorkspace::clearPlaylistHistory(Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    m_histories.erase(playlist);
}

void PlaylistWorkspace::resetPlaylistSessionState(Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    clearPlaylistHistory(playlist);
    resetPlaylistViewState(playlist);
}

void PlaylistWorkspace::resetCurrentPlaylistSessionState()
{
    resetPlaylistSessionState(m_currentPlaylist);
}

void PlaylistWorkspace::removePlaylist(Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    resetPlaylistSessionState(playlist);
    m_currentSearches.erase(playlist);
}

QString PlaylistWorkspace::currentSearch(Playlist* playlist) const
{
    if(!playlist || !m_currentSearches.contains(playlist)) {
        return {};
    }

    return m_currentSearches.at(playlist);
}

void PlaylistWorkspace::setCurrentSearch(Playlist* playlist, const QString& search)
{
    if(!playlist) {
        return;
    }

    m_currentSearches[playlist] = search;
}

std::optional<PlaylistViewState> PlaylistWorkspace::playlistState(Playlist* playlist) const
{
    if(!playlist || !m_states.contains(playlist)) {
        return {};
    }

    return m_states.at(playlist);
}

void PlaylistWorkspace::savePlaylistState(Playlist* playlist, const PlaylistViewState& state)
{
    if(!playlist) {
        return;
    }

    if(state.scrollPos == 0 || state.topIndex < 0) {
        m_states.erase(playlist);
    }
    else {
        m_states[playlist] = state;
    }
}

void PlaylistWorkspace::addToHistory(QUndoCommand* command)
{
    addToHistory(m_currentPlaylist, command);
}

void PlaylistWorkspace::addToHistory(Playlist* playlist, QUndoCommand* command)
{
    if(!playlist || !command) {
        return;
    }

    auto& history = m_histories[playlist];
    if(!history) {
        history = std::make_unique<QUndoStack>();
    }
    history->push(command);
}

bool PlaylistWorkspace::canUndo() const
{
    return canUndo(m_currentPlaylist);
}

bool PlaylistWorkspace::canUndo(Playlist* playlist) const
{
    if(const auto* undoHistory = history(playlist)) {
        return undoHistory->canUndo();
    }

    return false;
}

bool PlaylistWorkspace::canRedo() const
{
    return canRedo(m_currentPlaylist);
}

bool PlaylistWorkspace::canRedo(Playlist* playlist) const
{
    if(const auto* undoHistory = history(playlist)) {
        return undoHistory->canRedo();
    }

    return false;
}

void PlaylistWorkspace::undo()
{
    undo(m_currentPlaylist);
}

void PlaylistWorkspace::undo(Playlist* playlist)
{
    if(auto* undoHistory = history(playlist); undoHistory && undoHistory->canUndo()) {
        undoHistory->undo();
    }
}

void PlaylistWorkspace::redo()
{
    redo(m_currentPlaylist);
}

void PlaylistWorkspace::redo(Playlist* playlist)
{
    if(auto* undoHistory = history(playlist); undoHistory && undoHistory->canRedo()) {
        undoHistory->redo();
    }
}

void PlaylistWorkspace::clearHistory()
{
    m_histories.clear();
}

bool PlaylistWorkspace::clipboardEmpty() const
{
    return m_clipboard.empty();
}

TrackList PlaylistWorkspace::clipboard() const
{
    return m_clipboard;
}

void PlaylistWorkspace::setClipboard(const TrackList& tracks)
{
    m_clipboard = tracks;
}

QUndoStack* PlaylistWorkspace::history(Playlist* playlist)
{
    if(!playlist) {
        return nullptr;
    }

    const auto historyIt = m_histories.find(playlist);
    return historyIt != m_histories.end() ? historyIt->second.get() : nullptr;
}

const QUndoStack* PlaylistWorkspace::history(Playlist* playlist) const
{
    if(!playlist) {
        return nullptr;
    }

    const auto historyIt = m_histories.find(playlist);
    return historyIt != m_histories.cend() ? historyIt->second.get() : nullptr;
}
} // namespace Fooyin
