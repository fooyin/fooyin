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

#include "playlistcontroller.h"

#include "playlistcolumnregistry.h"
#include "playlistuicontroller.h"
#include "playlistworkspace.h"
#include "presetregistry.h"

#include <core/application.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <core/track.h>
#include <gui/trackselectioncontroller.h>
#include <utils/settings/settingsmanager.h>

#include <algorithm>
#include <ranges>
#include <set>

namespace Fooyin {
PlaylistController::PlaylistController(Application* app, TrackSelectionController* selectionController, QObject* parent)
    : QObject{parent}
    , m_handler{app->playlistHandler()}
    , m_playerController{app->playerController()}
    , m_selectionController{selectionController}
    , m_settings{app->settingsManager()}
    , m_presetRegistry{new PresetRegistry(m_settings, this)}
    , m_columnRegistry{new PlaylistColumnRegistry(m_settings, this)}
    , m_uiController{new PlaylistUiController(this, this)}
    , m_workspace{std::make_unique<PlaylistWorkspace>(m_settings)}
{
    m_workspace->restorePlaylistStates(m_handler);

    QObject::connect(m_handler, &PlaylistHandler::playlistsPopulated, this, &PlaylistController::restoreLastPlaylist);
    QObject::connect(m_handler, &PlaylistHandler::playlistAdded, this, &PlaylistController::handlePlaylistAdded);
    QObject::connect(m_handler, &PlaylistHandler::tracksChanged, this, &PlaylistController::handlePlaylistUpdated);
    QObject::connect(m_handler, &PlaylistHandler::tracksUpdated, this, &PlaylistController::handleTracksUpdated);
    QObject::connect(m_handler, &PlaylistHandler::tracksAdded, this, &PlaylistController::handlePlaylistTracksAdded);
    QObject::connect(m_handler, &PlaylistHandler::playlistRemoved, this, &PlaylistController::handlePlaylistRemoved);

    QObject::connect(m_playerController, &PlayerController::playlistTrackChanged, this,
                     &PlaylistController::playingTrackChanged);
    QObject::connect(m_playerController, &PlayerController::tracksQueued, this,
                     &PlaylistController::handleTracksQueued);
    QObject::connect(m_playerController, &PlayerController::trackQueueChanged, this,
                     &PlaylistController::handleQueueChanged);
    QObject::connect(m_playerController, &PlayerController::tracksDequeued, this,
                     [this](const QueueTracks& tracks) { handleTracksDequeued(tracks); });
    QObject::connect(m_playerController, &PlayerController::trackIndexesDequeued, this,
                     [this](const PlaylistIndexes& indexes) { handleTracksDequeued(indexes); });
    QObject::connect(m_playerController, &PlayerController::playStateChanged, this,
                     &PlaylistController::playStateChanged);
}

PlaylistController::~PlaylistController()
{
    m_workspace->savePersistentState();
}

PlayerController* PlaylistController::playerController() const
{
    return m_playerController;
}

PlaylistHandler* PlaylistController::playlistHandler() const
{
    return m_handler;
}

TrackSelectionController* PlaylistController::selectionController() const
{
    return m_selectionController;
}

PresetRegistry* PlaylistController::presetRegistry() const
{
    return m_presetRegistry;
}

PlaylistColumnRegistry* PlaylistController::columnRegistry() const
{
    return m_columnRegistry;
}

PlaylistUiController* PlaylistController::uiController() const
{
    return m_uiController;
}

bool PlaylistController::playlistsHaveLoaded() const
{
    return m_workspace->playlistsHaveLoaded();
}

PlaylistList PlaylistController::playlists() const
{
    return m_handler->playlists();
}

PlaylistTrack PlaylistController::currentTrack() const
{
    return m_playerController->currentPlaylistTrack();
}

Player::PlayState PlaylistController::playState() const
{
    return m_playerController->playState();
}

void PlaylistController::aboutToChangeTracks()
{
    m_changingTracks = true;
}

void PlaylistController::changedTracks()
{
    m_changingTracks = false;
}

void PlaylistController::startPlayback() const
{
    if(auto* currentPlaylist = m_workspace->currentPlaylist()) {
        m_playerController->startPlayback(currentPlaylist->id());
    }
}

bool PlaylistController::currentIsActive() const
{
    return m_workspace->currentIsActive(m_handler);
}

bool PlaylistController::currentIsAuto() const
{
    return m_workspace->currentIsAuto();
}

Playlist* PlaylistController::currentPlaylist() const
{
    return m_workspace->currentPlaylist();
}

UId PlaylistController::currentPlaylistId() const
{
    return m_workspace->currentPlaylistId();
}

void PlaylistController::changeCurrentPlaylist(Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    auto* prevPlaylist = m_workspace->changeCurrentPlaylist(m_handler, m_playerController, playlist);

    if(prevPlaylist == playlist) {
        m_workspace->resetPlaylistSessionState(playlist);

        emit playlistHistoryChanged();
        return;
    }

    emit currentPlaylistChanged(prevPlaylist, playlist);
}

void PlaylistController::changeCurrentPlaylist(const UId& id)
{
    if(auto* playlist = m_handler->playlistById(id)) {
        changeCurrentPlaylist(playlist);
    }
}

void PlaylistController::changePlaylistIndex(const UId& playlistId, int index)
{
    m_handler->changePlaylistIndex(playlistId, index);
}

void PlaylistController::clearCurrentPlaylist()
{
    if(auto* currentPlaylist = m_workspace->currentPlaylist()) {
        m_handler->clearPlaylistTracks(currentPlaylist->id());
    }
}

QString PlaylistController::currentSearch(Playlist* playlist) const
{
    return m_workspace->currentSearch(playlist);
}

void PlaylistController::setSearch(Playlist* playlist, const QString& search)
{
    m_workspace->setCurrentSearch(playlist, search);
}

std::optional<PlaylistViewState> PlaylistController::playlistState(Playlist* playlist) const
{
    return m_workspace->playlistState(playlist);
}

void PlaylistController::savePlaylistState(Playlist* playlist, const PlaylistViewState& state)
{
    m_workspace->savePlaylistState(playlist, state);
}

void PlaylistController::addToHistory(QUndoCommand* command)
{
    if(!command || !m_workspace->currentPlaylist()) {
        return;
    }

    m_workspace->addToHistory(command);

    emit playlistHistoryChanged();
}

bool PlaylistController::canUndo() const
{
    return m_workspace->canUndo();
}

bool PlaylistController::canRedo() const
{
    return m_workspace->canRedo();
}

void PlaylistController::undoPlaylistChanges()
{
    if(!m_workspace->currentPlaylist()) {
        return;
    }

    if(canUndo()) {
        m_workspace->undo();
        emit playlistHistoryChanged();
    }
}

void PlaylistController::redoPlaylistChanges()
{
    if(!m_workspace->currentPlaylist()) {
        return;
    }

    if(canRedo()) {
        m_workspace->redo();
        emit playlistHistoryChanged();
    }
}

void PlaylistController::clearHistory()
{
    m_workspace->clearHistory();
}

bool PlaylistController::clipboardEmpty() const
{
    return m_workspace->clipboardEmpty();
}

TrackList PlaylistController::clipboard() const
{
    return m_workspace->clipboard();
}

void PlaylistController::setClipboard(const TrackList& tracks)
{
    m_workspace->setClipboard(tracks);
    emit clipboardChanged();
}

void PlaylistController::handleTrackSelectionAction(TrackAction action)
{
    if(action == TrackAction::SendCurrentPlaylist && m_workspace->currentPlaylist()) {
        m_workspace->resetCurrentPlaylistSessionState();
    }
}

void PlaylistController::restoreLastPlaylist()
{
    m_workspace->restoreLastPlaylist(m_handler, m_playerController);
    emit playlistsLoaded();
}

void PlaylistController::handlePlaylistAdded(Playlist* playlist)
{
    if(playlist) {
        m_workspace->resetPlaylistSessionState(playlist);
    }
}

void PlaylistController::handlePlaylistTracksAdded(Playlist* playlist, const TrackList& tracks, int index)
{
    if(m_workspace->isCurrentPlaylist(playlist)) {
        emit currentPlaylistTracksAdded(tracks, index);
    }
}

void PlaylistController::handleTracksQueued(const QueueTracks& tracks)
{
    auto* currentPlaylist = m_workspace->currentPlaylist();
    if(!currentPlaylist) {
        return;
    }

    std::set<int> uniqueIndexes;

    for(const auto& track : tracks) {
        if(track.playlistId == currentPlaylist->id()) {
            uniqueIndexes.emplace(track.indexInPlaylist);
        }
    }

    const std::vector<int> indexes{uniqueIndexes.cbegin(), uniqueIndexes.cend()};

    if(!indexes.empty()) {
        emit currentPlaylistQueueChanged(indexes);
    }
}

void PlaylistController::handleTracksDequeued(const QueueTracks& tracks)
{
    auto* currentPlaylist = m_workspace->currentPlaylist();
    if(!currentPlaylist) {
        return;
    }

    std::set<int> uniqueIndexes;

    for(const auto& track : tracks) {
        if(track.playlistId == currentPlaylist->id()) {
            uniqueIndexes.emplace(track.indexInPlaylist);
        }
    }

    const auto queuedTracks = m_playerController->playbackQueue().indexesForPlaylist(currentPlaylist->id());
    for(const auto& trackIndex : queuedTracks | std::views::keys) {
        uniqueIndexes.emplace(trackIndex);
    }

    const std::vector<int> indexes{uniqueIndexes.cbegin(), uniqueIndexes.cend()};

    if(!indexes.empty()) {
        emit currentPlaylistQueueChanged(indexes);
    }
}

void PlaylistController::handleTracksDequeued(const PlaylistIndexes& indexes)
{
    auto* currentPlaylist = m_workspace->currentPlaylist();
    if(!currentPlaylist) {
        return;
    }

    std::set<int> uniqueIndexes;

    if(indexes.contains(currentPlaylist->id())) {
        const auto playlistIndexes = indexes.at(currentPlaylist->id());
        for(const auto& trackIndex : playlistIndexes) {
            uniqueIndexes.emplace(trackIndex);
        }
    }

    const auto queuedTracks = m_playerController->playbackQueue().indexesForPlaylist(currentPlaylist->id());
    for(const auto& trackIndex : queuedTracks | std::views::keys) {
        uniqueIndexes.emplace(trackIndex);
    }

    const std::vector<int> playlistIndexes{uniqueIndexes.cbegin(), uniqueIndexes.cend()};

    if(!indexes.empty()) {
        emit currentPlaylistQueueChanged(playlistIndexes);
    }
}

void PlaylistController::handleQueueChanged(const QueueTracks& removed, const QueueTracks& added)
{
    auto* currentPlaylist = m_workspace->currentPlaylist();
    if(!currentPlaylist) {
        return;
    }

    std::set<int> uniqueIndexes;

    auto gatherIndexes = [currentPlaylist, &uniqueIndexes](const QueueTracks& tracks) {
        for(const auto& track : tracks) {
            if(track.playlistId == currentPlaylist->id()) {
                uniqueIndexes.emplace(track.indexInPlaylist);
            }
        }
    };

    gatherIndexes(removed);
    gatherIndexes(added);

    const std::vector<int> indexes{uniqueIndexes.cbegin(), uniqueIndexes.cend()};

    if(!indexes.empty()) {
        emit currentPlaylistQueueChanged(indexes);
    }
}

void PlaylistController::handlePlaylistUpdated(Playlist* playlist, const std::vector<int>& indexes)
{
    if(m_changingTracks) {
        return;
    }

    bool allNew{false};

    if(playlist && (indexes.empty() || std::cmp_equal(indexes.size(), playlist->trackCount()))) {
        allNew = true;
        m_workspace->resetPlaylistSessionState(playlist);

        auto queueTracks = m_playerController->playbackQueue().tracks();
        for(auto& track : queueTracks) {
            if(track.playlistId == playlist->id()) {
                track.playlistId      = {};
                track.indexInPlaylist = -1;
            }
        }
        m_playerController->replaceTracks(queueTracks);
    }

    if(m_workspace->isCurrentPlaylist(playlist)) {
        emit currentPlaylistTracksChanged(indexes, allNew);
    }
}

void PlaylistController::handleTracksUpdated(Playlist* playlist, const std::vector<int>& indexes)
{
    if(m_changingTracks) {
        return;
    }

    if(m_workspace->isCurrentPlaylist(playlist)) {
        emit currentPlaylistTracksPlayed(indexes);
    }
}

void PlaylistController::handlePlaylistRemoved(Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    m_workspace->removePlaylist(playlist);

    if(!m_workspace->isCurrentPlaylist(playlist)) {
        return;
    }

    if(m_handler->playlistCount() == 0) {
        QObject::connect(
            m_handler, &PlaylistHandler::playlistAdded, this,
            [this](Playlist* newPlaylist) {
                if(newPlaylist) {
                    changeCurrentPlaylist(newPlaylist);
                }
            },
            Qt::SingleShotConnection);
    }
    else {
        const int nextIndex = std::max(0, playlist->index() - 1);
        if(auto* nextPlaylist = m_handler->playlistByIndex(nextIndex)) {
            changeCurrentPlaylist(nextPlaylist);
        }
    }
}
} // namespace Fooyin

#include "moc_playlistcontroller.cpp"
