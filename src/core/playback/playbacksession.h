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

#include "fycore_export.h"

#include <core/player/playerdefs.h>

#include <optional>

namespace Fooyin {
class FYCORE_EXPORT PlaybackSession
{
public:
    PlaybackSession();

    [[nodiscard]] PlaylistTrack* currentTrackPtr();
    [[nodiscard]] PlaylistTrack* scheduledTrackPtr();
    [[nodiscard]] bool* isQueueTrackPtr();

    [[nodiscard]] const PlaylistTrack& currentTrack() const;
    [[nodiscard]] PlaylistTrack& currentTrack();
    [[nodiscard]] const PlaylistTrack& scheduledTrack() const;
    [[nodiscard]] const std::optional<PlaylistTrack>& detachedCurrentPlaylistTrack() const;
    [[nodiscard]] bool hasCurrentTrack() const;
    [[nodiscard]] bool isQueueTrack() const;
    [[nodiscard]] uint64_t currentItemId() const;
    [[nodiscard]] bool isIdle() const;

    [[nodiscard]] const Player::TrackChangeContext& pendingChangeContext() const;
    [[nodiscard]] Player::TrackChangeContext& pendingChangeContext();
    [[nodiscard]] const Player::TrackChangeContext& lastChangeContext() const;
    [[nodiscard]] bool hasPendingRequest() const;
    [[nodiscard]] bool pendingRequestMatchesCurrentTrack() const;
    [[nodiscard]] bool canAcceptRequest() const;

    [[nodiscard]] Player::TrackChangeRequest withPlaybackItemId(Player::TrackChangeRequest request,
                                                                uint64_t nextItemId) const;
    [[nodiscard]] Player::TrackChangeRequest requestTrackChange(Player::TrackChangeRequest request);

    struct CommitResult
    {
        bool isQueueTrack{false};
        bool matchedPendingRequest{false};
    };
    [[nodiscard]] CommitResult commitRequest(const Player::TrackChangeRequest& request);

    void clearPendingRequest();
    void clearCurrentTrack();
    void resetCurrentTrackState();
    void scheduleTrack(const PlaylistTrack& track);
    void setDetachedCurrentPlaylistTrack(const PlaylistTrack& track);
    void clearDetachedCurrentPlaylistTrack();

    [[nodiscard]] bool updateCurrentTrack(const Track& track);
    [[nodiscard]] bool updateCurrentTrackPlaylist(const UId& playlistId);
    [[nodiscard]] bool updateCurrentTrackEntry(const UId& entryId);
    [[nodiscard]] bool updateCurrentTrackIndex(int index);

private:
    PlaylistTrack m_currentTrack;
    PlaylistTrack m_scheduledTrack;
    bool m_isQueueTrack;
    uint64_t m_currentItemId;
    Player::TrackChangeContext m_pendingChangeContext;
    Player::TrackChangeContext m_lastChangeContext;
    std::optional<Player::TrackChangeRequest> m_pendingRequest;
    std::optional<PlaylistTrack> m_detachedCurrentPlaylistTrack;
};
} // namespace Fooyin
