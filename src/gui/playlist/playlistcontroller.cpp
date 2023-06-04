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

#include "gui/guisettings.h"

#include <core/playlist/playlisthandler.h>

#include <utils/settings/settingsmanager.h>

namespace Fy::Gui::Widgets::Playlist {
struct PlaylistController::Private : QObject
{
    PlaylistController* controller;

    Core::Playlist::PlaylistHandler* handler;
    Utils::SettingsManager* settings;

    Core::Playlist::Playlist* currentPlaylist{nullptr};

    Private(PlaylistController* controller, Core::Playlist::PlaylistHandler* handler, Utils::SettingsManager* settings)
        : controller{controller}
        , handler{handler}
        , settings{settings}
    {
        connect(handler, &Core::Playlist::PlaylistHandler::playlistsPopulated, this,
                &PlaylistController::Private::restoreLastPlaylist);
    }

    void restoreLastPlaylist()
    {
        const int lastId = settings->value<Settings::LastPlaylistId>();
        if(lastId >= 0) {
            auto* playlist = handler->playlistById(lastId);
            if(playlist) {
                currentPlaylist = playlist;
                emit controller->currentPlaylistChanged(currentPlaylist);
            }
        }
    }
};

PlaylistController::PlaylistController(Core::Playlist::PlaylistHandler* handler, Utils::SettingsManager* settings,
                                       QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, handler, settings)}
{ }

PlaylistController::~PlaylistController()
{
    if(p->currentPlaylist) {
        p->settings->set<Settings::LastPlaylistId>(p->currentPlaylist->id());
    }
}

const Core::Playlist::PlaylistList &PlaylistController::playlists() const
{
return p->handler->playlists();
}

Core::Playlist::Playlist* PlaylistController::currentPlaylist() const
{
    return p->currentPlaylist;
}

void PlaylistController::changeCurrentPlaylist(Core::Playlist::Playlist* playlist)
{
    if(p->currentPlaylist == playlist) {
        return;
    }
    p->currentPlaylist = playlist;
    emit currentPlaylistChanged(p->currentPlaylist);
}

void PlaylistController::changeCurrentPlaylist(int id)
{
    if(auto* playlist = p->handler->playlistById(id)) {
        changeCurrentPlaylist(playlist);
    }
}

void PlaylistController::refreshCurrentPlaylist()
{
    emit refreshPlaylist(p->currentPlaylist);
}

void PlaylistController::startPlayback(const Core::Track& track) const
{
    p->handler->startPlayback(p->currentPlaylist->id(), track);
}
} // namespace Fy::Gui::Widgets::Playlist
