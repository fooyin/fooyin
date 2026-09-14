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

#include "gui/filters/filterpipeline.h"
#include "gui/filters/filterrows.h"

#include <core/scripting/trackqueryfilter.h>

#include <gtest/gtest.h>

#include <unordered_set>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
namespace {
Track makeTrack(int id, const QString& path)
{
    Track track{path};
    track.setId(id);
    return track;
}

Filters::RowKey keyFor(const QString& value)
{
    return value.toUtf8();
}

Filters::FilterRow makeRow(const Filters::RowKey& key, std::initializer_list<int> trackIds)
{
    Filters::FilterRow row;
    row.key = key;
    for(const int trackId : trackIds) {
        row.tracks.push_back(makeTrack(trackId, u"/music/%1.flac"_s.arg(trackId)));
    }
    return row;
}
} // namespace

TEST(FilterPipelineTest, ResolveFilterSelectionPrunesMissingKeysAndKeepsMatchingTracks)
{
    const TrackList inputTracks{
        makeTrack(1, u"/music/one.flac"_s),
        makeTrack(2, u"/music/two.flac"_s),
        makeTrack(3, u"/music/three.flac"_s),
    };
    const Filters::FilterRowList rows{
        makeRow(keyFor(u"A"_s), {1, 2}),
        makeRow(keyFor(u"B"_s), {3}),
    };

    const Filters::FilterSelectionResolution selection
        = Filters::resolveFilterSelection(rows, inputTracks, {keyFor(u"A"_s), keyFor(u"missing"_s), keyFor(u"B"_s)});

    ASSERT_EQ(2, selection.selectedKeys.size());
    EXPECT_EQ(keyFor(u"A"_s), selection.selectedKeys.at(0));
    EXPECT_EQ(keyFor(u"B"_s), selection.selectedKeys.at(1));
    EXPECT_TRUE(selection.isActive);
    ASSERT_EQ(3, selection.selectedTracks.size());
    EXPECT_EQ(1, selection.selectedTracks.at(0).id());
    EXPECT_EQ(2, selection.selectedTracks.at(1).id());
    EXPECT_EQ(3, selection.selectedTracks.at(2).id());
}

TEST(FilterPipelineTest, ResolveFilterSelectionUsesRebuiltRowLookup)
{
    const TrackList initialTracks{
        makeTrack(1, u"/music/one.flac"_s),
        makeTrack(2, u"/music/two.flac"_s),
    };
    const Filters::FilterRowList initialRows{
        makeRow(keyFor(u"A"_s), {1}),
    };

    Filters::FilterRowLookup lookup;
    lookup.rebuildRows(initialRows);

    const auto initialSelection = Filters::resolveFilterSelection(initialRows, initialTracks, {keyFor(u"A"_s)}, lookup);
    ASSERT_EQ(1, initialSelection.selectedTracks.size());
    EXPECT_EQ(1, initialSelection.selectedTracks.front().id());

    const TrackList updatedTracks{
        makeTrack(2, u"/music/two.flac"_s),
        makeTrack(3, u"/music/three.flac"_s),
    };
    const Filters::FilterRowList updatedRows{
        makeRow(keyFor(u"B"_s), {3, 2}),
    };

    lookup.rebuildRows(updatedRows);

    const auto updatedSelection
        = Filters::resolveFilterSelection(updatedRows, updatedTracks, {keyFor(u"A"_s), keyFor(u"B"_s)}, lookup);
    ASSERT_EQ(1, updatedSelection.selectedKeys.size());
    EXPECT_EQ(keyFor(u"B"_s), updatedSelection.selectedKeys.front());
    ASSERT_EQ(2, updatedSelection.selectedTracks.size());
    EXPECT_EQ(3, updatedSelection.selectedTracks.at(0).id());
    EXPECT_EQ(2, updatedSelection.selectedTracks.at(1).id());
}

