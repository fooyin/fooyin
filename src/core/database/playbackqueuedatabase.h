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

#include <utils/database/dbmodule.h>

#include <vector>

namespace Fooyin {
struct PlaybackQueueInfo
{
    int trackId{-1};
    int playlistDbId{-1};
    int playlistTrackIndex{-1};
};

class PlaybackQueueDatabase : public DbModule
{
public:
    [[nodiscard]] std::vector<PlaybackQueueInfo> queue() const;
    bool replaceQueue(const std::vector<PlaybackQueueInfo>& queue) const;
    bool clearQueue() const;
};
} // namespace Fooyin
