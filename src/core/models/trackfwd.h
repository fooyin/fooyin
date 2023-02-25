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

#include "album.h"
#include "artist.h"
#include "track.h"

#include <deque>

namespace Fy::Core {
using TrackList         = std::vector<Track>;
using TrackPtrList      = std::vector<Track*>;
using TrackSet          = std::set<Track*>;
using TrackIdUniqPtrMap = std::unordered_map<int, std::unique_ptr<Track>>;
using TrackPathMap      = std::unordered_map<QString, Track*>;

using AlbumList  = std::vector<Album>;
using ArtistHash = std::unordered_map<int, Artist>;
using GenreHash  = std::unordered_map<int, QString>;

using ContainerHash = std::unordered_map<QString, std::unique_ptr<Container>>;
} // namespace Fy::Core
