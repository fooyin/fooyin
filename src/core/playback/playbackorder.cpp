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

#include "playbackorder.h"

#include <core/library/tracksort.h>
#include <core/scripting/scriptparser.h>

#include <numeric>
#include <random>
#include <ranges>
#include <unordered_map>

namespace Fooyin::PlaybackOrder {
std::vector<int> shuffledTrackIndexes(int trackCount, int anchorIndex)
{
    if(trackCount <= 0) {
        return {};
    }

    std::vector<int> indexes(static_cast<size_t>(trackCount));
    std::iota(indexes.begin(), indexes.end(), 0);
    std::ranges::shuffle(indexes, std::mt19937{std::random_device{}()});

    if(anchorIndex >= 0 && anchorIndex < trackCount) {
        const auto anchor = std::ranges::find(indexes, anchorIndex);
        std::rotate(indexes.begin(), anchor, std::next(anchor));
    }

    return indexes;
}

IndexGroups groupedTrackIndexes(const TrackList& tracks, const QString& groupScript, const QString& sortScript)
{
    if(tracks.empty()) {
        return {};
    }

    IndexGroups groups;
    ScriptParser parser;

    std::unordered_map<QString, size_t> groupIndexes;
    groupIndexes.reserve(tracks.size());

    for(int index{0}; std::cmp_less(index, tracks.size()); ++index) {
        const QString group = parser.evaluate(groupScript, tracks.at(index));
        auto [it, inserted] = groupIndexes.try_emplace(group, groups.size());
        if(inserted) {
            groups.emplace_back();
        }
        groups.at(it->second).push_back(index);
    }

    if(!sortScript.isEmpty()) {
        TrackSorter sorter;
        for(auto& group : groups) {
            group = sorter.calcSortTracks(sortScript, group,
                                          [&tracks](int index) -> const Track& { return tracks.at(index); });
        }
    }

    return groups;
}

IndexGroups shuffledTrackGroups(IndexGroups groups, const int anchorIndex)
{
    std::ranges::shuffle(groups, std::mt19937{std::random_device{}()});

    if(anchorIndex >= 0) {
        const auto anchor = std::ranges::find_if(
            groups, [anchorIndex](const auto& group) { return std::ranges::find(group, anchorIndex) != group.end(); });
        if(anchor != groups.end()) {
            std::rotate(groups.begin(), anchor, std::next(anchor));
        }
    }

    return groups;
}
} // namespace Fooyin::PlaybackOrder
