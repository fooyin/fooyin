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

    int currentPlaylistId{-1};

    Private(PlaylistController* controller, Core::Playlist::PlaylistHandler* handler, Utils::SettingsManager* settings)
        : controller{controller}
        , handler{handler}
        , settings{settings}
    {
        connect(handler, &Core::Playlist::PlaylistHandler::playlistsPopulated, this,
                &PlaylistController::Private::restoreLastPlaylist);
        connect(handler, &Core::Playlist::PlaylistHandler::playlistTracksChanged, this,
                &PlaylistController::Private::handlePlaylistUpdated);
        QObject::connect(handler, &Core::Playlist::PlaylistHandler::playlistAdded, this,
                         &PlaylistController::Private::handlePlaylistAdded);
        QObject::connect(handler, &Core::Playlist::PlaylistHandler::playlistRemoved, this,
                         &PlaylistController::Private::handlePlaylistRemoved);
        QObject::connect(handler, &Core::Playlist::PlaylistHandler::playlistRenamed, controller,
                         &PlaylistController::refreshCurrentPlaylist);
    }

    void restoreLastPlaylist()
    {
        const int lastId = settings->value<Settings::LastPlaylistId>();
        if(lastId >= 0) {
            auto playlist = handler->playlistById(lastId);
            if(playlist) {
                currentPlaylistId = playlist->id();
                emit controller->currentPlaylistChanged(*playlist);
            }
        }
    }

    void handlePlaylistAdded(const Core::Playlist::Playlist& playlist, bool switchTo)
    {
        if(switchTo) {
            controller->changeCurrentPlaylist(playlist);
        }
    }

    void handlePlaylistUpdated(const Core::Playlist::Playlist& playlist, bool switchTo)
    {
        if(currentPlaylistId == playlist.id() || switchTo) {
            controller->changeCurrentPlaylist(playlist);
        }
    }

    void handlePlaylistRemoved(int id)
    {
        if(currentPlaylistId == id) {
            emit controller->currentPlaylistChanged(handler->playlists().back());
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
    p->settings->set<Settings::LastPlaylistId>(p->currentPlaylistId);
}

Core::Playlist::PlaylistHandler* PlaylistController::playlistHandler() const
{
    return p->handler;
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
