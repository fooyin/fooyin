/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <core/library/libraryutils.h>

#include <QFileInfo>

namespace Fooyin::Utils {
std::vector<int> updateCommonTracks(TrackList& tracks, const TrackList& updatedTracks, CommonOperation operation)
{
    std::vector<int> indexes;

    Fooyin::TrackList result;
    result.reserve(tracks.size());

    for(auto trackIt{tracks.begin()}; trackIt != tracks.end(); ++trackIt) {
        auto updatedIt = std::ranges::find_if(updatedTracks, [&trackIt](const Fooyin::Track& updatedTrack) {
            return trackIt->sameIdentityAs(updatedTrack);
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

std::optional<QString> physicalSourceKey(const Track& track)
{
    if(!track.isValid() || track.isRemote()) {
        return {};
    }

    const QString filepath = track.isInArchive() ? track.archivePath() : track.filepath();
    if(filepath.isEmpty()) {
        return {};
    }

    const QFileInfo info{filepath};
    const QString canonicalPath = info.canonicalFilePath();
    return canonicalPath.isEmpty() ? info.absoluteFilePath() : canonicalPath;
}
} // namespace Fooyin::Utils
