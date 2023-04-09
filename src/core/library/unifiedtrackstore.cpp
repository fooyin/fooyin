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

#include "unifiedtrackstore.h"

namespace Fy::Core::Library {
bool UnifiedTrackStore::hasTrack(int id) const
{
    return std::any_of(m_libraryStores.cbegin(), m_libraryStores.cend(), [id](const auto& library) {
        return library.second->hasTrack(id);
    });
}

Track UnifiedTrackStore::track(int id)
{
    for(const auto& [libraryId, store] : m_libraryStores) {
        if(store->hasTrack(id)) {
            return store->track(id);
        }
    }
    return {};
}

TrackList UnifiedTrackStore::tracks() const
{
    TrackList tracks{};
    for(const auto& [libraryId, store] : m_libraryStores) {
        const auto libraryTracks = store->tracks();
        tracks.insert(tracks.end(), libraryTracks.cbegin(), libraryTracks.cend());
    }
    return tracks;
}

TrackList UnifiedTrackStore::add(const TrackList& tracks)
{
    Q_UNUSED(tracks)
    return {};
}

TrackList UnifiedTrackStore::update(const TrackList& tracks)
{
    Q_UNUSED(tracks)
    return {};
}

void UnifiedTrackStore::remove(const TrackList& tracks)
{
    Q_UNUSED(tracks)
}

void UnifiedTrackStore::sort(SortOrder order)
{
    for(const auto& [libraryId, store] : m_libraryStores) {
        store->sort(order);
    }
}

void UnifiedTrackStore::addLibrary(int libraryId, TrackStore* store)
{
    m_libraryStores.emplace(libraryId, store);
}

void UnifiedTrackStore::removeLibrary(int libraryId)
{
    m_libraryStores.erase(libraryId);
}
} // namespace Fy::Core::Library