TEST(FilterPipelineTest, ResolveFilterSelectionUsesVisibleRowsForSummaryRow)
{
    const TrackList inputTracks{
        makeTrack(10, u"/music/all-a.flac"_s),
        makeTrack(20, u"/music/all-b.flac"_s),
        makeTrack(30, u"/music/hidden.flac"_s),
    };
    const Filters::FilterRowList rows{
        makeRow(keyFor(u"visible"_s), {20, 10}),
    };

    const Filters::FilterSelectionResolution selection
        = Filters::resolveFilterSelection(rows, inputTracks, {Filters::RowKey{}});

    EXPECT_TRUE(selection.isActive);
    ASSERT_EQ(2, selection.selectedTracks.size());
    EXPECT_EQ(10, selection.selectedTracks.at(0).id());
    EXPECT_EQ(20, selection.selectedTracks.at(1).id());
    ASSERT_EQ(1, selection.selectedKeys.size());
    EXPECT_TRUE(selection.selectedKeys.front().isEmpty());
}

TEST(FilterPipelineTest, ResolveFilterSelectionFallsBackToInputTracksForEmptySummaryRows)
{
    const TrackList inputTracks{
        makeTrack(10, u"/music/all-a.flac"_s),
        makeTrack(20, u"/music/all-b.flac"_s),
    };

    const Filters::FilterSelectionResolution selection
        = Filters::resolveFilterSelection({}, inputTracks, {Filters::RowKey{}});

    EXPECT_TRUE(selection.isActive);
    EXPECT_EQ(inputTracks, selection.selectedTracks);
    ASSERT_EQ(1, selection.selectedKeys.size());
    EXPECT_TRUE(selection.selectedKeys.front().isEmpty());
}

TEST(FilterPipelineTest, FilterRowsBySearchNarrowsMatchingTracksWithoutRebuildingColumns)
{
    const TrackList inputTracks{
        makeTrack(1, u"/music/one.flac"_s),
        makeTrack(2, u"/music/two.flac"_s),
        makeTrack(3, u"/music/three.flac"_s),
    };

    Filters::FilterRow groupedRow;
    groupedRow.key     = keyFor(u"grouped"_s);
    groupedRow.columns = {u"Shared Label"_s};
    groupedRow.tracks  = {inputTracks.at(0), inputTracks.at(1)};

    Filters::FilterRow singleRow;
    singleRow.key     = keyFor(u"single"_s);
    singleRow.columns = {u"Single Label"_s};
    singleRow.tracks  = {inputTracks.at(2)};

    const Filters::FilterRowList filteredRows
        = Filters::filterRowsBySearch(u"one"_s, {groupedRow, singleRow}, inputTracks);

    ASSERT_EQ(1, filteredRows.size());
    EXPECT_EQ(groupedRow.key, filteredRows.at(0).key);
    EXPECT_EQ(groupedRow.columns, filteredRows.at(0).columns);
    ASSERT_EQ(1, filteredRows.at(0).tracks.size());
    EXPECT_EQ(1, filteredRows.at(0).tracks.at(0).id());
}

TEST(FilterPipelineTest, FilterRowsBySearchKeepsCanonicalRowsWhenEveryTrackMatches)
{
    const TrackList inputTracks{
        makeTrack(1, u"/music/all-a.flac"_s),
        makeTrack(2, u"/music/all-b.flac"_s),
    };

    const Filters::FilterRowList rows{
        makeRow(keyFor(u"A"_s), {1}),
        makeRow(keyFor(u"B"_s), {2}),
    };

    const Filters::FilterRowList filteredRows = Filters::filterRowsBySearch(u"music"_s, rows, inputTracks);

    ASSERT_EQ(rows.size(), filteredRows.size());
    EXPECT_EQ(rows.at(0).key, filteredRows.at(0).key);
    EXPECT_EQ(rows.at(0).tracks, filteredRows.at(0).tracks);
    EXPECT_EQ(rows.at(1).key, filteredRows.at(1).key);
    EXPECT_EQ(rows.at(1).tracks, filteredRows.at(1).tracks);
}

