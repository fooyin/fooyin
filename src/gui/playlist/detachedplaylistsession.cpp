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

#include "detachedplaylistsession.h"

#include <core/library/musiclibrary.h>
#include <core/player/playercontroller.h>

#include <ranges>

namespace Fooyin {
std::unique_ptr<PlaylistWidgetSession> PlaylistWidgetSession::createDetachedPlaylist()
{
    return std::make_unique<DetachedPlaylistSession>();
}

std::unique_ptr<PlaylistWidgetSession> PlaylistWidgetSession::createDetachedLibrary()
{
    return std::make_unique<DetachedLibrarySession>();
}

QString DetachedSearchSession::emptyText() const
{
    return PlaylistWidget::tr("No results");
}

QString DetachedSearchSession::loadingText() const
{
    return PlaylistWidget::tr("Searching…");
}

int DetachedSearchSession::renderedTrackCount(const PlaylistController* /*playlistController*/) const
{
    return static_cast<int>(filteredTracks().size());
}

Playlist* DetachedSearchSession::modelPlaylist(Playlist* /*currentPlaylist*/) const
{
    return nullptr;
}

PlaylistTrackList DetachedSearchSession::modelTracks(Playlist* /*currentPlaylist*/) const
{
    return filteredTracks();
}

bool DetachedSearchSession::canResetWithoutPlaylist() const
{
    return true;
}

void DetachedSearchSession::handleTracksChanged(PlaylistWidgetSessionHost& host, const std::vector<int>& /*indexes*/,
                                                bool /*allNew*/)
{
    if(search().isEmpty() && filteredTracks().empty()) {
        return;
    }

    searchEvent(host, search());
}

void DetachedSearchSession::handleSearchChanged(PlaylistWidgetSessionHost& host, const QString& /*search*/)
{
    host.resetSort(true);
}

void DetachedSearchSession::finalise(PlaylistWidgetSessionHost& host)
{
    auto* receiver = host.sessionWidget();
    auto* hostPtr  = &host;
    QMetaObject::invokeMethod(receiver, [hostPtr]() { hostPtr->resetSort(true); }, Qt::QueuedConnection);
}

PlaylistWidget::ModeCapabilities DetachedPlaylistSession::capabilities() const
{
    return {.playlistBackedSelection = true};
}

TrackSelection DetachedPlaylistSession::selection(Playlist* currentPlaylist, const TrackList& tracks,
                                                  const std::set<int>& trackIndexes,
                                                  const PlaylistTrack& firstTrack) const
{
    TrackSelection trackSelection
        = makeTrackSelection(tracks, {trackIndexes.cbegin(), trackIndexes.cend()}, currentPlaylist);
    if(firstTrack.isValid()) {
        trackSelection.primaryPlaylistIndex = firstTrack.indexInPlaylist;
    }
    return trackSelection;
}

bool DetachedPlaylistSession::canDequeue(const PlayerController* playerController, Playlist* currentPlaylist,
                                         const std::set<int>& trackIndexes,
                                         const std::set<Track>& /*selectedTracks*/) const
{
    if(!currentPlaylist) {
        return false;
    }

    const auto queuedTracks = playerController->playbackQueue().indexesForPlaylist(currentPlaylist->id());
    return std::ranges::any_of(queuedTracks,
                               [&trackIndexes](const auto& track) { return trackIndexes.contains(track.first); });
}

PlaylistTrackList DetachedPlaylistSession::searchSourceTracks(const PlaylistController* playlistController,
                                                              const MusicLibrary* /*library*/) const
{
    if(const auto* playlist = playlistController->currentPlaylist()) {
        return playlist->playlistTracks();
    }

    return {};
}

void DetachedPlaylistSession::selectionChanged(PlaylistWidgetSessionHost& /*host*/) { }

PlaylistWidget::ModeCapabilities DetachedLibrarySession::capabilities() const
{
    return {};
}

TrackSelection DetachedLibrarySession::selection(Playlist* /*currentPlaylist*/, const TrackList& tracks,
                                                 const std::set<int>& /*trackIndexes*/,
                                                 const PlaylistTrack& /*firstTrack*/) const
{
    TrackSelection trackSelection;
    trackSelection.tracks = tracks;
    return trackSelection;
}

bool DetachedLibrarySession::canDequeue(const PlayerController* playerController, Playlist* /*currentPlaylist*/,
                                        const std::set<int>& /*trackIndexes*/,
                                        const std::set<Track>& selectedTracks) const
{
    const auto queuedTracks = playerController->playbackQueue().tracks();
    return std::ranges::any_of(
        queuedTracks, [&selectedTracks](const PlaylistTrack& track) { return selectedTracks.contains(track.track); });
}

PlaylistTrackList DetachedLibrarySession::searchSourceTracks(const PlaylistController* /*playlistController*/,
                                                             const MusicLibrary* library) const
{
    return PlaylistTrack::fromTracks(library->tracks(), {});
}

bool DetachedLibrarySession::canResetWithoutPlaylist() const
{
    return true;
}

PlaylistAction::ActionOptions DetachedLibrarySession::playbackOptions() const
{
    return PlaylistAction::TempPlaylist;
}
} // namespace Fooyin
