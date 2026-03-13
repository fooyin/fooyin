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

#pragma once

#include "playlistworkspacestate.h"

namespace Fooyin {
class PlayerController;
class Playlist;
class PlaylistHandler;
class SettingsManager;

class PlaylistWorkspace
{
public:
    explicit PlaylistWorkspace(SettingsManager* settings);
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
    void resetPlaylistSessionState(Playlist* playlist);
    void resetCurrentPlaylistSessionState();
    void removePlaylist(Playlist* playlist);

    [[nodiscard]] QString currentSearch(Playlist* playlist) const;
    void setCurrentSearch(Playlist* playlist, const QString& search);

    [[nodiscard]] std::optional<PlaylistViewState> playlistState(Playlist* playlist) const;
    void savePlaylistState(Playlist* playlist, const PlaylistViewState& state);

    void addToHistory(QUndoCommand* command);
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;
    void undo();
    void redo();
    void clearHistory();

    [[nodiscard]] bool clipboardEmpty() const;
    [[nodiscard]] TrackList clipboard() const;
    void setClipboard(const TrackList& tracks);

private:
    bool m_loaded;
    Playlist* m_currentPlaylist;
    PlaylistWorkspaceState m_state;
};
} // namespace Fooyin
