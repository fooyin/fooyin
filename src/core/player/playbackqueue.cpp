/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <core/player/playbackqueue.h>

#include <core/track.h>

constexpr auto MaxQueue = 250;

namespace Fooyin {
bool PlaybackQueue::empty() const
{
    return m_tracks.empty();
}

QueueTracks PlaybackQueue::tracks() const
{
    return m_tracks;
}

PlaylistTrack PlaybackQueue::track(int index) const
{
    if(m_tracks.empty() || index < 0 || index >= trackCount()) {
        return {};
    }

    return m_tracks.at(index);
}

int PlaybackQueue::trackCount() const
{
    return static_cast<int>(m_tracks.size());
}

int PlaybackQueue::freeSpace() const
{
    return MaxQueue - trackCount();
}

PlaylistIndexes PlaybackQueue::playlistIndexes() const
{
    PlaylistIndexes indexes;

    for(const auto& track : m_tracks) {
        indexes[track.playlistId].emplace_back(track.indexInPlaylist);
    }

    return indexes;
}

PlaylistTrackIndexes PlaybackQueue::indexesForPlaylist(const Id& id) const
{
    PlaylistTrackIndexes indexes;

    for(auto i{0}; const auto& track : m_tracks) {
        if(track.playlistId == id) {
            indexes[track.indexInPlaylist].emplace_back(i);
        }
        ++i;
    }

    return indexes;
}

PlaylistTrack PlaybackQueue::nextTrack()
{
    if(m_tracks.empty()) {
        return {};
    }

    auto track = m_tracks.front();
    m_tracks.erase(m_tracks.begin());
    return track;
}

void PlaybackQueue::addTracks(const QueueTracks& tracks)
{
    m_tracks.insert(m_tracks.end(), tracks.cbegin(), tracks.cend());
}

void PlaybackQueue::replaceTracks(const QueueTracks& tracks)
{
    m_tracks = tracks;
}

QueueTracks PlaybackQueue::removeTracks(const QueueTracks& tracks)
{
    QueueTracks removedTracks;

    std::set<PlaylistTrack> tracksToRemove{tracks.cbegin(), tracks.cend()};

    auto matchingTrack = [&tracksToRemove](const PlaylistTrack& track) {
        const bool isSame = tracksToRemove.contains(track);
        return isSame;
    };

    std::ranges::copy_if(m_tracks, std::back_inserter(removedTracks), matchingTrack);
    std::erase_if(m_tracks, matchingTrack);

    return removedTracks;
}

QueueTracks PlaybackQueue::removePlaylistTracks(const Id& playlistId)
{
    QueueTracks removedTracks;

    auto it = std::remove_if(m_tracks.begin(), m_tracks.end(),
                             [playlistId](const PlaylistTrack& track) { return track.playlistId == playlistId; });

    removedTracks.insert(removedTracks.end(), std::make_move_iterator(it), std::make_move_iterator(m_tracks.end()));
    m_tracks.erase(it, m_tracks.end());

    return removedTracks;
}

void PlaybackQueue::clear()
{
    m_tracks.clear();
}
} // namespace Fooyin
