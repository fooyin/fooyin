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

#pragma once

#include "fygui_export.h"

#include "filterrows.h"

#include <functional>

namespace Fooyin::Filters {
struct FYGUI_EXPORT FilterSelectionResolution
{
    std::vector<RowKey> selectedKeys;
    TrackList selectedTracks;
    bool isActive{false};
};

struct FYGUI_EXPORT FilterPipelineStageRequest
{
    std::vector<RowKey> selectedKeys;
};

struct FYGUI_EXPORT FilterPipelineStageResult
{
    TrackList inputTracks;
    FilterRowList rows;
    std::vector<RowKey> selectedKeys;
    TrackList selectedTracks;
    bool isActive{false};
};

struct FYGUI_EXPORT FilterPipelineResult
{
    std::vector<FilterPipelineStageResult> stages;
    TrackList finalFilteredTracks;
    bool hasActiveStages{false};
};

using FilterRowsBuilder = std::function<FilterRowList(int stageIndex, const TrackList& inputTracks)>;

FYGUI_EXPORT FilterSelectionResolution resolveFilterSelection(const FilterRowList& rows, const TrackList& inputTracks,
                                                              const std::vector<RowKey>& selectedKeys);
FYGUI_EXPORT FilterPipelineResult runFilterPipeline(const TrackList& sourceTracks,
                                                    const std::vector<FilterPipelineStageRequest>& stages,
                                                    const FilterRowsBuilder& rowBuilder);
} // namespace Fooyin::Filters
