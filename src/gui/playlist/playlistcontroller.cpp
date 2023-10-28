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

    Core::Playlist::Playlist* currentPlaylist{nullptr};

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
            if(auto* playlist = handler->playlistById(lastId)) {
                currentPlaylist = playlist;
                QMetaObject::invokeMethod(self, "currentPlaylistChanged", Q_ARG(Core::Playlist::Playlist*, playlist));
            }
        }
    }

    void handlePlaylistUpdated(Core::Playlist::Playlist* playlist) const
    {
        if(currentPlaylist == playlist) {
            self->changeCurrentPlaylist(playlist);
        }
    }

    void handlePlaylistRemoved(const Core::Playlist::Playlist* playlist) const
    {
        if(currentPlaylist == playlist) {
            const int nextIndex = playlist->index() <= 1 ? 0 : playlist->index() - 1;
            if(auto* nextPlaylist = handler->playlistByIndex(nextIndex)) {
                self->changeCurrentPlaylist(nextPlaylist);
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
                     [this](Core::Playlist::Playlist* playlist) { p->handlePlaylistUpdated(playlist); });
    QObject::connect(handler, &Core::Playlist::PlaylistManager::playlistRemoved, this,
                     [this](const Core::Playlist::Playlist* playlist) { p->handlePlaylistRemoved(playlist); });
    QObject::connect(handler, &Core::Playlist::PlaylistManager::playlistRenamed, this,
                     [this](Core::Playlist::Playlist* playlist) { p->handlePlaylistUpdated(playlist); });
}

PlaylistController::~PlaylistController()
{
    if(p->currentPlaylist) {
        p->settings->set<Settings::LastPlaylistId>(p->currentPlaylist->id());
    }
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

const Core::Playlist::PlaylistList& PlaylistController::playlists() const
{
    return p->handler->playlists();
}

Core::Playlist::Playlist* PlaylistController::currentPlaylist() const
{
    return p->currentPlaylist;
}

void PlaylistController::changeCurrentPlaylist(Core::Playlist::Playlist* playlist)
{
    p->currentPlaylist = playlist;
    emit currentPlaylistChanged(playlist);
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
} // namespace Fy::Gui::Widgets::Playlist
