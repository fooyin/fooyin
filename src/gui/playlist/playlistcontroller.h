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

#include <core/playlist/playlist.h>

#include <QObject>

class QUndoCommand;

namespace Fooyin {
class SettingsManager;
class SortingRegistry;
class PlayerManager;
enum class PlayState;
enum class TrackAction;
class Playlist;
class PlaylistManager;
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
    PlaylistController(PlaylistManager* handler, PlayerManager* playerManager, MusicLibrary* library,
                       TrackSelectionController* selectionController, SettingsManager* settings,
                       QObject* parent = nullptr);
    ~PlaylistController() override;

    [[nodiscard]] PlaylistManager* playlistHandler() const;
    [[nodiscard]] TrackSelectionController* selectionController() const;

    [[nodiscard]] bool playlistsHaveLoaded() const;
    [[nodiscard]] PlaylistList playlists() const;
    [[nodiscard]] Track currentTrack() const;
    [[nodiscard]] PlayState playState() const;

    void startPlayback() const;

    [[nodiscard]] bool currentIsActive() const;
    [[nodiscard]] Playlist* currentPlaylist() const;
    [[nodiscard]] int currentPlaylistId() const;

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

    void filesToCurrentPlaylist(const QList<QUrl>& urls);
    void filesToNewPlaylist(const QString& playlistName, const QList<QUrl>& urls);
    void filesToTracks(const QList<QUrl>& urls, const std::function<void(const TrackList&)>& func);

signals:
    void playlistsLoaded();
    void currentPlaylistChanged(Playlist* playlist);
    void currentTrackChanged(const Track& track);
    void playStateChanged(PlayState state);
    void playlistHistoryChanged();

public slots:
    void handleTrackSelectionAction(TrackAction action);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
