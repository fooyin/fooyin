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

#pragma once

#include "playlistcontroller.h"

#include <memory>
#include <optional>
#include <unordered_map>

class QUndoCommand;
class QUndoStack;

namespace Fooyin {
class Playlist;
class PlaylistHandler;
class SettingsManager;

class PlaylistWorkspaceState
{
public:
    PlaylistWorkspaceState();
    ~PlaylistWorkspaceState();

    static int lastPlaylistDbId();
    void savePersistentState(Playlist* currentPlaylist) const;
    void restorePlaylistStates(PlaylistHandler* handler);

    void resetPlaylistViewState(Playlist* playlist);
    void clearPlaylistHistory(Playlist* playlist);
    void resetPlaylistSessionState(Playlist* playlist);
    void removePlaylist(Playlist* playlist);

    [[nodiscard]] QString currentSearch(Playlist* playlist) const;
    void setCurrentSearch(Playlist* playlist, const QString& search);

    [[nodiscard]] std::optional<PlaylistViewState> playlistState(Playlist* playlist) const;
    void savePlaylistState(Playlist* playlist, const PlaylistViewState& state);

    void addToHistory(Playlist* playlist, QUndoCommand* command);
    [[nodiscard]] bool canUndo(Playlist* playlist) const;
    [[nodiscard]] bool canRedo(Playlist* playlist) const;
    void undo(Playlist* playlist);
    void redo(Playlist* playlist);
    void clearHistory();

    [[nodiscard]] bool clipboardEmpty() const;
    [[nodiscard]] TrackList clipboard() const;
    void setClipboard(const TrackList& tracks);

private:
    [[nodiscard]] QUndoStack* history(Playlist* playlist);
    [[nodiscard]] const QUndoStack* history(Playlist* playlist) const;

    std::unordered_map<Playlist*, std::unique_ptr<QUndoStack>> m_histories;
    std::unordered_map<Playlist*, PlaylistViewState> m_states;
    std::unordered_map<Playlist*, QString> m_currentSearches;
    TrackList m_clipboard;
};
} // namespace Fooyin
