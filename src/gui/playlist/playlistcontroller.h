/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/player/playbackqueue.h>
#include <core/playlist/playlist.h>

#include <QObject>

class QMenu;
class QUndoCommand;

namespace Fooyin {
class PlayerController;
class Playlist;
class PlaylistColumnRegistry;
class PlaylistControllerPrivate;
class PlaylistHandler;
struct PlaylistTrack;
class PresetRegistry;
class SettingsManager;
enum class TrackAction;
class TrackSelectionController;

struct PlaylistViewState
{
    int topIndex{-1};
    int scrollPos{0};
};

class PlaylistController : public QObject
{
    Q_OBJECT

public:
    PlaylistController(PlaylistHandler* handler, PlayerController* playerController,
                       TrackSelectionController* selectionController, SettingsManager* settings,
                       QObject* parent = nullptr);
    ~PlaylistController() override;

    [[nodiscard]] PlayerController* playerController() const;
    [[nodiscard]] PlaylistHandler* playlistHandler() const;
    [[nodiscard]] TrackSelectionController* selectionController() const;
    [[nodiscard]] PresetRegistry* presetRegistry() const;
    [[nodiscard]] PlaylistColumnRegistry* columnRegistry() const;

    [[nodiscard]] bool playlistsHaveLoaded() const;
    [[nodiscard]] PlaylistList playlists() const;
    [[nodiscard]] PlaylistTrack currentTrack() const;
    [[nodiscard]] Player::PlayState playState() const;

    void aboutToChangeTracks();
    void changedTracks();

    void addPlaylistMenu(QMenu* menu);

    void startPlayback() const;
    void showNowPlaying();
    void selectTrackIds(const std::set<int>& ids);

    [[nodiscard]] bool currentIsActive() const;
    [[nodiscard]] Playlist* currentPlaylist() const;
    [[nodiscard]] UId currentPlaylistId() const;

    void changeCurrentPlaylist(Playlist* playlist);
    void changeCurrentPlaylist(const UId& id);
    void changePlaylistIndex(const UId& playlistId, int index);
    void clearCurrentPlaylist();

    [[nodiscard]] std::optional<PlaylistViewState> playlistState(Playlist* playlist) const;
    void savePlaylistState(Playlist* playlist, const PlaylistViewState& state);

    void addToHistory(QUndoCommand* command);
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;
    void undoPlaylistChanges();
    void redoPlaylistChanges();
    void clearHistory();

    [[nodiscard]] bool clipboardEmpty() const;
    [[nodiscard]] TrackList clipboard() const;
    void setClipboard(const TrackList& tracks);

signals:
    void playlistsLoaded();
    void currentPlaylistChanged(Fooyin::Playlist* prevPlaylist, Fooyin::Playlist* playlist);
    void currentPlaylistTracksChanged(const std::vector<int>& indexes, bool allNew);
    void currentPlaylistTracksPlayed(const std::vector<int>& indexes);
    void currentPlaylistTracksAdded(const Fooyin::TrackList& tracks, int index);
    void currentPlaylistTracksRemoved(const std::vector<int>& indexes);
    void currentPlaylistQueueChanged(const std::vector<int>& tracks);

    void playStateChanged(Player::PlayState state);
    void clipboardChanged();
    void playlistHistoryChanged();
    void playingTrackChanged(const Fooyin::PlaylistTrack& track);
    void showCurrentTrack();
    void selectTracks(const std::set<int>& ids);

public slots:
    void handleTrackSelectionAction(TrackAction action);

private:
    std::unique_ptr<PlaylistControllerPrivate> p;
};
} // namespace Fooyin
