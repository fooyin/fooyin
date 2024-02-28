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

#include <ranges>

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

PlaylistTrack PlaybackQueue::nextTrack()
{
    if(m_tracks.empty()) {
        return {};
    }

    auto track = m_tracks.front();
    m_tracks.erase(m_tracks.begin());
    return track;
}

void PlaybackQueue::addTrack(const PlaylistTrack& track)
{
    m_tracks.emplace_back(track);
}

void PlaybackQueue::addTrack(const Track& track)
{
    m_tracks.emplace_back(track);
}

void PlaybackQueue::addTracks(const QueueTracks& tracks)
{
    m_tracks.insert(m_tracks.end(), tracks.cbegin(), tracks.cend());
}

void PlaybackQueue::addTracks(const TrackList& tracks)
{
    for(const Track& track : tracks) {
        addTrack(track);
    }
}

void PlaybackQueue::removeTrack(int index)
{
    if(index >= 0 && index < trackCount()) {
        m_tracks.erase(m_tracks.begin() + index);
    }
}

void PlaybackQueue::removeTracks(const IndexSet& indexes)
{
    for(const int index : indexes | std::views::reverse) {
        removeTrack(index);
    }
}

void PlaybackQueue::clear()
{
    m_tracks.clear();
}
} // namespace Fooyin
