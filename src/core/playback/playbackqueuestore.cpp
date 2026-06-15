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

#include "playbackqueuestore.h"

#include <core/library/musiclibrary.h>
#include <core/playlist/playlisthandler.h>
#include <utils/database/dbconnectionprovider.h>

#include <optional>

namespace Fooyin {
namespace {
std::optional<PlaylistTrack> findPlaylistTrack(const Playlist& playlist, int trackId, int index)
{
    if(index >= 0) {
        if(const auto playlistTrack = playlist.playlistTrack(index);
           playlistTrack && playlistTrack->track.id() == trackId) {
            return playlistTrack;
        }
    }

    std::optional<PlaylistTrack> matchingTrack;

    const auto playlistTracks = playlist.playlistTracks();
    for(const PlaylistTrack& playlistTrack : playlistTracks) {
        if(playlistTrack.track.id() != trackId) {
            continue;
        }

        // If there are multiple matching tracks, leave entry detached rather than picking one arbitrarily
        if(matchingTrack.has_value()) {
            return {};
        }

        matchingTrack = playlistTrack;
    }

    return matchingTrack;
}
} // namespace

PlaybackQueueStore::PlaybackQueueStore(DbConnectionPoolPtr dbPool, MusicLibrary* library,
                                       PlaylistHandler* playlistHandler)
    : m_library{library}
    , m_playlistHandler{playlistHandler}
{
    const DbConnectionProvider dbProvider{std::move(dbPool)};
    m_database.initialise(dbProvider);
}

void PlaybackQueueStore::save(const PlaybackQueue& queue) const
{
    const auto& queueTracks = queue.tracks();

    std::vector<PlaybackQueueInfo> items;
    items.reserve(queueTracks.size());

    for(const auto& track : queueTracks) {
        if(!track.track.isValid() || !track.track.isInDatabase()) {
            continue;
        }

        PlaybackQueueInfo item;
        item.trackId = track.track.id();

        if(track.playlistId.isValid()) {
            if(auto* playlist = m_playlistHandler->playlistById(track.playlistId);
               playlist && !playlist->isTemporary()) {
                item.playlistDbId       = playlist->dbId();
                item.playlistTrackIndex = track.indexInPlaylist;
            }
        }

        items.emplace_back(item);
    }

    m_database.replaceQueue(items);
}

QueueTracks PlaybackQueueStore::load() const
{
    const auto savedQueue = m_database.queue();

    QueueTracks queue;
    queue.reserve(savedQueue.size());

    for(const auto& item : savedQueue) {
        const Track track = m_library->trackForId(item.trackId);
        if(!track.isValid()) {
            continue;
        }

        PlaylistTrack queueTrack{.track = track, .playlistId = {}, .entryId = {}, .indexInPlaylist = -1};

        if(item.playlistDbId >= 0 && item.playlistTrackIndex >= 0) {
            if(auto* playlist = m_playlistHandler->playlistByDbId(item.playlistDbId)) {
                if(const auto playlistTrack = findPlaylistTrack(*playlist, item.trackId, item.playlistTrackIndex)) {
                    queueTrack.playlistId      = playlist->id();
                    queueTrack.entryId         = playlistTrack->entryId;
                    queueTrack.indexInPlaylist = playlistTrack->indexInPlaylist;
                }
            }
        }

        queue.emplace_back(queueTrack);
    }

    return queue;
}
} // namespace Fooyin
