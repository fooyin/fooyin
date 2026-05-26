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

#include <gui/playlist/playlistviewrefresher.h>

#include "playlistviewrefreshsource.h"

namespace Fooyin {
class PlaylistViewRefresherImpl : public PlaylistViewRefresher
{
    Q_OBJECT

public:
    PlaylistViewRefresherImpl(PlaylistViewRefreshSource* source, QObject* parent = nullptr);

    void refreshPlaylist(const UId& playlistId) override;
    void refreshEntries(const UId& playlistId, std::span<const UId> entryIds) override;

private:
    PlaylistViewRefreshSource* m_source;
};
} // namespace Fooyin
