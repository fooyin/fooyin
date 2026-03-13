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

#include "playlistwidgetsession.h"

namespace Fooyin {
class DetachedSearchSession : public PlaylistWidgetSession
{
public:
    [[nodiscard]] QString emptyText() const override;
    [[nodiscard]] QString loadingText() const override;
    [[nodiscard]] int renderedTrackCount(const PlaylistController* playlistController) const override;
    [[nodiscard]] Playlist* modelPlaylist(Playlist* currentPlaylist) const override;
    [[nodiscard]] PlaylistTrackList modelTracks(Playlist* currentPlaylist) const override;
    [[nodiscard]] bool canResetWithoutPlaylist() const override;

    void handleSearchChanged(PlaylistWidgetSessionHost& host, const QString& search) override;
    void finalise(PlaylistWidgetSessionHost& host) override;
};

class DetachedPlaylistSession final : public DetachedSearchSession
{
public:
    [[nodiscard]] PlaylistWidget::ModeCapabilities capabilities() const override;
    [[nodiscard]] TrackSelection selection(Playlist* currentPlaylist, const TrackList& tracks,
                                           const std::set<int>& trackIndexes,
                                           const PlaylistTrack& firstTrack) const override;
    [[nodiscard]] bool canDequeue(const PlayerController* playerController, Playlist* currentPlaylist,
                                  const std::set<int>& trackIndexes,
                                  const std::set<Track>& selectedTracks) const override;
    [[nodiscard]] PlaylistTrackList searchSourceTracks(const PlaylistController* playlistController,
                                                       const MusicLibrary* library) const override;
    void selectionChanged(PlaylistWidgetSessionHost& host) override;
};

class DetachedLibrarySession final : public DetachedSearchSession
{
public:
    [[nodiscard]] PlaylistWidget::ModeCapabilities capabilities() const override;
    [[nodiscard]] TrackSelection selection(Playlist* currentPlaylist, const TrackList& tracks,
                                           const std::set<int>& trackIndexes,
                                           const PlaylistTrack& firstTrack) const override;
    [[nodiscard]] bool canDequeue(const PlayerController* playerController, Playlist* currentPlaylist,
                                  const std::set<int>& trackIndexes,
                                  const std::set<Track>& selectedTracks) const override;
    [[nodiscard]] PlaylistTrackList searchSourceTracks(const PlaylistController* playlistController,
                                                       const MusicLibrary* library) const override;
    [[nodiscard]] bool canResetWithoutPlaylist() const override;
    [[nodiscard]] PlaylistAction::ActionOptions playbackOptions() const override;
};
} // namespace Fooyin