TEST(FilterPipelineTest, PatchFilterRowsMovesUpdatedTrackBetweenBucketsAndPrunesEmptyRows)
{
    const Track oldRock = [] {
        Track track{u"/music/one.flac"_s};
        track.setId(1);
        track.setLibraryId(1);
        track.setGenres({u"Rock"_s});
        return track;
    }();
    const Track oldJazz = [] {
        Track track{u"/music/two.flac"_s};
        track.setId(2);
        track.setLibraryId(1);
        track.setGenres({u"Jazz"_s});
        return track;
    }();
    const Track newJazz = [] {
        Track track{u"/music/one.flac"_s};
        track.setId(1);
        track.setLibraryId(1);
        track.setGenres({u"Jazz"_s});
        return track;
    }();

    const Filters::FilterColumnList columns{
        {.id = 0, .name = u"Genre"_s, .field = u"%<genre>%"_s, .sortField = {}},
    };
    const Filters::FilterRowBuildContext context{
        .font          = {},
        .ratingSymbols = {u"*"_s, u"/"_s, u"-"_s},
        .useVarious    = false,
    };

    const Filters::FilterRowList previousRows = Filters::buildFilterRows(nullptr, columns, {oldRock, oldJazz}, context);
    const Filters::FilterRowList patchedRows  = Filters::patchFilterRows(
        nullptr, columns, previousRows, {oldRock, oldJazz}, {newJazz, oldJazz}, {1}, context);

    ASSERT_EQ(1, patchedRows.size());
    EXPECT_EQ(QStringList{u"Jazz"_s}, patchedRows.at(0).columns);
    ASSERT_EQ(2, patchedRows.at(0).tracks.size());
    EXPECT_EQ(1, patchedRows.at(0).tracks.at(0).id());
    EXPECT_EQ(2, patchedRows.at(0).tracks.at(1).id());
}

TEST(FilterPipelineTest, PatchFilterRowsRefreshesTrackMetadataWithinExistingBucket)
{
    Track oldTrack{u"/music/one.flac"_s};
    oldTrack.setId(1);
    oldTrack.setLibraryId(1);
    oldTrack.setGenres({u"Rock"_s});
    oldTrack.setTitle(u"Old title"_s);

    Track updatedTrack = oldTrack;
    updatedTrack.setTitle(u"Updated title"_s);

    const Filters::FilterColumnList columns{
        {.id = 0, .name = u"Genre"_s, .field = u"%<genre>%"_s, .sortField = {}},
    };
    const Filters::FilterRowBuildContext context{
        .font          = {},
        .ratingSymbols = {u"*"_s, u"/"_s, u"-"_s},
        .useVarious    = false,
    };

    const Filters::FilterRowList previousRows = Filters::buildFilterRows(nullptr, columns, {oldTrack}, context);
    const Filters::FilterRowList patchedRows
        = Filters::patchFilterRows(nullptr, columns, previousRows, {oldTrack}, {updatedTrack}, {1}, context);

    ASSERT_EQ(1, patchedRows.size());
    ASSERT_EQ(1, patchedRows.front().tracks.size());
    EXPECT_EQ(u"Updated title"_s, patchedRows.front().tracks.front().title());
}

TEST(FilterPipelineTest, PatchFilterRowsAddsNewTrackIntoExistingBucketInCurrentOrder)
{
    const Track oldRock = [] {
        Track track{u"/music/one.flac"_s};
        track.setId(1);
        track.setLibraryId(1);
        track.setGenres({u"Rock"_s});
        return track;
    }();
    const Track newRock = [] {
        Track track{u"/music/two.flac"_s};
        track.setId(2);
        track.setLibraryId(1);
        track.setGenres({u"Rock"_s});
        return track;
    }();

    const Filters::FilterColumnList columns{
        {.id = 0, .name = u"Genre"_s, .field = u"%<genre>%"_s, .sortField = {}},
    };
    const Filters::FilterRowBuildContext context{
        .font          = {},
        .ratingSymbols = {u"*"_s, u"/"_s, u"-"_s},
        .useVarious    = false,
    };

    const Filters::FilterRowList previousRows = Filters::buildFilterRows(nullptr, columns, {oldRock}, context);
    const Filters::FilterRowList patchedRows
        = Filters::patchFilterRows(nullptr, columns, previousRows, {oldRock}, {newRock, oldRock}, {2}, context);

    ASSERT_EQ(1, patchedRows.size());
    EXPECT_EQ(QStringList{u"Rock"_s}, patchedRows.at(0).columns);
    ASSERT_EQ(2, patchedRows.at(0).tracks.size());
    EXPECT_EQ(2, patchedRows.at(0).tracks.at(0).id());
    EXPECT_EQ(1, patchedRows.at(0).tracks.at(1).id());
}

