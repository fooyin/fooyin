/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <gui/fywidget.h>

class QComboBox;

namespace Fooyin {
class Playlist;
class PlaylistController;
class PlaylistHandler;

class PlaylistBox : public FyWidget
{
    Q_OBJECT

public:
    explicit PlaylistBox(PlaylistController* playlistController, QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

    void setupBox();

    void addPlaylist(const Playlist* playlist);
    void removePlaylist(const Playlist* playlist);
    void playlistRenamed(const Playlist* playlist) const;
    void currentPlaylistChanged(const Playlist* playlist) const;
    void changePlaylist(int index);

private:
    PlaylistController* m_playlistController;
    PlaylistHandler* m_playlistHandler;

    QComboBox* m_playlistBox;
};
} // namespace Fooyin
