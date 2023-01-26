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

#include "core/models/trackfwd.h"

#include <QObject>

namespace Core {
namespace Player {
class PlayerManager;
}

namespace Playlist {
class Playlist : public QObject
{
    Q_OBJECT

public:
    Playlist(Player::PlayerManager* playerManager, int idx, QString name);
    ~Playlist() override = default;

    QString name();

    int createPlaylist(const TrackPtrList& tracks);

    [[nodiscard]] int currentTrackIndex() const;
    [[nodiscard]] Track* currentTrack() const;

    [[nodiscard]] int index() const;

    void insertTracks(const TrackPtrList& tracks);
    void appendTracks(const TrackPtrList& tracks);

    void clear();

    void setCurrentTrack(int index);
    bool changeTrack(int index);

    void play();
    void stop();
    int next();
    int previous();

protected:
    int nextIndex();
    int numberOfTracks() const;

private:
    Player::PlayerManager* m_playerManager;

    QString m_name;
    int m_playlistIndex;
    Track* m_playingTrack;
    TrackPtrList m_tracks;
};
} // namespace Playlist
} // namespace Core
