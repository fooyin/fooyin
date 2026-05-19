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

#include "playbacksession.h"

#include <utility>

namespace Fooyin {
PlaybackSession::PlaybackSession()
    : m_isQueueTrack{false}
    , m_currentItemId{0}
{ }

PlaylistTrack* PlaybackSession::currentTrackPtr()
{
    return &m_currentTrack;
}

PlaylistTrack* PlaybackSession::scheduledTrackPtr()
{
    return &m_scheduledTrack;
}

bool* PlaybackSession::isQueueTrackPtr()
{
    return &m_isQueueTrack;
}

const PlaylistTrack& PlaybackSession::currentTrack() const
{
    return m_currentTrack;
}

PlaylistTrack& PlaybackSession::currentTrack()
{
    return m_currentTrack;
}

const PlaylistTrack& PlaybackSession::scheduledTrack() const
{
    return m_scheduledTrack;
}

const std::optional<PlaylistTrack>& PlaybackSession::detachedCurrentPlaylistTrack() const
{
    return m_detachedCurrentPlaylistTrack;
}

bool PlaybackSession::hasCurrentTrack() const
{
    return m_currentTrack.isValid();
}

bool PlaybackSession::isQueueTrack() const
{
    return m_isQueueTrack;
}

uint64_t PlaybackSession::currentItemId() const
{
    return m_currentItemId;
}

bool PlaybackSession::isIdle() const
{
    return !hasCurrentTrack() && !hasPendingRequest();
}

const Player::TrackChangeContext& PlaybackSession::pendingChangeContext() const
{
    return m_pendingChangeContext;
}

Player::TrackChangeContext& PlaybackSession::pendingChangeContext()
{
    return m_pendingChangeContext;
}

const Player::TrackChangeContext& PlaybackSession::lastChangeContext() const
{
    return m_lastChangeContext;
}

bool PlaybackSession::hasPendingRequest() const
{
    return m_pendingRequest.has_value();
}

bool PlaybackSession::pendingRequestMatchesCurrentTrack() const
{
    return m_pendingRequest.has_value() && m_currentTrack.isValid()
        && m_pendingRequest->track.track.sameIdentityAs(m_currentTrack.track);
}

bool PlaybackSession::canAcceptRequest() const
{
    return !hasPendingRequest();
}

Player::TrackChangeRequest PlaybackSession::withPlaybackItemId(Player::TrackChangeRequest request,
                                                               uint64_t nextItemId) const
{
    if(request.itemId == 0 && request.track.isValid()) {
        request.itemId = nextItemId;
    }
    return request;
}

Player::TrackChangeRequest PlaybackSession::requestTrackChange(Player::TrackChangeRequest request)
{
    m_pendingRequest       = request;
    m_pendingChangeContext = request.context;
    return request;
}

PlaybackSession::CommitResult PlaybackSession::commitRequest(const Player::TrackChangeRequest& request)
{
    Player::TrackChangeContext context{request.context};

    CommitResult result{
        .isQueueTrack          = request.isQueueTrack,
        .matchedPendingRequest = false,
    };

    uint64_t itemId = request.itemId;

    if(m_pendingRequest.has_value() && m_pendingRequest->track == request.track
       && m_pendingRequest->itemId == request.itemId) {
        context                      = m_pendingRequest->context;
        result.isQueueTrack          = m_pendingRequest->isQueueTrack;
        result.matchedPendingRequest = true;
        itemId                       = m_pendingRequest->itemId;
    }

    m_currentTrack         = request.track;
    m_lastChangeContext    = context;
    m_pendingChangeContext = {};
    m_isQueueTrack         = result.isQueueTrack;
    m_currentItemId        = itemId;
    m_scheduledTrack       = {};
    m_pendingRequest.reset();
    m_detachedCurrentPlaylistTrack.reset();

    return result;
}

void PlaybackSession::clearPendingRequest()
{
    m_pendingRequest.reset();
    m_pendingChangeContext = {};
}

void PlaybackSession::clearCurrentTrack()
{
    m_currentTrack = {};
    m_detachedCurrentPlaylistTrack.reset();
}

void PlaybackSession::resetCurrentTrackState()
{
    m_currentTrack  = {};
    m_currentItemId = 0;
    m_detachedCurrentPlaylistTrack.reset();
}

void PlaybackSession::scheduleTrack(const PlaylistTrack& track)
{
    m_scheduledTrack = track;
}

void PlaybackSession::setDetachedCurrentPlaylistTrack(const PlaylistTrack& track)
{
    m_detachedCurrentPlaylistTrack = track;
}

void PlaybackSession::clearDetachedCurrentPlaylistTrack()
{
    m_detachedCurrentPlaylistTrack.reset();
}

bool PlaybackSession::updateCurrentTrack(const Track& track)
{
    if(!track.sameIdentityAs(m_currentTrack.track) || track.sameDataAs(m_currentTrack.track)) {
        return false;
    }

    m_currentTrack.track = track;
    return true;
}

bool PlaybackSession::updateCurrentTrackPlaylist(const UId& playlistId)
{
    return std::exchange(m_currentTrack.playlistId, playlistId) != playlistId;
}

bool PlaybackSession::updateCurrentTrackEntry(const UId& entryId)
{
    return std::exchange(m_currentTrack.entryId, entryId) != entryId;
}

bool PlaybackSession::updateCurrentTrackIndex(int index)
{
    return std::exchange(m_currentTrack.indexInPlaylist, index) != index;
}
} // namespace Fooyin