TEST(FilterPipelineTest, RunFilterPipelinePropagatesUpstreamSelectionToDownstreamInputs)
{
    const TrackList sourceTracks{
        makeTrack(1, u"/music/one.flac"_s),
        makeTrack(2, u"/music/two.flac"_s),
        makeTrack(3, u"/music/three.flac"_s),
        makeTrack(4, u"/music/four.flac"_s),
    };

    const std::vector<Filters::FilterPipelineStageRequest> requests{
        {.selectedKeys = {keyFor(u"group-a"_s)}},
        {.selectedKeys = {}},
    };

    const Filters::FilterPipelineResult result
        = Filters::runFilterPipeline(sourceTracks, requests, [](int stageIndex, const TrackList& inputTracks) {
              if(stageIndex == 0) {
                  return Filters::FilterRowList{
                      makeRow(keyFor(u"group-a"_s), {1, 2}),
                      makeRow(keyFor(u"group-b"_s), {3, 4}),
                  };
              }

              Filters::FilterRow row;
              row.key    = keyFor(u"downstream"_s);
              row.tracks = inputTracks;
              return Filters::FilterRowList{row};
          });

    ASSERT_EQ(2, result.stages.size());
    EXPECT_EQ(sourceTracks, result.stages.at(0).inputTracks);
    ASSERT_EQ(2, result.stages.at(0).selectedTracks.size());
    EXPECT_EQ(1, result.stages.at(0).selectedTracks.at(0).id());
    EXPECT_EQ(2, result.stages.at(0).selectedTracks.at(1).id());

    ASSERT_EQ(2, result.stages.at(1).inputTracks.size());
    EXPECT_EQ(1, result.stages.at(1).inputTracks.at(0).id());
    EXPECT_EQ(2, result.stages.at(1).inputTracks.at(1).id());

    EXPECT_TRUE(result.hasActiveStages);
    EXPECT_EQ(result.stages.at(0).selectedTracks, result.finalFilteredTracks);
}

TEST(FilterPipelineTest, RunFilterPipelineWithNoActiveStagesLeavesFinalFilteredTracksEmpty)
{
    const TrackList sourceTracks{
        makeTrack(1, u"/music/one.flac"_s),
        makeTrack(2, u"/music/two.flac"_s),
    };

    const std::vector<Filters::FilterPipelineStageRequest> requests{
        {.selectedKeys = {}},
        {.selectedKeys = {}},
    };

    const Filters::FilterPipelineResult result
        = Filters::runFilterPipeline(sourceTracks, requests, [](int stageIndex, const TrackList& inputTracks) {
              Q_UNUSED(stageIndex);
              Filters::FilterRow row;
              row.key    = keyFor(u"all"_s);
              row.tracks = inputTracks;
              return Filters::FilterRowList{row};
          });

    ASSERT_EQ(2, result.stages.size());
    EXPECT_FALSE(result.stages.at(0).isActive);
    EXPECT_FALSE(result.stages.at(1).isActive);
    EXPECT_EQ(sourceTracks, result.stages.at(0).inputTracks);
    EXPECT_EQ(sourceTracks, result.stages.at(1).inputTracks);
    EXPECT_FALSE(result.hasActiveStages);
    EXPECT_TRUE(result.finalFilteredTracks.empty());
}

