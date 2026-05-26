/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include <utils/id.h>

#include <span>

namespace Fooyin {
class PlaylistViewRefreshSource
{
public:
    virtual ~PlaylistViewRefreshSource() = default;

    virtual void refreshPlaylist(const UId& playlistId)                               = 0;
    virtual void refreshEntries(const UId& playlistId, std::span<const UId> entryIds) = 0;
};
} // namespace Fooyin
