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

#include <core/playlist/playlist.h>

#include <optional>

namespace Fooyin {
class PlaybackQueue;
class PlaylistHandler;
class SettingsManager;

class PlaybackOrderNavigator
{
public:
    struct PlaybackOrderState
    {
        PlaylistTrack* currentTrack{nullptr};
        Playlist::PlayModes* playMode{nullptr};
        bool* isQueueTrack{nullptr};
        PlaylistTrack* scheduledTrack{nullptr};
    };

    enum class RestartTarget : uint8_t
    {
        None = 0,
        Beginning,
        End,
    };

    struct RequestedTrack
    {
        PlaylistTrack track;
        bool isQueueTrack{false};
    };

    PlaybackOrderNavigator(SettingsManager* settings, PlaylistHandler* playlistHandler, PlaybackQueue* queue,
                           PlaybackOrderState state);

    [[nodiscard]] Playlist* playbackPlaylist() const;
    [[nodiscard]] PlaylistTrack previewPlaybackRelativeTrack(int delta) const;
    std::optional<RequestedTrack> selectScheduledTrack();
    std::optional<RequestedTrack> selectPlaybackOrderTrack(int delta);
    std::optional<RequestedTrack> selectPlayFromIdleState(bool preferScheduledTrack);
    PlaylistTrack restartPlaylist(RestartTarget target);

private:
    PlaylistTrack advancePlaybackRelativeTrack(int delta);
    [[nodiscard]] Playlist* playlistForTrack(const PlaylistTrack& track) const;
    PlaylistTrack restartPlaylistFromBeginning();
    PlaylistTrack restartPlaylistFromEnd();

    void activatePlaylistTrack(const PlaylistTrack& track);
    [[nodiscard]] std::optional<PlaylistTrack> followQueuedTrackIndex(int delta) const;

    SettingsManager* m_settings;
    PlaylistHandler* m_playlistHandler;
    PlaybackQueue* m_queue;
    PlaybackOrderState m_state;
};
} // namespace Fooyin
