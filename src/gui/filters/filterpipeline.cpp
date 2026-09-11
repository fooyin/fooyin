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

#include "filterpipeline.h"

#include <unordered_map>
#include <unordered_set>

namespace Fooyin::Filters {
FilterSelectionResolution resolveFilterSelection(const FilterRowList& rows, const TrackList& inputTracks,
                                                 const std::vector<RowKey>& selectedKeys)
{
    FilterSelectionResolution resolution;
    resolution.selectedKeys = selectedKeys;
    resolution.isActive     = !resolution.selectedKeys.empty();

    if(!resolution.isActive) {
        return resolution;
    }

    for(const RowKey& key : resolution.selectedKeys) {
        if(key.isEmpty()) {
            if(rows.empty()) {
                resolution.selectedTracks = inputTracks;
                return resolution;
            }

            std::unordered_set<int> selectedTrackIds;
            selectedTrackIds.reserve(inputTracks.size());

            for(const FilterRow& row : rows) {
                for(const int trackId : row.trackIds) {
                    selectedTrackIds.emplace(trackId);
                }
            }

            resolution.selectedTracks.reserve(selectedTrackIds.size());
            for(const Track& track : inputTracks) {
                if(selectedTrackIds.contains(track.id())) {
                    resolution.selectedTracks.push_back(track);
                }
            }

            return resolution;
        }
    }

    std::unordered_map<int, Track> tracksById;
    tracksById.reserve(inputTracks.size());
    for(const Track& track : inputTracks) {
        tracksById.emplace(track.id(), track);
    }

    std::unordered_map<RowKey, const FilterRow*> rowsByKey;
    rowsByKey.reserve(rows.size());
    for(const FilterRow& row : rows) {
        rowsByKey.emplace(row.key, &row);
    }

    std::vector<RowKey> prunedKeys;
    prunedKeys.reserve(resolution.selectedKeys.size());

    for(const RowKey& key : resolution.selectedKeys) {
        const auto rowIt = rowsByKey.find(key);
        if(rowIt == rowsByKey.cend()) {
            continue;
        }

        prunedKeys.push_back(key);

        for(const int trackId : rowIt->second->trackIds) {
            if(const auto trackIt = tracksById.find(trackId); trackIt != tracksById.cend()) {
                resolution.selectedTracks.push_back(trackIt->second);
            }
        }
    }

    resolution.selectedKeys = std::move(prunedKeys);
    resolution.isActive     = !resolution.selectedKeys.empty();
    return resolution;
}

FilterPipelineResult runFilterPipeline(const TrackList& sourceTracks,
                                       const std::vector<FilterPipelineStageRequest>& stages,
                                       const FilterRowsBuilder& rowBuilder)
{
    FilterPipelineResult result;
    result.stages.reserve(stages.size());

    TrackList currentTracks = sourceTracks;
    bool constrained{false};

    for(int stageIndex{0}; std::cmp_less(stageIndex, stages.size()); ++stageIndex) {
        FilterPipelineStageResult stage;
        stage.inputTracks = currentTracks;
        stage.rows        = rowBuilder ? rowBuilder(stageIndex, stage.inputTracks) : FilterRowList{};

        const FilterSelectionResolution selection
            = resolveFilterSelection(stage.rows, stage.inputTracks, stages.at(stageIndex).selectedKeys);
        stage.selectedKeys   = selection.selectedKeys;
        stage.selectedTracks = selection.selectedTracks;
        stage.isActive       = selection.isActive;

        if(stage.isActive) {
            currentTracks = stage.selectedTracks;
            constrained   = true;
        }

        result.stages.push_back(std::move(stage));
    }

    result.finalFilteredTracks = constrained ? currentTracks : TrackList{};
    result.hasActiveStages     = constrained;
    return result;
}
} // namespace Fooyin::Filters
