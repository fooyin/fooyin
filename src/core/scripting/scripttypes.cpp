/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include <core/scripting/scripttypes.h>

#include <core/player/playbackqueue.h>
#include <core/player/playercontroller.h>

namespace Fooyin {
ScriptContextData makePlaybackScriptData(PlayerController* playerController, Playlist* playlist)
{
    ScriptContextData context;
    context.playlist = playlist;
    context.tracks   = playlist ? &playlist->tracks() : nullptr;

    int playlistTrackIndex{-1};
    uint64_t currentPosition{0};
    uint64_t currentTrackDuration{0};
    int bitrate{0};
    QString decoder;
    Engine::PlaybackOutputInfo outputInfo;
    auto playState{Player::PlayState::Stopped};
    auto queueMode{PlaybackQueueMode::PlaylistWithOverrides};

    if(playerController) {
        playlistTrackIndex   = playerController->currentPlaylistTrack().indexInPlaylist;
        currentPosition      = playerController->currentPosition();
        currentTrackDuration = playerController->currentTrack().duration();
        bitrate              = playerController->bitrate();
        decoder              = playerController->decoder();
        outputInfo           = playerController->playbackOutputInfo();
        playState            = playerController->playState();
        queueMode            = playerController->playbackQueueMode();
    }

    const auto playingTrack = playerController ? playerController->currentPlaylistTrack() : PlaylistTrack{};
    const bool samePlaylist = playlist && playingTrack.playlistId == playlist->id();
    context.playlistData    = ScriptPlaylistData{
        .playlistId        = playlist ? playlist->id() : UId{},
        .entryId           = samePlaylist ? playingTrack.entryId : UId{},
        .name              = playlist ? playlist->name() : QString{},
        .trackCount        = playlist ? playlist->trackCount() : 0,
        .trackIndex        = samePlaylist ? playlistTrackIndex : -1,
        .playingTrackIndex = samePlaylist ? playlistTrackIndex : -1,
        .playingTrackId    = playerController ? playerController->currentTrackId() : -1,
        .queueIndexes      = playerController && samePlaylist && playlistTrackIndex >= 0
                               ? playerController->playbackQueue().indexesForTrack(playlist->id(), playlistTrackIndex)
                               : std::vector<int>{},
        .queueTotal        = playerController ? playerController->queuedTracksCount() : 0,
    };
    context.playback = ScriptPlaybackData{
        .position   = currentPosition,
        .duration   = currentTrackDuration,
        .bitrate    = bitrate,
        .decoder    = std::move(decoder),
        .outputInfo = std::move(outputInfo),
        .playState  = playState,
        .queueMode  = queueMode,
        .track      = playerController ? playerController->currentTrack() : Track{},
    };

    return context;
}
} // namespace Fooyin
