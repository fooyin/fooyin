/*
 * Fooyin
 * Copyright © 2026, Piotr Wicijowski <piotr@wicijowski.pl>
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

#include "currentplaylistcontrollerimpl.h"

#include "playlistcontroller.h"

namespace Fooyin {
CurrentPlaylistController::CurrentPlaylistController(QObject* parent)
    : QObject{parent}
{ }

CurrentPlaylistController::~CurrentPlaylistController() = default;

CurrentPlaylistControllerImpl::CurrentPlaylistControllerImpl(PlaylistController* playlistController, QObject* parent)
    : CurrentPlaylistController{parent}
    , m_playlistController{playlistController}
{
    QObject::connect(m_playlistController, &PlaylistController::currentPlaylistChanged, this,
                     &CurrentPlaylistController::currentPlaylistChanged);
}

Playlist* CurrentPlaylistControllerImpl::currentPlaylist() const
{
    return m_playlistController ? m_playlistController->currentPlaylist() : nullptr;
}

UId CurrentPlaylistControllerImpl::currentPlaylistId() const
{
    return m_playlistController ? m_playlistController->currentPlaylistId() : UId{};
}

void CurrentPlaylistControllerImpl::changeCurrentPlaylist(const UId& id)
{
    if(m_playlistController) {
        m_playlistController->changeCurrentPlaylist(id);
    }
}
} // namespace Fooyin
