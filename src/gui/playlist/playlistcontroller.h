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

#pragma once

#include "playlist/playlistmodel.h"

#include <core/playlist/playlist.h>

#include <QObject>

class QUndoCommand;

namespace Fooyin {
class SettingsManager;
class SortingRegistry;
class PlayerManager;
enum class PlayState;
class Playlist;
class PlaylistManager;
class TrackSelectionController;
class PresetRegistry;

struct PlaylistViewState
{
    int topIndex;
    int scrollPos{0};
};

class PlaylistController : public QObject
{
    Q_OBJECT

public:
    PlaylistController(PlaylistManager* handler, PlayerManager* playerManager, PresetRegistry* presetRegistry,
                       SortingRegistry* sortRegistry, TrackSelectionController* selectionController,
                       SettingsManager* settings, QObject* parent = nullptr);
    ~PlaylistController() override;

    [[nodiscard]] PlaylistManager* playlistHandler() const;
    [[nodiscard]] PresetRegistry* presetRegistry() const;
    [[nodiscard]] SortingRegistry* sortRegistry() const;
    [[nodiscard]] TrackSelectionController* selectionController() const;

    [[nodiscard]] PlaylistList playlists() const;
    [[nodiscard]] Track currentTrack() const;
    [[nodiscard]] PlayState playState() const;

    void startPlayback() const;

    [[nodiscard]] bool currentIsActive() const;
    [[nodiscard]] Playlist* currentPlaylist() const;

    void changeCurrentPlaylist(Playlist* playlist);
    void changeCurrentPlaylist(int id);
    void changePlaylistIndex(int playlistId, int index);

    std::optional<PlaylistViewState> playlistState(int playlistId) const;
    void savePlaylistState(int playlistId, const PlaylistViewState& state);

    void addToHistory(QUndoCommand* command);
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;
    void undoPlaylistChanges();
    void redoPlaylistChanges();

signals:
    void currentPlaylistChanged(Playlist* playlist);
    void currentTrackChanged(const Track& track);
    void playStateChanged(PlayState state);
    void playlistHistoryChanged();

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