TEST(FilterPipelineTest, RunFilterPipelinePrunesStaleSelectionAndRemovesConstraint)
{
    const TrackList sourceTracks{
        makeTrack(1, u"/music/one.flac"_s),
        makeTrack(2, u"/music/two.flac"_s),
    };

    const std::vector<Filters::FilterPipelineStageRequest> requests{
        {.selectedKeys = {keyFor(u"stale"_s)}},
        {.selectedKeys = {}},
    };

    const Filters::FilterPipelineResult result
        = Filters::runFilterPipeline(sourceTracks, requests, [](int stageIndex, const TrackList& inputTracks) {
              if(stageIndex == 0) {
                  Q_UNUSED(inputTracks);
                  return Filters::FilterRowList{makeRow(keyFor(u"fresh"_s), {1})};
              }

              Filters::FilterRow row;
              row.key    = keyFor(u"downstream"_s);
              row.tracks = inputTracks;
              return Filters::FilterRowList{row};
          });

    ASSERT_EQ(2, result.stages.size());
    EXPECT_TRUE(result.stages.at(0).selectedKeys.empty());
    EXPECT_FALSE(result.stages.at(0).isActive);
    EXPECT_EQ(sourceTracks, result.stages.at(1).inputTracks);
    EXPECT_FALSE(result.hasActiveStages);
    EXPECT_TRUE(result.finalFilteredTracks.empty());
}

TEST(FilterPipelineTest, RunFilterPipelinePropagatesEmptySelectionAsEmptyConstraint)
{
    const TrackList sourceTracks{
        makeTrack(1, u"/music/one.flac"_s),
        makeTrack(2, u"/music/two.flac"_s),
    };

    const std::vector<Filters::FilterPipelineStageRequest> requests{
        {.selectedKeys = {keyFor(u"empty-row"_s)}},
        {.selectedKeys = {}},
    };

    const Filters::FilterPipelineResult result
        = Filters::runFilterPipeline(sourceTracks, requests, [](int stageIndex, const TrackList& inputTracks) {
              if(stageIndex == 0) {
                  Q_UNUSED(inputTracks);
                  return Filters::FilterRowList{makeRow(keyFor(u"empty-row"_s), {})};
              }

              return Filters::FilterRowList{makeRow(keyFor(u"downstream"_s), {})};
          });

    ASSERT_EQ(2, result.stages.size());
    EXPECT_TRUE(result.stages.at(0).isActive);
    EXPECT_TRUE(result.stages.at(0).selectedTracks.empty());
    EXPECT_TRUE(result.stages.at(1).inputTracks.empty());
    EXPECT_TRUE(result.hasActiveStages);
    EXPECT_TRUE(result.finalFilteredTracks.empty());
}

TEST(FilterPipelineTest, RunFilterPipelineChainsMultipleActiveStagesLeftToRight)
{
    const TrackList sourceTracks{
        makeTrack(1, u"/music/one.flac"_s),
        makeTrack(2, u"/music/two.flac"_s),
        makeTrack(3, u"/music/three.flac"_s),
    };

    const std::vector<Filters::FilterPipelineStageRequest> requests{
        {.selectedKeys = {keyFor(u"artists-a"_s)}},
        {.selectedKeys = {keyFor(u"albums-a2"_s)}},
    };

    const Filters::FilterPipelineResult result
        = Filters::runFilterPipeline(sourceTracks, requests, [](int stageIndex, const TrackList& inputTracks) {
              if(stageIndex == 0) {
                  return Filters::FilterRowList{
                      makeRow(keyFor(u"artists-a"_s), {1, 2}),
                      makeRow(keyFor(u"artists-b"_s), {3}),
                  };
              }

              Filters::FilterRowList rows;
              rows.push_back(makeRow(keyFor(u"albums-a1"_s), {1}));
              rows.push_back(makeRow(keyFor(u"albums-a2"_s), {2}));

              const std::unordered_set<int> allowedIds = [&inputTracks]() {
                  std::unordered_set<int> ids;
                  for(const Track& track : inputTracks) {
                      ids.emplace(track.id());
                  }
                  return ids;
              }();

              for(Filters::FilterRow& row : rows) {
                  std::erase_if(row.tracks,
                                [&allowedIds](const Track& track) { return !allowedIds.contains(track.id()); });
              }

              return rows;
          });

    ASSERT_EQ(2, result.stages.size());
    ASSERT_EQ(2, result.stages.at(1).inputTracks.size());
    EXPECT_EQ(1, result.stages.at(1).inputTracks.at(0).id());
    EXPECT_EQ(2, result.stages.at(1).inputTracks.at(1).id());
    ASSERT_EQ(1, result.finalFilteredTracks.size());
    EXPECT_EQ(2, result.finalFilteredTracks.front().id());
}
} // namespace Fooyin::Testing
