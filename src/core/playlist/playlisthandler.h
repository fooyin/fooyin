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

#pragma once

#include "playlistmanager.h"

namespace Core::Player {
class PlayerManager;
}

namespace Core::Playlist {
class PlaylistHandler : public QObject,
                        PlaylistManager
{
    Q_OBJECT

public:
    explicit PlaylistHandler(Player::PlayerManager* playerManager, QObject* parent = nullptr);
    ~PlaylistHandler() override;

    Playlist* playlist(int id) override;

    int createPlaylist(const TrackPtrList& tracks, const QString& name) override;
    int createEmptyPlaylist() override;

    [[nodiscard]] int activeIndex() const override;
    Playlist* activePlaylist() override;
    [[nodiscard]] int currentIndex() const override;
    void setCurrentIndex(int playlistIndex) override;
    [[nodiscard]] int count() const override;

private:
    [[nodiscard]] int exists(const QString& name) const;
    int addNewPlaylist(const QString& name);

protected:
    void next();
    void previous();

private:
    QMap<int, Playlist*> m_playlists;
    Player::PlayerManager* m_playerManager;
    int m_currentPlaylistIndex;
};
} // namespace Core::Playlist
