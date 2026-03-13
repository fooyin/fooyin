/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
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

#include "playlistuicontroller.h"

#include "playlistcontroller.h"

#include <QAction>
#include <QMenu>

namespace Fooyin {
PlaylistUiController::PlaylistUiController(PlaylistController* playlistController, QObject* parent)
    : QObject{parent}
    , m_playlistController{playlistController}
{ }

void PlaylistUiController::addPlaylistMenu(QMenu* menu) const
{
    if(!menu || !m_playlistController) {
        return;
    }

    auto* currentPlaylist = m_playlistController->currentPlaylist();
    if(!currentPlaylist) {
        return;
    }

    auto* playlistMenu = new QMenu(tr("Playlists"), menu);

    for(const auto& playlist : m_playlistController->playlists()) {
        if(playlist != currentPlaylist) {
            auto* switchPlaylist = new QAction(playlist->name(), playlistMenu);
            const UId id         = playlist->id();
            QObject::connect(switchPlaylist, &QAction::triggered, playlistMenu,
                             [controller = m_playlistController, id]() { controller->changeCurrentPlaylist(id); });
            playlistMenu->addAction(switchPlaylist);
        }
    }

    menu->addMenu(playlistMenu);
}

void PlaylistUiController::showNowPlaying()
{
    emit showCurrentTrack();
}

void PlaylistUiController::selectTrackIds(const TrackIds& ids)
{
    emit selectTracks(ids);
}

void PlaylistUiController::focusPlaylist()
{
    emit requestPlaylistFocus();
}

void PlaylistUiController::filterCurrentPlaylist(const PlaylistTrackList& tracks)
{
    if(m_playlistController && m_playlistController->currentPlaylist()) {
        emit filterTracks(tracks);
    }
}
} // namespace Fooyin
