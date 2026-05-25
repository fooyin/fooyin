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

#include "playlistselectionobserverimpl.h"

#include "playlistcontroller.h"

namespace Fooyin {
PlaylistSelectionObserver::PlaylistSelectionObserver(QObject* parent)
    : QObject{parent}
{ }

PlaylistSelectionObserver::~PlaylistSelectionObserver() = default;

PlaylistSelectionObserverImpl::PlaylistSelectionObserverImpl(PlaylistController* playlistController, QObject* parent)
    : PlaylistSelectionObserver{parent}
    , m_playlistController{playlistController}
{
    QObject::connect(m_playlistController, &PlaylistController::currentPlaylistChanged, this,
                     &PlaylistSelectionObserver::currentPlaylistChanged);
}

Playlist* PlaylistSelectionObserverImpl::currentPlaylist() const
{
    return m_playlistController ? m_playlistController->currentPlaylist() : nullptr;
}

UId PlaylistSelectionObserverImpl::currentPlaylistId() const
{
    return m_playlistController ? m_playlistController->currentPlaylistId() : UId{};
}
} // namespace Fooyin
