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

#include "playlistnavigator.h"

namespace Fooyin {
PlaylistNavigator::PlaylistNavigator(std::shared_ptr<AudioLoader> audioLoader)
    : m_audioLoader{std::move(audioLoader)}
{ }

PlaylistTrack PlaylistNavigator::currentTrack(Playlist* playlist) const
{
    if(!playlist) {
        return {};
    }

    return populateTrackMetadata(playlist, playlist->currentTrack(), playlist->currentTrackIndex());
}

PlaylistTrack PlaylistNavigator::previewRelativeTrack(Playlist* playlist, Playlist::PlayModes mode, int delta) const
{
    if(!playlist) {
        return {};
    }

    const int index = playlist->nextIndex(delta, mode);
    if(const auto track = playlist->track(index)) {
        return populateTrackMetadata(playlist, track.value(), index);
    }

    return {};
}

PlaylistTrack PlaylistNavigator::previewRelativeTrackFrom(Playlist* playlist, int currentIndex,
                                                          Playlist::PlayModes mode, int delta) const
{
    if(!playlist || currentIndex < 0) {
        return {};
    }

    const int index = playlist->nextIndexFrom(currentIndex, delta, mode);
    if(const auto track = playlist->track(index)) {
        return populateTrackMetadata(playlist, track.value(), index);
    }

    return {};
}

PlaylistTrack PlaylistNavigator::advanceRelativeTrack(Playlist* playlist, Playlist::PlayModes mode, int delta) const
{
    if(!playlist) {
        return {};
    }

    Track track = playlist->nextTrackChange(delta, mode);
    if(!track.isValid()) {
        return {};
    }

    return populateTrackMetadata(playlist, std::move(track), playlist->currentTrackIndex());
}

PlaylistTrack PlaylistNavigator::populateTrackMetadata(Playlist* playlist, Track track, int index) const
{
    if(!playlist || !track.isValid() || index < 0) {
        return {};
    }

    if(!track.isRemote() && !track.metadataWasRead() && m_audioLoader && m_audioLoader->readTrackMetadata(track)) {
        track.generateHash();
        playlist->updateTrackAtIndex(index, track);
    }

    if(const auto playlistTrack = playlist->playlistTrack(index)) {
        auto result{*playlistTrack};
        result.track = track;
        return result;
    }

    return {.track = track, .playlistId = playlist->id(), .entryId = {}, .indexInPlaylist = index};
}
} // namespace Fooyin
