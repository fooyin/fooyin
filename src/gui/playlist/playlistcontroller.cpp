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

#include "presetregistry.h"

#include <core/library/sortingregistry.h>
#include <core/playlist/playlistmanager.h>
#include <gui/guisettings.h>
#include <utils/settings/settingsmanager.h>

namespace Fy::Gui::Widgets::Playlist {
struct PlaylistController::Private
{
    PlaylistController* self;

    Core::Playlist::PlaylistManager* handler;
    PresetRegistry* presetRegistry;
    Core::Library::SortingRegistry* sortRegistry;
    Utils::SettingsManager* settings;

    int currentPlaylistId{-1};

    Private(PlaylistController* self, Core::Playlist::PlaylistManager* handler, PresetRegistry* presetRegistry,
            Core::Library::SortingRegistry* sortRegistry, Utils::SettingsManager* settings)
        : self{self}
        , handler{handler}
        , presetRegistry{presetRegistry}
        , sortRegistry{sortRegistry}
        , settings{settings}
    { }

    void restoreLastPlaylist()
    {
        const int lastId = settings->value<Settings::LastPlaylistId>();
        if(lastId >= 0) {
            auto playlist = handler->playlistById(lastId);
            if(playlist) {
                currentPlaylistId = playlist->id();
                QMetaObject::invokeMethod(self, "currentPlaylistChanged",
                                          Q_ARG(const Core::Playlist::Playlist&, *playlist));
            }
        }
    }

    void handlePlaylistAdded(const Core::Playlist::Playlist& playlist, bool switchTo) const
    {
        if(switchTo) {
            self->changeCurrentPlaylist(playlist);
        }
    }

    void handlePlaylistUpdated(const Core::Playlist::Playlist& playlist, bool switchTo) const
    {
        if(currentPlaylistId == playlist.id() || switchTo) {
            self->changeCurrentPlaylist(playlist);
        }
    }

    void handlePlaylistRemoved(const Core::Playlist::Playlist& playlist) const
    {
        if(currentPlaylistId == playlist.id()) {
            const int nextIndex = playlist.index() <= 1 ? 0 : playlist.index() - 1;
            if(auto nextPlaylist = handler->playlistByIndex(nextIndex)) {
                self->changeCurrentPlaylist(*nextPlaylist);
            }
        }
    }
};

PlaylistController::PlaylistController(Core::Playlist::PlaylistManager* handler, PresetRegistry* presetRegistry,
                                       Core::Library::SortingRegistry* sortRegistry, Utils::SettingsManager* settings,
                                       QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, handler, presetRegistry, sortRegistry, settings)}
{
    QObject::connect(handler, &Core::Playlist::PlaylistManager::playlistsPopulated, this,
                     [this]() { p->restoreLastPlaylist(); });
    QObject::connect(handler, &Core::Playlist::PlaylistManager::playlistTracksChanged, this,
                     [this](const Core::Playlist::Playlist& playlist, bool switchTo) {
                         p->handlePlaylistUpdated(playlist, switchTo);
                     });
    QObject::connect(handler, &Core::Playlist::PlaylistManager::playlistAdded, this,
                     [this](const Core::Playlist::Playlist& playlist, bool switchTo) {
                         p->handlePlaylistAdded(playlist, switchTo);
                     });
    QObject::connect(handler, &Core::Playlist::PlaylistManager::playlistRemoved, this,
                     [this](const Core::Playlist::Playlist& playlist) { p->handlePlaylistRemoved(playlist); });
    QObject::connect(handler, &Core::Playlist::PlaylistManager::playlistRenamed, this,
                     &PlaylistController::refreshCurrentPlaylist);
}

PlaylistController::~PlaylistController()
{
    p->settings->set<Settings::LastPlaylistId>(p->currentPlaylistId);
}

Core::Playlist::PlaylistManager* PlaylistController::playlistHandler() const
{
    return p->handler;
}

PresetRegistry* PlaylistController::presetRegistry() const
{
    return p->presetRegistry;
}

Core::Library::SortingRegistry* PlaylistController::sortRegistry() const
{
    return p->sortRegistry;
}

Core::Playlist::PlaylistList PlaylistController::playlists() const
{
    return p->handler->playlists();
}

std::optional<Core::Playlist::Playlist> PlaylistController::currentPlaylist() const
{
    return p->handler->playlistById(p->currentPlaylistId);
}

void PlaylistController::changeCurrentPlaylist(const Core::Playlist::Playlist& playlist)
{
    p->currentPlaylistId = playlist.id();
    emit currentPlaylistChanged(playlist);
}

void PlaylistController::changeCurrentPlaylist(int id)
{
    if(auto playlist = p->handler->playlistById(id)) {
        changeCurrentPlaylist(*playlist);
    }
}

void PlaylistController::removePlaylistTracks(const Core::TrackList& tracks)
{
    auto playlist = currentPlaylist();
    if(!playlist) {
        return;
    }
    auto playlistTracks = playlist->tracks();
    std::erase_if(playlistTracks,
                  [&tracks](const Core::Track& track) { return std::ranges::find(tracks, track) != tracks.end(); });
    p->handler->replacePlaylistTracks(playlist->id(), playlistTracks);
}

void PlaylistController::refreshCurrentPlaylist()
{
    if(auto playlist = p->handler->playlistById(p->currentPlaylistId)) {
        emit refreshPlaylist(*playlist);
    }
}

void PlaylistController::startPlayback(const Core::Track& track) const
{
    p->handler->startPlayback(p->currentPlaylistId, track);
}
} // namespace Fy::Gui::Widgets::Playlist
