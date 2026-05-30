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

#include <gui/playlist/currentplaylistcontroller.h>

#include <core/playlist/playlist.h>

namespace Fooyin {
CurrentPlaylistController::CurrentPlaylistController(QObject* parent)
    : QObject{parent}
{ }

CurrentPlaylistController::~CurrentPlaylistController() = default;

Playlist* CurrentPlaylistController::currentPlaylist() const
{
    return m_currentPlaylist ? m_currentPlaylist() : nullptr;
}

UId CurrentPlaylistController::currentPlaylistId() const
{
    if(const auto* playlist = currentPlaylist()) {
        return playlist->id();
    }

    return {};
}

void CurrentPlaylistController::changeCurrentPlaylist(const UId& id)
{
    if(m_changeCurrentPlaylist) {
        m_changeCurrentPlaylist(id);
    }
}

void CurrentPlaylistController::setCurrentPlaylistProvider(std::function<Playlist*()> provider)
{
    m_currentPlaylist = std::move(provider);
}

void CurrentPlaylistController::handleCurrentPlaylistChanged(Playlist* previous, Playlist* current)
{
    Q_EMIT currentPlaylistChanged(previous, current);
}

void CurrentPlaylistController::setChangeCurrentPlaylistHandler(std::function<void(const UId& id)> handler)
{
    m_changeCurrentPlaylist = std::move(handler);
}
} // namespace Fooyin
