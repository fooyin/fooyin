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

#include "libraryutils.h"

namespace Fooyin::Utils {
std::vector<int> updateCommonTracks(TrackList& tracks, const TrackList& updatedTracks, CommonOperation operation)
{
    std::vector<int> indexes;

    Fooyin::TrackList result;
    result.reserve(tracks.size());

    for(auto trackIt{tracks.begin()}; trackIt != tracks.end(); ++trackIt) {
        auto updatedIt = std::ranges::find_if(updatedTracks, [&trackIt](const Fooyin::Track& updatedTrack) {
            return updatedTrack.isInDatabase() && trackIt->id() == updatedTrack.id();
        });
        if(updatedIt != updatedTracks.end()) {
            indexes.push_back(static_cast<int>(std::distance(tracks.begin(), trackIt)));
            if(operation == CommonOperation::Update) {
                result.push_back(*updatedIt);
            }
        }
        else {
            result.push_back(*trackIt);
        }
    }

    tracks = result;
    return indexes;
}
} // namespace Fooyin::Utils
