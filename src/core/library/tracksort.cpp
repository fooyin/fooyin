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

#include <core/library/tracksort.h>

#include <core/scripting/scriptparser.h>
#include <core/track.h>

namespace {
Fooyin::ParsedScript parseScript(const QString& sort)
{
    static Fooyin::ScriptRegistry registry;
    static Fooyin::ScriptParser parser{&registry};

    return parser.parse(sort);
}
} // namespace

namespace Fooyin::Sorting {
TrackList calcSortFields(const QString& sort, const TrackList& tracks)
{
    return calcSortFields(parseScript(sort), tracks);
}

TrackList calcSortFields(const ParsedScript& sortScript, const TrackList& tracks)
{
    static ScriptRegistry registry;
    static ScriptParser parser{&registry};

    TrackList calcTracks{tracks};
    for(Track& track : calcTracks) {
        track.setSort(parser.evaluate(sortScript, track));
    }
    return calcTracks;
}

TrackList sortTracks(const TrackList& tracks, Qt::SortOrder order)
{
    TrackList sortedTracks{tracks};
    std::ranges::sort(sortedTracks, [order](const Track& lhs, const Track& rhs) {
        const auto cmp = QString::localeAwareCompare(lhs.sort(), rhs.sort());
        if(cmp == 0) {
            return false;
        }
        if(order == Qt::AscendingOrder) {
            return cmp < 0;
        }
        return cmp > 0;
    });
    return sortedTracks;
}

TrackList calcSortTracks(const QString& sort, const TrackList& tracks, Qt::SortOrder order)
{
    return calcSortTracks(parseScript(sort), tracks, order);
}

TrackList calcSortTracks(const ParsedScript& sortScript, const TrackList& tracks, Qt::SortOrder order)
{
    const TrackList calcTracks = calcSortFields(sortScript, tracks);
    return sortTracks(calcTracks, order);
}
} // namespace Fooyin::Sorting
