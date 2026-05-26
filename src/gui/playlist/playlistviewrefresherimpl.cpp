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

#include "playlistviewrefresherimpl.h"

namespace Fooyin {
PlaylistViewRefresher::PlaylistViewRefresher(QObject* parent)
    : QObject{parent}
{ }

PlaylistViewRefresher::~PlaylistViewRefresher() = default;

PlaylistViewRefresherImpl::PlaylistViewRefresherImpl(PlaylistViewRefreshSource* source, QObject* parent)
    : PlaylistViewRefresher{parent}
    , m_source{source}
{ }

void PlaylistViewRefresherImpl::refreshPlaylist(const UId& playlistId)
{
    if(m_source) {
        m_source->refreshPlaylist(playlistId);
    }
}

void PlaylistViewRefresherImpl::refreshEntries(const UId& playlistId, std::span<const UId> entryIds)
{
    if(m_source) {
        m_source->refreshEntries(playlistId, entryIds);
    }
}
} // namespace Fooyin
