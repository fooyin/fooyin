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

#pragma once

#include <core/playlist/playlist.h>
#include <core/track.h>

#include <QObject>

class QMenu;

namespace Fooyin {
class PlaylistController;

class PlaylistUiController : public QObject
{
    Q_OBJECT

public:
    explicit PlaylistUiController(PlaylistController* playlistController, QObject* parent = nullptr);

    void addPlaylistMenu(QMenu* menu) const;
    void showNowPlaying();
    void selectTrackIds(const TrackIds& ids);
    void focusPlaylist();
    void filterCurrentPlaylist(const PlaylistTrackList& tracks);

signals:
    void showCurrentTrack();
    void selectTracks(const Fooyin::TrackIds& ids);
    void filterTracks(const Fooyin::PlaylistTrackList& tracks);
    void requestPlaylistFocus();

private:
    PlaylistController* m_playlistController;
};
} // namespace Fooyin
