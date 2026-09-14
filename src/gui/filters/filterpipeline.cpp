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
void FilterRowLookup::rebuildRows(const FilterRowList& rows)
{
    m_rowIndexes.clear();
    m_rowIndexes.reserve(rows.size());

    for(std::size_t index{0}; index < rows.size(); ++index) {
        m_rowIndexes.emplace(rows.at(index).key, index);
    }
}

std::optional<size_t> FilterRowLookup::rowIndex(const RowKey& key) const
{
    const auto it = m_rowIndexes.find(key);
    return it != m_rowIndexes.cend() ? std::optional{it->second} : std::nullopt;
}

FilterSelectionResolution resolveFilterSelection(const FilterRowList& rows, const TrackList& inputTracks,
                                                 const std::vector<RowKey>& selectedKeys)
{
    FilterRowLookup lookup;
    lookup.rebuildRows(rows);
    return resolveFilterSelection(rows, inputTracks, selectedKeys, lookup);
}

FilterSelectionResolution resolveFilterSelection(const FilterRowList& rows, const TrackList& inputTracks,
                                                 const std::vector<RowKey>& selectedKeys, const FilterRowLookup& lookup)
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
                for(const Track& track : row.tracks) {
                    selectedTrackIds.emplace(track.id());
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

    std::vector<RowKey> prunedKeys;
    prunedKeys.reserve(resolution.selectedKeys.size());

    for(const RowKey& key : resolution.selectedKeys) {
        const auto rowIndex = lookup.rowIndex(key);
        if(!rowIndex || *rowIndex >= rows.size()) {
            continue;
        }

        prunedKeys.push_back(key);

        const TrackList& rowTracks = rows.at(*rowIndex).tracks;
        std::ranges::copy(rowTracks, std::back_inserter(resolution.selectedTracks));
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
