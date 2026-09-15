/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include <core/playlist/playlistchangeset.h>

#include <algorithm>
#include <ranges>
#include <unordered_map>
#include <utility>

constexpr auto ResetThreshold = 500;

namespace Fooyin {
namespace {
using TrackEntryList = std::vector<UId>;

TrackEntryList playlistTrackEntries(const PlaylistTrackList& tracks)
{
    TrackEntryList result;
    result.reserve(tracks.size());
    std::ranges::transform(tracks, std::back_inserter(result), &PlaylistTrack::entryId);
    return result;
}

std::optional<std::vector<PlaylistTrackMove>> buildPlaylistMoves(TrackEntryList currentEntries,
                                                                 const TrackEntryList& newEntries, int moveLimit)
{
    if(currentEntries.size() != newEntries.size()) {
        return {};
    }

    std::vector<PlaylistTrackMove> result;

    for(int targetIndex{0}; std::cmp_less(targetIndex, newEntries.size()); ++targetIndex) {
        if(currentEntries.at(static_cast<size_t>(targetIndex)) == newEntries.at(static_cast<size_t>(targetIndex))) {
            continue;
        }

        const auto sourceIt = std::ranges::find(currentEntries.begin() + targetIndex, currentEntries.end(),
                                                newEntries.at(static_cast<size_t>(targetIndex)));
        if(sourceIt == currentEntries.end()) {
            return {};
        }

        result.emplace_back(*sourceIt, targetIndex);

        // Retain one move beyond budget so caller can distinguish exceeded limit from an exact fit
        if(std::cmp_greater(result.size(), moveLimit)) {
            break;
        }

        std::rotate(currentEntries.begin() + targetIndex, sourceIt, sourceIt + 1);
    }

    return result;
}
} // namespace

std::optional<PlaylistChangeset> buildPlaylistChangeset(const PlaylistTrackList& oldTracks,
                                                        const PlaylistTrackList& newTracks,
                                                        const TrackEntryIdSet& updatedTrackEntries)
{
    PlaylistChangeset result;
    const TrackEntryList newTrackEntries = playlistTrackEntries(newTracks);

    std::unordered_map<UId, int, UId::UIdHash> newTrackIndexes;
    newTrackIndexes.reserve(newTracks.size());

    for(int newIndex{0}; const auto& track : newTracks) {
        if(!track.entryId.isValid() || !newTrackIndexes.emplace(track.entryId, newIndex++).second) {
            return {};
        }
    }

    TrackEntryList retainedTrackEntries;
    retainedTrackEntries.reserve(std::min(oldTracks.size(), newTracks.size()));

    TrackEntryIdSet oldEntrySet;
    oldEntrySet.reserve(oldTracks.size());

    for(const auto& track : oldTracks) {
        if(!track.entryId.isValid() || !oldEntrySet.emplace(track.entryId).second) {
            return {};
        }
    }

    for(const auto& oldTrack : oldTracks) {
        const auto newIt = newTrackIndexes.find(oldTrack.entryId);
        if(newIt == newTrackIndexes.end()) {
            result.removedEntries.emplace_back(oldTrack.entryId);
            continue;
        }

        retainedTrackEntries.emplace_back(oldTrack.entryId);

        if(!oldTrack.track.sameDataAs(newTracks.at(static_cast<size_t>(newIt->second)).track)
           || updatedTrackEntries.contains(oldTrack.entryId)) {
            result.updatedEntries.emplace_back(oldTrack.entryId);
        }
    }

    result.replacesAllEntries = !oldTracks.empty() && retainedTrackEntries.empty();

    PlaylistTrackInsertion insertion;
    for(int newIndex{0}; const auto& track : newTracks) {
        const bool isNewTrack = !oldEntrySet.contains(track.entryId);
        if(isNewTrack) {
            if(insertion.index < 0) {
                insertion.index = newIndex;
            }
            insertion.tracks.push_back(track);
        }
        else if(insertion.isValid()) {
            result.insertions.push_back(std::move(insertion));
            insertion = {};
        }
        ++newIndex;
    }
    if(insertion.isValid()) {
        result.insertions.push_back(std::move(insertion));
    }

    int insertedTrackCount{0};
    for(const auto& insertionGroup : result.insertions) {
        insertedTrackCount += static_cast<int>(insertionGroup.tracks.size());
    }
    const int changedTrackCount  = static_cast<int>(result.removedEntries.size()) + insertedTrackCount
                                 + static_cast<int>(result.updatedEntries.size());
    const int baselineTrackCount = static_cast<int>(std::max(oldTracks.size(), newTracks.size()));
    const int changeLimit        = std::min(ResetThreshold, baselineTrackCount / 2);
    if(changedTrackCount > changeLimit) {
        result.requiresReset = true;
        return result;
    }

    TrackEntryList currentEntries{retainedTrackEntries};
    for(const auto& groupedInsertion : result.insertions) {
        const TrackEntryList insertionEntries = playlistTrackEntries(groupedInsertion.tracks);
        currentEntries.insert(currentEntries.begin() + groupedInsertion.index, insertionEntries.cbegin(),
                              insertionEntries.cend());
    }

    if(const auto moves
       = buildPlaylistMoves(std::move(currentEntries), newTrackEntries, changeLimit - changedTrackCount)) {
        result.moves = *moves;
    }
    else {
        return {};
    }

    if(changedTrackCount + static_cast<int>(result.moves.size()) > changeLimit) {
        result.requiresReset = true;
    }

    return result;
}
} // namespace Fooyin
