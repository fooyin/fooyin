/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "singletrackstore.h"

#include "core/library/sorting/sorting.h"

namespace Fy::Core::Library {
TrackList SingleTrackStore::tracks() const
{
    return m_tracks;
}

void SingleTrackStore::add(const TrackList& tracks)
{
    m_tracks.reserve(m_tracks.capacity() + tracks.size());
    for(const Track& track : tracks) {
        m_tracks.emplace_back(track);
    }
}

void SingleTrackStore::update(const TrackList& tracks)
{
    for(const auto& track : tracks) {
        auto it = std::find(m_tracks.begin(), m_tracks.end(), track);
        if(it != m_tracks.cend()) {
            *it = track;
        }
    }
}

void SingleTrackStore::remove(const TrackList& tracks)
{
    for(const Track& track : tracks) {
        remove(track.id());
    }
}

void SingleTrackStore::remove(int trackId)
{
    m_tracks.erase(std::find_if(m_tracks.begin(), m_tracks.end(), [trackId](const Track& track) {
        return track.id() == trackId;
    }));
}

void SingleTrackStore::sort(SortOrder order)
{
    Sorting::sortTracks(m_tracks, order);
}

void SingleTrackStore::clear()
{
    m_tracks.clear();
}
} // namespace Fy::Core::Library
