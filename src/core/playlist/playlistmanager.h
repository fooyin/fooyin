/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

namespace Core::Playlist {
class Playlist;
class PlaylistManager
{
public:
    virtual ~PlaylistManager() = default;

    virtual Playlist* playlist(int id) = 0;

    [[nodiscard]] virtual int activeIndex() const = 0;
    virtual Playlist* activePlaylist()            = 0;

    [[nodiscard]] virtual int currentIndex() const  = 0;
    virtual void setCurrentIndex(int playlistIndex) = 0;

    [[nodiscard]] virtual int count() const = 0;

    virtual int createPlaylist(const TrackPtrList& tracks, const QString& name) = 0;
    virtual int createEmptyPlaylist()                                           = 0;
};
} // namespace Core::Playlist
