/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "tracklistfuncs.h"

#include <core/track.h>
#include <utils/stringutils.h>

#include <set>

using namespace Qt::StringLiterals;

namespace Fooyin::Scripting {
int trackCount(const TrackList& tracks)
{
    return static_cast<int>(tracks.size());
}

QString playtime(const TrackList& tracks)
{
    const uint64_t total = std::transform_reduce(tracks.cbegin(), tracks.cend(), 0ULL, std::plus<>(),
                                                 [](const auto& track) { return track.duration(); });

    return Utils::msToString(total);
}

QString genres(const TrackList& tracks)
{
    std::set<QString> uniqueGenres;

    for(const auto& track : tracks) {
        const auto trackGenres = track.genres();
        const std::set<QString> genreSet{trackGenres.cbegin(), trackGenres.cend()};
        uniqueGenres.insert(genreSet.cbegin(), genreSet.cend());
    }

    const QStringList genreList{uniqueGenres.cbegin(), uniqueGenres.cend()};

    return genreList.join(" / "_L1);
}
} // namespace Fooyin::Scripting
