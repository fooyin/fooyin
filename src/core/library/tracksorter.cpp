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

#include "tracksorter.h"

namespace Fy::Core::Library {
void TrackSorter::sortTracks(TrackList& tracks)
{
    TrackList sortedTracks{tracks};
    std::sort(sortedTracks.begin(), sortedTracks.end(), [](const Track& lhs, const Track& rhs) {
        const auto cmp = QString::localeAwareCompare(lhs.sort(), rhs.sort());
        if(cmp == 0) {
            return false;
        }
        return QString::localeAwareCompare(lhs.sort(), rhs.sort()) < 0;
    });
    tracks = sortedTracks;
}

void TrackSorter::calcSortField(Track& track)
{
    track.setSort(m_parser.evaluate(m_sortScript, track));
}

void TrackSorter::calcSortFields(TrackList& tracks)
{
    for(Track& track : tracks) {
        track.setSort(m_parser.evaluate(m_sortScript, track));
    }
}

void TrackSorter::changeSorting(const QString& sort)
{
    const auto sortScript = m_parser.parse(sort);
    if(sortScript.valid) {
        m_sortScript = sortScript;
    }
}
} // namespace Fy::Core::Library
