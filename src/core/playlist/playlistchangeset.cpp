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

#include <utility>

constexpr auto ResetThreshold = 500;

namespace Fooyin {
namespace {
using TrackKeyList = std::vector<QString>;

TrackKeyList playlistTrackKeys(const TrackList& tracks)
{
    TrackKeyList result;
    result.reserve(tracks.size());
    std::ranges::transform(tracks, std::back_inserter(result), &Track::uniqueFilepath);
    return result;
}

std::optional<std::vector<PlaylistTrackMove>> buildPlaylistMoves(TrackKeyList currentKeys, const TrackKeyList& newKeys)
{
    if(currentKeys.size() != newKeys.size()) {
        return {};
    }

    std::vector<PlaylistTrackMove> result;

    for(int targetIndex{0}; std::cmp_less(targetIndex, newKeys.size()); ++targetIndex) {
        if(currentKeys.at(static_cast<size_t>(targetIndex)) == newKeys.at(static_cast<size_t>(targetIndex))) {
            continue;
        }

        const auto sourceIt = std::ranges::find(currentKeys.cbegin() + targetIndex, currentKeys.cend(),
                                                newKeys.at(static_cast<size_t>(targetIndex)));
        if(sourceIt == currentKeys.cend()) {
            return {};
        }

        const int sourceIndex = static_cast<int>(std::distance(currentKeys.cbegin(), sourceIt));
        result.push_back({.sourceIndex = sourceIndex, .targetIndex = targetIndex});

        std::rotate(currentKeys.begin() + targetIndex, currentKeys.begin() + sourceIndex,
                    currentKeys.begin() + sourceIndex + 1);
    }

    return result;
}
} // namespace

std::optional<PlaylistChangeset> buildPlaylistChangeset(const TrackList& oldTracks, const TrackList& newTracks,
                                                        const TrackKeySet& updatedTrackPaths)
{
    PlaylistChangeset result;
    const TrackKeyList newTrackKeys = playlistTrackKeys(newTracks);
    std::set<int> updatedIndexes;

    std::unordered_map<QString, int> newTrackIndexes;
    newTrackIndexes.reserve(newTracks.size());

    for(int newIndex{0}; const auto& key : newTrackKeys) {
        if(!newTrackIndexes.emplace(key, newIndex++).second) {
            return {};
        }
    }

    std::unordered_set<QString> oldTrackKeys;
    oldTrackKeys.reserve(oldTracks.size());

    TrackKeyList retainedTrackKeys;
    retainedTrackKeys.reserve(std::min(oldTracks.size(), newTracks.size()));

    for(int oldIndex{0}; const auto& oldTrack : oldTracks) {
        const QString key = oldTrack.uniqueFilepath();
        if(!oldTrackKeys.emplace(key).second) {
            return {};
        }

        const auto newIndexIt = newTrackIndexes.find(key);
        if(newIndexIt == newTrackIndexes.cend()) {
            result.removedIndexes.push_back(oldIndex);
            ++oldIndex;
            continue;
        }

        const int newIndex = newIndexIt->second;
        retainedTrackKeys.push_back(key);

        if(oldTrack != newTracks.at(static_cast<size_t>(newIndex))) {
            updatedIndexes.emplace(newIndex);
        }
        if(updatedTrackPaths.contains(key)) {
            updatedIndexes.emplace(newIndex);
        }

        ++oldIndex;
    }

    PlaylistTrackInsertion insertion;
    for(int newIndex{0}; const auto& track : newTracks) {
        const bool isNewTrack = !oldTrackKeys.contains(track.uniqueFilepath());
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

    TrackKeyList currentKeys{retainedTrackKeys};
    for(const auto& groupedInsertion : result.insertions) {
        const TrackKeyList insertionKeys = playlistTrackKeys(groupedInsertion.tracks);
        currentKeys.insert(currentKeys.begin() + groupedInsertion.index, insertionKeys.cbegin(), insertionKeys.cend());
    }

    if(const auto moves = buildPlaylistMoves(std::move(currentKeys), newTrackKeys)) {
        result.moves = *moves;
    }
    else {
        return {};
    }

    result.updatedIndexes.assign(updatedIndexes.cbegin(), updatedIndexes.cend());

    int insertedTrackCount{0};
    for(const auto& insertionGroup : result.insertions) {
        insertedTrackCount += static_cast<int>(insertionGroup.tracks.size());
    }

    const int changedTrackCount = static_cast<int>(result.removedIndexes.size()) + insertedTrackCount
                                + static_cast<int>(result.moves.size())
                                + static_cast<int>(result.updatedIndexes.size());
    const int baselineTrackCount
        = static_cast<int>(oldTracks.size() > newTracks.size() ? oldTracks.size() : newTracks.size());

    if(changedTrackCount > ResetThreshold || changedTrackCount > (baselineTrackCount / 2)) {
        result.requiresReset = true;
    }

    return result;
}
} // namespace Fooyin
