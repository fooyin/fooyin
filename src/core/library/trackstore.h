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

namespace Fy::Core::Library {
class LibraryManager;

class TrackStore
{
public:
    virtual ~TrackStore() = default;

    [[nodiscard]] virtual bool hasTrack(int id) const = 0;
    [[nodiscard]] virtual Track* track(int id)        = 0;
    [[nodiscard]] virtual TrackPtrList tracks() const = 0;

    virtual TrackPtrList add(const TrackList& tracks)      = 0;
    virtual TrackPtrList update(const TrackList& tracks)   = 0;
    virtual void markForDelete(const TrackPtrList& tracks) = 0;
    virtual void remove(const TrackPtrList& tracks)        = 0;

    virtual void sort(SortOrder order) = 0;
};
} // namespace Fy::Core::Library
