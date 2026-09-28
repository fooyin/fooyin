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

#include "detachedplaylistsession.h"

#include "playlistview.h"

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

std::unique_ptr<PlaylistWidgetSession> PlaylistWidgetSession::createDetachedTracks(const TrackList& tracks)
{
    return std::make_unique<DetachedTrackListSession>(tracks);
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
    if(search().isEmpty() && emptyMode() == EmptySearchMode::Clear) {
        return;
    }

    searchEvent(host, {search(), emptyMode()});
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

void DetachedLibrarySession::setupConnections(PlaylistWidgetSessionHost& host)
{
    auto* widget  = host.sessionWidget();
    auto* hostPtr = &host;
    auto refresh  = [this, hostPtr]() {
        handleTracksChanged(*hostPtr, {}, false);
    };

    auto* library = host.musicLibrary();
    QObject::connect(library, &MusicLibrary::tracksLoaded, widget, refresh);
    QObject::connect(library, &MusicLibrary::tracksAdded, widget, refresh);
    QObject::connect(library, &MusicLibrary::tracksMetadataChanged, widget, refresh);
    QObject::connect(library, &MusicLibrary::tracksUpdated, widget, refresh);
    QObject::connect(library, &MusicLibrary::tracksDeleted, widget, refresh);
    QObject::connect(library, &MusicLibrary::tracksSorted, widget, refresh);
}

DetachedTrackListSession::DetachedTrackListSession(const TrackList& tracks)
    : m_tracks{PlaylistTrack::fromTracks(tracks, {})}
{
    setFilteredTracks(m_tracks);
}

PlaylistWidget::ModeCapabilities DetachedTrackListSession::capabilities() const
{
    return {};
}

TrackSelection DetachedTrackListSession::selection(Playlist* /*currentPlaylist*/, const TrackList& tracks,
                                                   const std::set<int>& /*trackIndexes*/,
                                                   const PlaylistTrack& /*firstTrack*/) const
{
    TrackSelection trackSelection;
    trackSelection.tracks = tracks;
    return trackSelection;
}

bool DetachedTrackListSession::canDequeue(const PlayerController* playerController, Playlist* /*currentPlaylist*/,
                                          const std::set<int>& /*trackIndexes*/,
                                          const std::set<Track>& selectedTracks) const
{
    const auto queuedTracks = playerController->playbackQueue().tracks();
    return std::ranges::any_of(
        queuedTracks, [&selectedTracks](const PlaylistTrack& track) { return selectedTracks.contains(track.track); });
}

PlaylistTrackList DetachedTrackListSession::searchSourceTracks(const PlaylistController* /*playlistController*/,
                                                               const MusicLibrary* /*library*/) const
{
    return m_tracks;
}

void DetachedTrackListSession::startPlayback(PlaylistWidgetSessionHost& host) const
{
    const auto& playlistTracks = filteredTracks();
    if(playlistTracks.empty()) {
        return;
    }

    int trackIndex{0};
    const QModelIndex currentIndex = host.playlistView()->currentIndex();
    if(currentIndex.isValid() && currentIndex.data(PlaylistItem::Type).toInt() == PlaylistItem::Track) {
        const auto currentTrack = currentIndex.data(PlaylistItem::Role::PersistentItemData).value<PlaylistTrack>();
        const auto trackIt      = std::ranges::find(playlistTracks, currentTrack.entryId, &PlaylistTrack::entryId);
        if(trackIt != playlistTracks.cend()) {
            trackIndex = static_cast<int>(std::distance(playlistTracks.cbegin(), trackIt));
        }
    }

    host.selectionController()->startPlayback(PlaylistTrack::toTracks(playlistTracks), trackIndex);
}

void DetachedTrackListSession::setupConnections(PlaylistWidgetSessionHost& host)
{
    auto* model  = host.playlistModel();
    auto* player = host.playerController();

    const auto updatePlayingTrack = [this, model](const PlaylistTrack& track) {
        model->playingTrackChanged(playingTrackForView(track));
    };

    model->setMatchPlayingTrackByIdentity(true);
    updatePlayingTrack(player->currentPlaylistTrack());
    model->playStateChanged(player->playState());

    QObject::connect(player, &PlayerController::playlistTrackChanged, model, updatePlayingTrack);
    QObject::connect(player, &PlayerController::playlistTrackUpdated, model, updatePlayingTrack);
    QObject::connect(
        player, &PlayerController::trackChangeRequested, model,
        [updatePlayingTrack](const Player::TrackChangeRequest& request) { updatePlayingTrack(request.track); });
    QObject::connect(player, &PlayerController::playStateChanged, model, &PlaylistModel::playStateChanged);
    QObject::connect(player, &PlayerController::positionChangedSeconds, model,
                     &PlaylistModel::refreshPlayingTrackPositionData);
    QObject::connect(player, &PlayerController::positionMoved, model, &PlaylistModel::refreshPlayingTrackPositionData);
    QObject::connect(player, &PlayerController::bitrateChanged, model, &PlaylistModel::refreshPlayingTrackBitrateData);
    QObject::connect(player, &PlayerController::playbackOutputInfoChanged, model,
                     &PlaylistModel::refreshPlayingTrackOutputData);
}

PlaylistTrack DetachedTrackListSession::playingTrackForView(const PlaylistTrack& track) const
{
    PlaylistTrack viewTrack{track};
    viewTrack.indexInPlaylist = -1;

    if(track.indexInPlaylist < 0 || std::cmp_greater_equal(track.indexInPlaylist, filteredTracks().size())) {
        return viewTrack;
    }

    const auto& candidate = filteredTracks().at(track.indexInPlaylist);
    if(candidate.track.sameIdentityAs(track.track)) {
        viewTrack.indexInPlaylist = candidate.indexInPlaylist;
    }

    return viewTrack;
}

void DetachedTrackListSession::replaceTracks(PlaylistWidgetSessionHost& host, const TrackList& tracks)
{
    m_tracks = PlaylistTrack::fromTracks(tracks, {});

    if(hasSearch()) {
        searchEvent(host, {.text = search(), .emptyMode = emptyMode()});
        return;
    }

    setFilteredTracks(m_tracks);
    host.resetModelThrottled();
}
} // namespace Fooyin
