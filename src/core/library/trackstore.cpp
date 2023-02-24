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

#include "trackstore.h"

#include "core/library/sorting/sorting.h"

namespace Core::Library {
Track* TrackStore::track(int id)
{
    if(hasTrack(id)) {
        return m_trackIdMap.at(id).get();
    }
    return nullptr;
}

Track* TrackStore::track(const QString& filepath)
{
    if(hasTrack(filepath)) {
        return m_trackPathMap.at(filepath);
    }
    return nullptr;
}

TrackPtrList TrackStore::tracks() const
{
    return m_tracks;
}

void TrackStore::add(const TrackList& tracks)
{
    m_trackIdMap.reserve(m_trackIdMap.size() + tracks.size());
    m_tracks.reserve(m_tracks.size() + tracks.size());
    m_trackPathMap.reserve(m_trackPathMap.size() + tracks.size());

    for(const Track& track : tracks) {
        add(track);
    }
}

void TrackStore::update(const TrackList& tracks)
{
    for(const auto& track : tracks) {
        update(track);
    }
}

void TrackStore::markForDelete(const IdSet& tracks)
{
    for(auto trackId : tracks) {
        markForDelete(trackId);
    }
}

void TrackStore::remove(const IdSet& tracks)
{
    for(auto trackId : tracks) {
        remove(trackId);
    }
}

void TrackStore::sort(SortOrder order)
{
    Sorting::sortTracks(m_tracks, order);
}

void TrackStore::clear()
{
    m_tracks.clear();
    m_trackPathMap.clear();
    m_trackIdMap.clear();
}

bool TrackStore::hasTrack(int id) const
{
    return m_trackIdMap.count(id);
}

bool TrackStore::hasTrack(const QString& filepath) const
{
    return m_trackPathMap.count(filepath);
}

void TrackStore::add(const Track& track)
{
    Track* newTrack = m_trackIdMap.emplace(track.id(), std::make_unique<Track>(track)).first->second.get();
    m_tracks.emplace_back(newTrack);
    m_trackPathMap.emplace(newTrack->filepath(), newTrack);
}

void TrackStore::update(const Track& track)
{
    Track* libraryTrack = m_trackIdMap.at(track.id()).get();
    *libraryTrack       = track;
}

void TrackStore::markForDelete(int trackId)
{
    if(hasTrack(trackId)) {
        Track* track = m_trackIdMap.at(trackId).get();
        track->setIsEnabled(false);
    }
}

void TrackStore::remove(int trackId)
{
    if(hasTrack(trackId)) {
        Track* track = m_trackIdMap.at(trackId).get();
        m_tracks.erase(std::find(m_tracks.begin(), m_tracks.end(), track));
        m_trackPathMap.erase(track->filepath());
        m_trackIdMap.erase(track->id());
    }
}
} // namespace Core::Library
