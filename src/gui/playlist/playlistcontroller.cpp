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

#include "playlistcontroller.h"

#include "playlist/playlisthistory.h"
#include "presetregistry.h"

#include <core/library/sortingregistry.h>
#include <core/player/playermanager.h>
#include <core/playlist/playlistmanager.h>
#include <core/track.h>
#include <gui/guisettings.h>
#include <utils/settings/settingsmanager.h>

#include <QUndoStack>

namespace Fooyin {
struct PlaylistController::Private
{
    PlaylistController* self;

    PlaylistManager* handler;
    PlayerManager* playerManager;
    PresetRegistry* presetRegistry;
    SortingRegistry* sortRegistry;
    TrackSelectionController* selectionController;
    SettingsManager* settings;

    Playlist* currentPlaylist{nullptr};

    std::map<int, QUndoStack> histories;

    Private(PlaylistController* self, PlaylistManager* handler, PlayerManager* playerManager,
            PresetRegistry* presetRegistry, SortingRegistry* sortRegistry,
            TrackSelectionController* selectionController, SettingsManager* settings)
        : self{self}
        , handler{handler}
        , playerManager{playerManager}
        , presetRegistry{presetRegistry}
        , sortRegistry{sortRegistry}
        , selectionController{selectionController}
        , settings{settings}
    { }

    void restoreLastPlaylist()
    {
        const int lastId = settings->value<Gui::Settings::LastPlaylistId>();
        if(lastId >= 0) {
            if(auto* playlist = handler->playlistById(lastId)) {
                currentPlaylist = playlist;
                QMetaObject::invokeMethod(self, "currentPlaylistChanged", Q_ARG(Playlist*, playlist));
            }
        }
    }

    void handlePlaylistUpdated(Playlist* playlist)
    {
        if(playlist) {
            histories.erase(playlist->id());
        }
        if(currentPlaylist == playlist) {
            self->changeCurrentPlaylist(playlist);
        }
    }

    void handlePlaylistRemoved(const Playlist* playlist) const
    {
        if(currentPlaylist == playlist) {
            const int nextIndex = playlist->index() <= 1 ? 0 : playlist->index() - 1;
            if(auto* nextPlaylist = handler->playlistByIndex(nextIndex)) {
                self->changeCurrentPlaylist(nextPlaylist);
            }
        }
    }
};

PlaylistController::PlaylistController(PlaylistManager* handler, PlayerManager* playerManager,
                                       PresetRegistry* presetRegistry, SortingRegistry* sortRegistry,
                                       TrackSelectionController* selectionController, SettingsManager* settings,
                                       QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, handler, playerManager, presetRegistry, sortRegistry, selectionController,
                                  settings)}
{
    QObject::connect(handler, &PlaylistManager::playlistsPopulated, this, [this]() { p->restoreLastPlaylist(); });
    QObject::connect(handler, &PlaylistManager::playlistTracksChanged, this,
                     [this](Playlist* playlist) { p->handlePlaylistUpdated(playlist); });
    QObject::connect(handler, &PlaylistManager::playlistRemoved, this,
                     [this](const Playlist* playlist) { p->handlePlaylistRemoved(playlist); });
    QObject::connect(handler, &PlaylistManager::playlistRenamed, this,
                     [this](Playlist* playlist) { p->handlePlaylistUpdated(playlist); });

    QObject::connect(playerManager, &PlayerManager::currentTrackChanged, this,
                     &PlaylistController::currentTrackChanged);
    QObject::connect(playerManager, &PlayerManager::playStateChanged, this, &PlaylistController::playStateChanged);
}

PlaylistController::~PlaylistController()
{
    if(p->currentPlaylist) {
        p->settings->set<Gui::Settings::LastPlaylistId>(p->currentPlaylist->id());
    }
}

PlaylistManager* PlaylistController::playlistHandler() const
{
    return p->handler;
}

PresetRegistry* PlaylistController::presetRegistry() const
{
    return p->presetRegistry;
}

SortingRegistry* PlaylistController::sortRegistry() const
{
    return p->sortRegistry;
}

TrackSelectionController* PlaylistController::selectionController() const
{
    return p->selectionController;
}

const PlaylistList& PlaylistController::playlists() const
{
    return p->handler->playlists();
}

Track PlaylistController::currentTrack() const
{
    return p->playerManager->currentTrack();
}

PlayState PlaylistController::playState() const
{
    return p->playerManager->playState();
}

Playlist* PlaylistController::currentPlaylist() const
{
    return p->currentPlaylist;
}

void PlaylistController::changeCurrentPlaylist(Playlist* playlist)
{
    if(std::exchange(p->currentPlaylist, playlist) == playlist) {
        p->histories.erase(playlist->id());
    }
    emit currentPlaylistChanged(playlist);
    emit playlistHistoryChanged();
}

void PlaylistController::changeCurrentPlaylist(int id)
{
    if(auto* playlist = p->handler->playlistById(id)) {
        changeCurrentPlaylist(playlist);
    }
}

void PlaylistController::changePlaylistIndex(int playlistId, int index)
{
    p->handler->changePlaylistIndex(playlistId, index);
}

void PlaylistController::addToHistory(QUndoCommand* command)
{
    if(!p->currentPlaylist) {
        return;
    }
    p->histories[p->currentPlaylist->id()].push(command);
    emit playlistHistoryChanged();
}

bool PlaylistController::canUndo() const
{
    if(!p->currentPlaylist || p->histories.empty()) {
        return false;
    }

    const int currentId = p->currentPlaylist->id();

    if(!p->histories.contains(currentId)) {
        return false;
    }

    return p->histories.at(currentId).canUndo();
}

bool PlaylistController::canRedo() const
{
    if(!p->currentPlaylist || p->histories.empty()) {
        return false;
    }

    const int currentId = p->currentPlaylist->id();

    if(!p->histories.contains(currentId)) {
        return false;
    }

    return p->histories.at(currentId).canRedo();
}

void PlaylistController::undoPlaylistChanges()
{
    if(!p->currentPlaylist) {
        return;
    }

    if(canUndo()) {
        const int currentId = p->currentPlaylist->id();
        p->histories.at(currentId).undo();
        emit playlistHistoryChanged();
    }
}

void PlaylistController::redoPlaylistChanges()
{
    if(!p->currentPlaylist) {
        return;
    }

    if(canRedo()) {
        const int currentId = p->currentPlaylist->id();
        p->histories.at(currentId).redo();
        emit playlistHistoryChanged();
    }
}

void PlaylistController::startPlayback() const
{
    if(p->currentPlaylist) {
        p->handler->startPlayback(p->currentPlaylist->id());
    }
}

bool PlaylistController::currentIsActive() const
{
    return p->currentPlaylist == p->handler->activePlaylist();
}
} // namespace Fooyin

#include "moc_playlistcontroller.cpp"
