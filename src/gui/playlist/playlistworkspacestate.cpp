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

#include "playlistworkspacestate.h"

#include <core/coresettings.h>
#include <core/playlist/playlisthandler.h>

#include <QDataStream>
#include <QIODevice>
#include <QUndoCommand>
#include <QUndoStack>

constexpr auto LastPlaylistId = "PlaylistWidget/LastPlaylistId";
constexpr auto PlaylistStates = "PlaylistWidget/PlaylistStates";

namespace Fooyin {
PlaylistWorkspaceState::PlaylistWorkspaceState() = default;

PlaylistWorkspaceState::~PlaylistWorkspaceState() = default;

int PlaylistWorkspaceState::lastPlaylistDbId()
{
    const FyStateSettings stateSettings;
    return stateSettings.value(LastPlaylistId, -1).toInt();
}

void PlaylistWorkspaceState::savePersistentState(Playlist* currentPlaylist) const
{
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
    if(currentPlaylist) {
        stateSettings.setValue(LastPlaylistId, currentPlaylist->dbId());
    }
}

void PlaylistWorkspaceState::restorePlaylistStates(PlaylistHandler* handler)
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

void PlaylistWorkspaceState::resetPlaylistViewState(Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    m_states.erase(playlist);
}

void PlaylistWorkspaceState::clearPlaylistHistory(Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    m_histories.erase(playlist);
}

void PlaylistWorkspaceState::resetPlaylistSessionState(Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    clearPlaylistHistory(playlist);
    resetPlaylistViewState(playlist);
}

void PlaylistWorkspaceState::removePlaylist(Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    resetPlaylistSessionState(playlist);
    m_currentSearches.erase(playlist);
}

QString PlaylistWorkspaceState::currentSearch(Playlist* playlist) const
{
    if(!playlist || !m_currentSearches.contains(playlist)) {
        return {};
    }

    return m_currentSearches.at(playlist);
}

void PlaylistWorkspaceState::setCurrentSearch(Playlist* playlist, const QString& search)
{
    if(!playlist) {
        return;
    }

    m_currentSearches[playlist] = search;
}

std::optional<PlaylistViewState> PlaylistWorkspaceState::playlistState(Playlist* playlist) const
{
    if(!playlist || !m_states.contains(playlist)) {
        return {};
    }

    return m_states.at(playlist);
}

void PlaylistWorkspaceState::savePlaylistState(Playlist* playlist, const PlaylistViewState& state)
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

void PlaylistWorkspaceState::addToHistory(Playlist* playlist, QUndoCommand* command)
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

bool PlaylistWorkspaceState::canUndo(Playlist* playlist) const
{
    if(const auto* undoHistory = history(playlist)) {
        return undoHistory->canUndo();
    }

    return false;
}

bool PlaylistWorkspaceState::canRedo(Playlist* playlist) const
{
    if(const auto* undoHistory = history(playlist)) {
        return undoHistory->canRedo();
    }

    return false;
}

void PlaylistWorkspaceState::undo(Playlist* playlist)
{
    if(auto* undoHistory = history(playlist); undoHistory && undoHistory->canUndo()) {
        undoHistory->undo();
    }
}

void PlaylistWorkspaceState::redo(Playlist* playlist)
{
    if(auto* undoHistory = history(playlist); undoHistory && undoHistory->canRedo()) {
        undoHistory->redo();
    }
}

void PlaylistWorkspaceState::clearHistory()
{
    m_histories.clear();
}

bool PlaylistWorkspaceState::clipboardEmpty() const
{
    return m_clipboard.empty();
}

TrackList PlaylistWorkspaceState::clipboard() const
{
    return m_clipboard;
}

void PlaylistWorkspaceState::setClipboard(const TrackList& tracks)
{
    m_clipboard = tracks;
}

QUndoStack* PlaylistWorkspaceState::history(Playlist* playlist)
{
    if(!playlist) {
        return nullptr;
    }

    const auto historyIt = m_histories.find(playlist);
    return historyIt != m_histories.end() ? historyIt->second.get() : nullptr;
}

const QUndoStack* PlaylistWorkspaceState::history(Playlist* playlist) const
{
    if(!playlist) {
        return nullptr;
    }

    const auto historyIt = m_histories.find(playlist);
    return historyIt != m_histories.cend() ? historyIt->second.get() : nullptr;
}
} // namespace Fooyin
