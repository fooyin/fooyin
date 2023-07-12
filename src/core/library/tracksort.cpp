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

#include "tracksort.h"

namespace Fy::Core::Library::Sorting {
TrackList sortTracks(const TrackList& tracks)
{
    TrackList sortedTracks{tracks};
    std::sort(sortedTracks.begin(), sortedTracks.end(), [](const Track& lhs, const Track& rhs) {
        const auto cmp = QString::localeAwareCompare(lhs.sort(), rhs.sort());
        if(cmp == 0) {
            return false;
        }
        return cmp < 0;
    });
    return sortedTracks;
}

Scripting::ParsedScript parseScript(const QString& sort)
{
    Scripting::Registry registry;
    Scripting::Parser parser{&registry};

    return parser.parse(sort);
}

TrackList calcSortFields(const QString& sort, const TrackList& tracks)
{
    return calcSortFields(parseScript(sort), tracks);
}

TrackList calcSortFields(const Scripting::ParsedScript& sortScript, const TrackList& tracks)
{
    Scripting::Registry registry;
    Scripting::Parser parser{&registry};

    TrackList calcTracks{tracks};
    for(Track& track : calcTracks) {
        track.setSort(parser.evaluate(sortScript, track));
    }
    return calcTracks;
}
} // namespace Fy::Core::Library::Sorting
