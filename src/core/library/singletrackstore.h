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

#pragma once

#include "trackstore.h"

namespace Fy::Core::Library {
class SingleTrackStore : public TrackStore
{
public:
    [[nodiscard]] bool hasTrack(int id) const override;

    [[nodiscard]] Track* track(int id) override;
    [[nodiscard]] TrackPtrList tracks() const override;

    TrackPtrList add(const TrackList& tracks) override;
    Track* add(const Track& track);

    TrackPtrList update(const TrackList& tracks) override;
    Track* update(const Track& track);

    void markForDelete(const TrackPtrList& tracks) override;
    void markForDelete(Track* trackId);

    void remove(const TrackPtrList& tracks) override;
    void remove(int trackId);

    void sort(SortOrder order) override;

    void clear();

private:
    TrackIdMap m_trackIdMap;
    TrackPtrList m_tracks;
};
} // namespace Fy::Core::Library
