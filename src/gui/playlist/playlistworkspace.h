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

#include <optional>
#include <unordered_map>

class QUndoCommand;
class QUndoStack;

namespace Fooyin {
class PlayerController;
class Playlist;
class PlaylistHandler;

class PlaylistWorkspace
{
public:
    explicit PlaylistWorkspace();
    ~PlaylistWorkspace();

    void restorePlaylistStates(PlaylistHandler* handler);
    void restoreLastPlaylist(PlaylistHandler* handler, PlayerController* playerController);
    void savePersistentState() const;

    [[nodiscard]] bool playlistsHaveLoaded() const;
    [[nodiscard]] Playlist* currentPlaylist() const;
    [[nodiscard]] UId currentPlaylistId() const;
    [[nodiscard]] bool currentIsActive(const PlaylistHandler* handler) const;
    [[nodiscard]] bool currentIsAuto() const;
    [[nodiscard]] bool isCurrentPlaylist(const Playlist* playlist) const;

    [[nodiscard]] Playlist* changeCurrentPlaylist(PlaylistHandler* handler, PlayerController* playerController,
                                                  Playlist* playlist);
    void resetPlaylistViewState(Playlist* playlist);
    void clearPlaylistHistory(Playlist* playlist);
    void resetPlaylistSessionState(Playlist* playlist);
    void resetCurrentPlaylistSessionState();
    void removePlaylist(Playlist* playlist);

    [[nodiscard]] QString currentSearch(Playlist* playlist) const;
    void setCurrentSearch(Playlist* playlist, const QString& search);

    [[nodiscard]] std::optional<PlaylistViewState> playlistState(Playlist* playlist) const;
    void savePlaylistState(Playlist* playlist, const PlaylistViewState& state);

    void addToHistory(QUndoCommand* command);
    void addToHistory(Playlist* playlist, QUndoCommand* command);
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canUndo(Playlist* playlist) const;
    [[nodiscard]] bool canRedo() const;
    [[nodiscard]] bool canRedo(Playlist* playlist) const;
    void undo();
    void undo(Playlist* playlist);
    void redo();
    void redo(Playlist* playlist);
    void clearHistory();

    [[nodiscard]] bool clipboardEmpty() const;
    [[nodiscard]] TrackList clipboard() const;
    void setClipboard(const TrackList& tracks);

private:
    QUndoStack* history(Playlist* playlist);
    [[nodiscard]] const QUndoStack* history(Playlist* playlist) const;

    bool m_loaded;
    Playlist* m_currentPlaylist;
    std::unordered_map<Playlist*, std::unique_ptr<QUndoStack>> m_histories;
    std::unordered_map<Playlist*, PlaylistViewState> m_states;
    std::unordered_map<Playlist*, QString> m_currentSearches;
    TrackList m_clipboard;
};
} // namespace Fooyin
