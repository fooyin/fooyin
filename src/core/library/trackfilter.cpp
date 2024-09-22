/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/library/trackfilter.h>

#include <core/track.h>
#include <utils/helpers.h>

namespace {
// TODO: Use user-defined search script
bool matchSearch(const Fooyin::Track& track, const QString& search)
{
    if(search.isEmpty()) {
        return true;
    }

    const QStringList terms = search.split(u' ', Qt::SkipEmptyParts);
    return std::ranges::all_of(terms, [&track](const QString& term) { return track.hasMatch(term); });
}
} // namespace

namespace Fooyin::Filter {
TrackList filterTracks(const TrackList& tracks, const QString& search)
{
    return Utils::filter(tracks, [search](const Track& track) { return matchSearch(track, search); });
}
} // namespace Fooyin::Filter
