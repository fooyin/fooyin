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

#pragma once

#include "core/library/sorting/sortorder.h"
#include "core/models/trackfwd.h"

#include <QReadWriteLock>

namespace Core::Library {
class TrackStore
{
public:
    Track* track(int id);
    Track* track(const QString& filepath);
    TrackPtrList tracks() const;

    void add(const TrackList& tracks);
    void update(const TrackList& tracks);
    void markForDelete(const IdSet& tracks);
    void remove(const IdSet& tracks);

    void sort(SortOrder order);

    void clear();

private:
    bool hasTrack(int id) const;
    bool hasTrack(const QString& filepath) const;

    void add(const Track& track);
    void update(const Track& track);
    void markForDelete(int trackId);
    void remove(int trackId);

    TrackIdUniqPtrMap m_trackIdMap;
    TrackPtrList m_tracks;
    TrackPathMap m_trackPathMap;
};
} // namespace Core::Library
