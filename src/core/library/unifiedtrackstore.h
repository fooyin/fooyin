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
class UnifiedTrackStore : public TrackStore
{
public:
    [[nodiscard]] TrackList tracks() const override;

    void add(const TrackList& tracks) override;
    void update(const TrackList& tracks) override;
    void remove(const TrackList& tracks) override;

    void sort(SortOrder order) override;

    void addLibrary(int libraryId, TrackStore* store);
    void removeLibrary(int libraryId);

private:
    std::unordered_map<int, TrackStore*> m_libraryStores;
};
} // namespace Fy::Core::Library
