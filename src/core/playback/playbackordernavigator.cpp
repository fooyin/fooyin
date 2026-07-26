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

#include "playbackordernavigator.h"

#include "core/playlist/playlisthandler.h"

#include <core/player/playbackqueue.h>

#include <core/coresettings.h>
#include <core/track.h>
#include <utils/settings/settingsmanager.h>

#include <algorithm>

namespace Fooyin {
PlaybackOrderNavigator::PlaybackOrderNavigator(SettingsManager* settings, PlaylistHandler* playlistHandler,
                                               PlaybackQueue* queue, PlaybackOrderState state)
    : m_settings{settings}
    , m_playlistHandler{playlistHandler}
    , m_queue{queue}
    , m_state{state}
{ }

Playlist* PlaybackOrderNavigator::playbackPlaylist() const
{
    if(*m_state.isQueueTrack) {
        return nullptr;
    }

    return playlistForTrack(*m_state.currentTrack);
}

Playlist* PlaybackOrderNavigator::playlistForTrack(const PlaylistTrack& track) const
{
    if(!m_playlistHandler || !track.isValid() || !track.playlistId.isValid()) {
        return nullptr;
    }

    auto* playlist = m_playlistHandler->playlistById(track.playlistId);
    if(!playlist) {
        return nullptr;
    }

    if(track.entryId.isValid()) {
        if(const auto playlistTrack = playlist->playlistTrack(track.entryId);
           playlistTrack && playlistTrack->track.sameIdentityAs(track.track)) {
            return playlist;
        }
    }

    if(track.indexInPlaylist >= 0 && track.indexInPlaylist < playlist->trackCount()) {
        const auto playlistTrack = playlist->playlistTrack(track.indexInPlaylist);
        if(playlistTrack.has_value() && playlistTrack->track.sameIdentityAs(track.track)) {
            return playlist;
        }
    }

    return nullptr;
}

PlaylistTrack PlaybackOrderNavigator::previewPlaybackRelativeTrack(int delta) const
{
    if(m_settings->value<Settings::Core::FollowPlaybackQueue>() && *m_state.isQueueTrack) {
        if(const auto followedTrack = followQueuedTrackIndex(delta)) {
            return *followedTrack;
        }
    }

    if(auto* playlist = playbackPlaylist()) {
        const int nextIndex = playlist->nextIndexFrom(m_state.currentTrack->indexInPlaylist, delta, *m_state.playMode);
        return playlist->playlistTrack(nextIndex).value_or(PlaylistTrack{});
    }

    if(!m_playlistHandler) {
        return {};
    }

    return m_playlistHandler->peekRelativeTrack(*m_state.playMode, delta);
}

PlaylistTrack PlaybackOrderNavigator::advancePlaybackRelativeTrack(int delta)
{
    if(auto* playlist = playbackPlaylist()) {
        const Track track
            = playlist->nextTrackChangeFrom(m_state.currentTrack->indexInPlaylist, delta, *m_state.playMode);
        if(!track.isValid()) {
            return {};
        }

        return playlist->playlistTrack(playlist->currentTrackIndex()).value_or(PlaylistTrack{});
    }

    if(!m_playlistHandler) {
        return {};
    }

    return m_playlistHandler->advanceRelativeTrack(*m_state.playMode, delta);
}

PlaylistTrack PlaybackOrderNavigator::restartPlaylistFromBeginning()
{
    Playlist* playlist{playbackPlaylist()};

    if(!playlist && m_playlistHandler) {
        playlist = m_playlistHandler->activePlaylist();
    }

    if(!playlist || playlist->trackCount() <= 0) {
        return {};
    }

    if(m_playlistHandler && m_playlistHandler->activePlaylist() != playlist) {
        m_playlistHandler->changeActivePlaylist(playlist);
    }

    const int restartIndex = playlist->firstIndex(*m_state.playMode);
    const int startIndex   = restartIndex >= 0 ? restartIndex : 0;

    playlist->changeCurrentIndex(startIndex);
    return playlist->playlistTrack(startIndex).value_or(PlaylistTrack{});
}

PlaylistTrack PlaybackOrderNavigator::restartPlaylistFromEnd()
{
    Playlist* playlist{playbackPlaylist()};

    if(!playlist && m_playlistHandler) {
        playlist = m_playlistHandler->activePlaylist();
    }

    if(!playlist || playlist->trackCount() <= 0) {
        return {};
    }

    if(m_playlistHandler && m_playlistHandler->activePlaylist() != playlist) {
        m_playlistHandler->changeActivePlaylist(playlist);
    }

    const int restartIndex = playlist->lastIndex(*m_state.playMode);
    const int endIndex     = restartIndex >= 0 ? restartIndex : std::max(playlist->trackCount() - 1, 0);

    playlist->changeCurrentIndex(endIndex);
    return playlist->playlistTrack(endIndex).value_or(PlaylistTrack{});
}

PlaylistTrack PlaybackOrderNavigator::restartPlaylist(RestartTarget target)
{
    switch(target) {
        case RestartTarget::Beginning:
            return restartPlaylistFromBeginning();
        case RestartTarget::End:
            return restartPlaylistFromEnd();
        case RestartTarget::None:
        default:
            return {};
    }
}

void PlaybackOrderNavigator::activatePlaylistTrack(const PlaylistTrack& track)
{
    if(!m_playlistHandler || !track.playlistId.isValid()) {
        return;
    }

    if(auto* playlist = m_playlistHandler->playlistById(track.playlistId)) {
        if(m_playlistHandler->activePlaylist() != playlist) {
            m_playlistHandler->changeActivePlaylist(playlist);
        }

        int trackIndex{track.indexInPlaylist};
        if(track.entryId.isValid()) {
            trackIndex = playlist->indexOfTrackEntry(track.entryId);
        }

        if(trackIndex >= 0) {
            playlist->changeCurrentIndex(trackIndex);
        }
    }
}

std::optional<PlaylistTrack> PlaybackOrderNavigator::followQueuedTrackIndex(int delta) const
{
    if(auto* playlist = playlistForTrack(*m_state.currentTrack)) {
        const int nextIndex = playlist->nextIndexFrom(m_state.currentTrack->indexInPlaylist, delta, *m_state.playMode);
        return playlist->playlistTrack(nextIndex).value_or(PlaylistTrack{});
    }

    return {};
}

std::optional<PlaybackOrderNavigator::RequestedTrack> PlaybackOrderNavigator::selectScheduledTrack()
{
    if(!m_state.scheduledTrack->isValid()) {
        return {};
    }

    activatePlaylistTrack(*m_state.scheduledTrack);
    return RequestedTrack{
        .track        = *m_state.scheduledTrack,
        .isQueueTrack = false,
    };
}

std::optional<PlaybackOrderNavigator::RequestedTrack> PlaybackOrderNavigator::selectPlaybackOrderTrack(int delta)
{
    const bool followQueue = m_settings->value<Settings::Core::FollowPlaybackQueue>() && *m_state.isQueueTrack;

    if(delta <= 0) {
        if(followQueue) {
            if(const auto followedTrack = followQueuedTrackIndex(delta)) {
                activatePlaylistTrack(*followedTrack);
                return RequestedTrack{
                    .track        = *followedTrack,
                    .isQueueTrack = false,
                };
            }
        }

        if(m_playlistHandler) {
            auto track = advancePlaybackRelativeTrack(delta);
            return RequestedTrack{
                .track        = track,
                .isQueueTrack = m_queue->containsTrack(track),
            };
        }
        return {};
    }

    if(m_queue->empty() && m_playlistHandler) {
        if(followQueue) {
            if(const auto followedTrack = followQueuedTrackIndex(delta)) {
                activatePlaylistTrack(*followedTrack);
                return RequestedTrack{
                    .track        = *followedTrack,
                    .isQueueTrack = false,
                };
            }
        }

        return RequestedTrack{
            .track        = advancePlaybackRelativeTrack(delta),
            .isQueueTrack = false,
        };
    }

    if(!m_queue->empty()) {
        return RequestedTrack{
            .track        = m_queue->nextTrack(),
            .isQueueTrack = true,
        };
    }

    return {};
}

std::optional<PlaybackOrderNavigator::RequestedTrack>
PlaybackOrderNavigator::selectPlayFromIdleState(bool preferScheduledTrack)
{
    if(preferScheduledTrack) {
        if(auto scheduledTrack = selectScheduledTrack()) {
            return scheduledTrack;
        }
    }

    if(!m_queue->empty()) {
        return RequestedTrack{
            .track        = m_queue->nextTrack(),
            .isQueueTrack = true,
        };
    }

    if(!preferScheduledTrack) {
        if(auto scheduledTrack = selectScheduledTrack()) {
            return scheduledTrack;
        }
    }

    if(m_playlistHandler) {
        if(const auto currentTrack = m_playlistHandler->currentTrack(); currentTrack.isValid()) {
            return RequestedTrack{
                .track        = currentTrack,
                .isQueueTrack = false,
            };
        }

        return RequestedTrack{
            .track        = m_playlistHandler->advanceRelativeTrack(*m_state.playMode, 1),
            .isQueueTrack = false,
        };
    }

    return {};
}
} // namespace Fooyin
