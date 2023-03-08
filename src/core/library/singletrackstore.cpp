/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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
Track* SingleTrackStore::track(int id)
{
    if(hasTrack(id)) {
        return &m_trackIdMap.at(id);
    }
    return nullptr;
}

TrackPtrList SingleTrackStore::tracks() const
{
    return m_tracks;
}

void SingleTrackStore::add(const TrackList& tracks)
{
    m_trackIdMap.reserve(m_trackIdMap.size() + tracks.size());
    m_tracks.reserve(m_tracks.size() + tracks.size());

    for(const Track& track : tracks) {
        add(track);
    }
}

void SingleTrackStore::add(const Track& track)
{
    Track* newTrack = &m_trackIdMap.emplace(track.id(), track).first->second;
    m_tracks.emplace_back(newTrack);
}

void SingleTrackStore::update(const TrackList& tracks)
{
    for(const auto& track : tracks) {
        update(track);
    }
}

void SingleTrackStore::markForDelete(const TrackPtrList& tracks)
{
    for(auto* track : tracks) {
        markForDelete(track);
    }
}

void SingleTrackStore::markForDelete(Track* track)
{
    if(hasTrack(track->id())) {
        track->setIsEnabled(false);
    }
}

void SingleTrackStore::remove(const TrackPtrList& tracks)
{
    for(const auto& track : tracks) {
        remove(track->id());
    }
}

void SingleTrackStore::remove(int trackId)
{
    if(hasTrack(trackId)) {
        Track* track = &m_trackIdMap.at(trackId);
        m_tracks.erase(std::find(m_tracks.begin(), m_tracks.end(), track));
        m_trackIdMap.erase(trackId);
    }
}

void SingleTrackStore::sort(SortOrder order)
{
    Sorting::sortTracks(m_tracks, order);
}

void SingleTrackStore::clear()
{
    m_tracks.clear();
    m_trackIdMap.clear();
}

bool SingleTrackStore::hasTrack(int id) const
{
    return m_trackIdMap.count(id);
}

void SingleTrackStore::update(const Track& track)
{
    m_trackIdMap.at(track.id()) = track;
}
} // namespace Fy::Core::Library
