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

TEST(FilterPipelineTest, ResolveFilterSelectionRejectsStaleRowLookup)
{
    const TrackList tracks{
        makeTrack(1, u"/music/one.flac"_s),
        makeTrack(2, u"/music/two.flac"_s),
    };
    const Filters::FilterRowList initialRows{
        makeRow(keyFor(u"B"_s), {2}),
        makeRow(keyFor(u"A"_s), {1}),
    };
    const Filters::FilterRowList reorderedRows{
        makeRow(keyFor(u"A"_s), {1}),
        makeRow(keyFor(u"B"_s), {2}),
    };

    Filters::FilterRowLookup lookup;
    lookup.rebuildRows(initialRows);

    const auto selection = Filters::resolveFilterSelection(reorderedRows, tracks, {keyFor(u"A"_s)}, lookup);

    EXPECT_FALSE(selection.isActive);
    EXPECT_TRUE(selection.selectedKeys.empty());
    EXPECT_TRUE(selection.selectedTracks.empty());
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

TEST(FilterPipelineTest, ResolveFilterSelectionKeepsEmptySummarySelectionActive)
{
    const TrackList inputTracks{
        makeTrack(10, u"/music/all-a.flac"_s),
        makeTrack(20, u"/music/all-b.flac"_s),
    };

    const Filters::FilterSelectionResolution selection
        = Filters::resolveFilterSelection({}, inputTracks, {Filters::RowKey{}});

    EXPECT_TRUE(selection.isActive);
    EXPECT_TRUE(selection.selectedTracks.empty());
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

    const Filters::FilterRowList filteredRows = Filters::filterRowsBySearch(
        u"one"_s, {groupedRow, singleRow}, inputTracks, TrackQueryFilter::defaultSearchOptions(), {}, {}, {});

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

    const Filters::FilterRowList filteredRows = Filters::filterRowsBySearch(
        u"music"_s, rows, inputTracks, TrackQueryFilter::defaultSearchOptions(), {}, {}, {});

    ASSERT_EQ(rows.size(), filteredRows.size());
    EXPECT_EQ(rows.at(0).key, filteredRows.at(0).key);
    EXPECT_EQ(rows.at(0).tracks, filteredRows.at(0).tracks);
    EXPECT_EQ(rows.at(1).key, filteredRows.at(1).key);
    EXPECT_EQ(rows.at(1).tracks, filteredRows.at(1).tracks);
}

TEST(FilterPipelineTest, SplitFunctionCreatesRowsFromSingleTagValue)
{
    Track track{u"/music/one.flac"_s};
    track.setId(1);
    track.setLibraryId(1);
    track.addExtraTag(u"TEST"_s, u"B; A"_s);

    const Filters::FilterColumnList columns{
        {.id = 0, .name = u"Test"_s, .field = u"$split(%test%,; )"_s, .sortField = {}},
    };
    const Filters::FilterRowBuildContext context{
        .font          = {},
        .ratingSymbols = {u"*"_s, u"/"_s, u"-"_s},
        .useVarious    = false,
    };

    const Filters::FilterRowList rows = Filters::buildFilterRows({}, columns, {track}, context);

    ASSERT_EQ(2, rows.size());
    EXPECT_EQ(QStringList{u"B"_s}, rows.at(0).columns);
    EXPECT_EQ(QStringList{u"A"_s}, rows.at(1).columns);
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
        {.id = 0, .name = u"Genre"_s, .field = u"Shared label"_s, .sortField = {}, .groupingField = u"%<genre>%"_s},
    };
    const Filters::FilterRowBuildContext context{
        .font          = {},
        .ratingSymbols = {u"*"_s, u"/"_s, u"-"_s},
        .useVarious    = false,
    };

    const Filters::FilterRowList previousRows = Filters::buildFilterRows({}, columns, {oldRock, oldJazz}, context);
    ASSERT_EQ(2, previousRows.size());
    EXPECT_NE(previousRows.at(0).key, previousRows.at(1).key);
    EXPECT_EQ(previousRows.at(0).columns, previousRows.at(1).columns);
    const Filters::FilterRowList patchedRows
        = Filters::patchFilterRows({}, columns, previousRows, {oldRock, oldJazz}, {newJazz, oldJazz}, {1}, context);

    ASSERT_EQ(1, patchedRows.size());
    EXPECT_EQ(previousRows.at(1).key, patchedRows.at(0).key);
    EXPECT_EQ(QStringList{u"Shared label"_s}, patchedRows.at(0).columns);
    EXPECT_EQ(QStringList{u"Jazz"_s}, patchedRows.at(0).sortColumns);
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
        {.id            = 0,
         .name          = u"Genre"_s,
         .field         = u"%title%"_s,
         .sortField     = u"%title%"_s,
         .groupingField = u"%<genre>%"_s},
    };
    const Filters::FilterRowBuildContext context{
        .font          = {},
        .ratingSymbols = {u"*"_s, u"/"_s, u"-"_s},
        .useVarious    = false,
    };

    const Filters::FilterRowList previousRows = Filters::buildFilterRows({}, columns, {oldTrack}, context);
    const Filters::FilterRowList patchedRows
        = Filters::patchFilterRows({}, columns, previousRows, {oldTrack}, {updatedTrack}, {1}, context);

    ASSERT_EQ(1, patchedRows.size());
    EXPECT_EQ(previousRows.front().key, patchedRows.front().key);
    EXPECT_EQ(QStringList{u"Updated title"_s}, patchedRows.front().columns);
    EXPECT_EQ(QStringList{u"Updated title"_s}, patchedRows.front().sortColumns);
    ASSERT_EQ(1, patchedRows.front().tracks.size());
    EXPECT_EQ(u"Updated title"_s, patchedRows.front().tracks.front().title());
}

TEST(FilterPipelineTest, PatchFilterRowsAddsNewTrackIntoExistingBucketInCurrentOrder)
{
    const Track oldRock = [] {
        Track track{u"/music/one.flac"_s};
        track.setId(1);
        track.setLibraryId(1);
        track.setGenres({u"Rock"_s, u"Jazz"_s});
        return track;
    }();
    const Track newRock = [] {
        Track track{u"/music/two.flac"_s};
        track.setId(2);
        track.setLibraryId(1);
        track.setGenres({u"Jazz"_s, u"Rock"_s});
        return track;
    }();

    const Filters::FilterColumnList columns{
        {.id            = 0,
         .name          = u"Genre"_s,
         .field         = u"$upper(%<genre>%)"_s,
         .sortField     = {},
         .groupingField = u"%<genre>%"_s},
    };
    const Filters::FilterRowBuildContext context{
        .font          = {},
        .ratingSymbols = {u"*"_s, u"/"_s, u"-"_s},
        .useVarious    = false,
    };

    const Filters::FilterRowList previousRows = Filters::buildFilterRows({}, columns, {oldRock}, context);
    const Filters::FilterRowList patchedRows
        = Filters::patchFilterRows({}, columns, previousRows, {oldRock}, {newRock, oldRock}, {2}, context);

    ASSERT_EQ(2, patchedRows.size());
    EXPECT_EQ(QStringList{u"JAZZ"_s}, patchedRows.at(0).columns);
    EXPECT_EQ(QStringList{u"ROCK"_s}, patchedRows.at(1).columns);
    EXPECT_EQ(QStringList{u"Jazz"_s}, patchedRows.at(0).sortColumns);
    EXPECT_EQ(QStringList{u"Rock"_s}, patchedRows.at(1).sortColumns);
    for(const auto& row : patchedRows) {
        ASSERT_EQ(2, row.tracks.size());
        EXPECT_EQ(2, row.tracks.at(0).id());
        EXPECT_EQ(1, row.tracks.at(1).id());
    }
}

TEST(FilterPipelineTest, FilterRowsBySearchUpdatesCountsAndKeepsOrder)
{
    TrackList libraryTracks{
        makeTrack(1, u"/music/one.flac"_s),
        makeTrack(2, u"/music/two.flac"_s),
        makeTrack(3, u"/music/three.flac"_s),
    };
    for(auto& track : libraryTracks) {
        track.setLibraryId(1);
        track.setDuration(static_cast<uint64_t>(track.id() + 1) * 1000);
    }
    libraryTracks.at(0).setGenres({u"Rock"_s, u"Jazz"_s});
    libraryTracks.at(1).setGenres({u"Jazz"_s, u"Rock"_s});
    libraryTracks.at(2).setGenres({u"Blues"_s});

    const Filters::FilterColumnList columns{
        {.id            = 0,
         .name          = u"Genre"_s,
         .field         = u"%<genre>% (%trackcount%)"_s,
         .sortField     = {},
         .groupingField = u"%<genre>%"_s},
        {.id            = 1,
         .name          = u"Duration"_s,
         .field         = u"%playtime%"_s,
         .sortField     = u"$num(%trackcount%,3)"_s,
         .groupingField = u"1"_s},
    };
    const Filters::FilterRowBuildContext context;
    const auto rows = Filters::buildFilterRows({}, columns, libraryTracks, context);
    ASSERT_EQ(3, rows.size());
    EXPECT_EQ((QStringList{u"Rock (2)"_s, u"00:05"_s}), rows.at(0).columns);

    const auto searchedRows = Filters::filterRowsBySearch(
        u"two"_s, rows, libraryTracks, TrackQueryFilter::defaultSearchOptions(), {}, columns, context);
    ASSERT_EQ(2, searchedRows.size());
    EXPECT_EQ(rows.at(0).key, searchedRows.at(0).key);
    EXPECT_EQ(rows.at(1).key, searchedRows.at(1).key);
    EXPECT_EQ((QStringList{u"Rock (1)"_s, u"00:03"_s}), searchedRows.at(0).columns);
    EXPECT_EQ((QStringList{u"Jazz (1)"_s, u"00:03"_s}), searchedRows.at(1).columns);
    EXPECT_EQ((QStringList{u"Rock"_s, u"001"_s}), searchedRows.at(0).sortColumns);
    EXPECT_EQ((QStringList{u"Jazz"_s, u"001"_s}), searchedRows.at(1).sortColumns);
    EXPECT_EQ(u"Rock (1)"_s, searchedRows.at(0).richColumns.at(0).joinedText());
}

TEST(FilterPipelineTest, UpdatingTracksUpdatesCountsWithoutCategories)
{
    Track track{u"/music/one.flac"_s};
    track.setId(1);
    track.setLibraryId(1);
    track.addExtraTag(u"TEST"_s, u"B; A"_s);

    const Filters::FilterColumnList columns{
        {.id = 0, .name = u"Test"_s, .field = u"$split(%test%,; ) (%trackcount%)"_s, .sortField = {}},
    };
    const Filters::FilterRowBuildContext context{
        .font          = {},
        .ratingSymbols = {u"*"_s, u"/"_s, u"-"_s},
        .useVarious    = false,
    };

    const Filters::FilterRowList rows = Filters::buildFilterRows({}, columns, {track}, context);

    ASSERT_EQ(2, rows.size());
    EXPECT_EQ(QStringList{u"B (1)"_s}, rows.at(0).columns);
    EXPECT_EQ(QStringList{u"A (1)"_s}, rows.at(1).columns);

    Track secondTrack = makeTrack(2, u"/music/two.flac"_s);
    secondTrack.setLibraryId(1);
    secondTrack.addExtraTag(u"TEST"_s, u"A; B"_s);
    const auto patchedRows = Filters::patchFilterRows({}, columns, rows, {track}, {secondTrack, track}, {2}, context);
    ASSERT_EQ(2, patchedRows.size());
    EXPECT_EQ(rows.at(1).key, patchedRows.at(0).key);
    EXPECT_EQ(rows.at(0).key, patchedRows.at(1).key);
    EXPECT_EQ(QStringList{u"A (2)"_s}, patchedRows.at(0).columns);
    EXPECT_EQ(QStringList{u"B (2)"_s}, patchedRows.at(1).columns);
    EXPECT_EQ(QStringList{u"A (2)"_s}, patchedRows.at(0).sortColumns);
}

TEST(FilterPipelineTest, MovingTracksBetweenGroupsUpdatesCounts)
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
        {.id            = 0,
         .name          = u"Genre"_s,
         .field         = u"Shared label (%trackcount%)"_s,
         .sortField     = {},
         .groupingField = u"%<genre>%"_s},
    };
    const Filters::FilterRowBuildContext context{
        .font          = {},
        .ratingSymbols = {u"*"_s, u"/"_s, u"-"_s},
        .useVarious    = false,
    };

    const Filters::FilterRowList previousRows = Filters::buildFilterRows({}, columns, {oldRock, oldJazz}, context);
    ASSERT_EQ(2, previousRows.size());
    EXPECT_NE(previousRows.at(0).key, previousRows.at(1).key);
    EXPECT_EQ(previousRows.at(0).columns, previousRows.at(1).columns);
    const Filters::FilterRowList patchedRows
        = Filters::patchFilterRows({}, columns, previousRows, {oldRock, oldJazz}, {newJazz, oldJazz}, {1}, context);

    ASSERT_EQ(1, patchedRows.size());
    EXPECT_EQ(previousRows.at(1).key, patchedRows.at(0).key);
    EXPECT_EQ(QStringList{u"Shared label (2)"_s}, patchedRows.at(0).columns);
    EXPECT_EQ(QStringList{u"Jazz"_s}, patchedRows.at(0).sortColumns);
    ASSERT_EQ(2, patchedRows.at(0).tracks.size());
    EXPECT_EQ(1, patchedRows.at(0).tracks.at(0).id());
    EXPECT_EQ(2, patchedRows.at(0).tracks.at(1).id());
}

TEST(FilterPipelineTest, EditingTrackDetailsRefreshesCalculations)
{
    Track oldTrack{u"/music/one.flac"_s};
    oldTrack.setId(1);
    oldTrack.setLibraryId(1);
    oldTrack.setGenres({u"Rock"_s});
    oldTrack.setTitle(u"Old title"_s);
    oldTrack.setDuration(2000);

    Track updatedTrack = oldTrack;
    updatedTrack.setTitle(u"Updated title"_s);
    updatedTrack.setDuration(3000);

    const Filters::FilterColumnList columns{
        {.id            = 0,
         .name          = u"Genre"_s,
         .field         = u"%title% (%trackcount%) [%playtime%]"_s,
         .sortField     = u"%playtime%"_s,
         .groupingField = u"%<genre>%"_s},
    };
    const Filters::FilterRowBuildContext context{
        .font          = {},
        .ratingSymbols = {u"*"_s, u"/"_s, u"-"_s},
        .useVarious    = false,
    };

    const Filters::FilterRowList previousRows = Filters::buildFilterRows({}, columns, {oldTrack}, context);
    const Filters::FilterRowList patchedRows
        = Filters::patchFilterRows({}, columns, previousRows, {oldTrack}, {updatedTrack}, {1}, context);

    ASSERT_EQ(1, patchedRows.size());
    EXPECT_EQ(previousRows.front().key, patchedRows.front().key);
    EXPECT_EQ(QStringList{u"Updated title (1) 00:03"_s}, patchedRows.front().columns);
    EXPECT_EQ(QStringList{u"00:03"_s}, patchedRows.front().sortColumns);
    ASSERT_EQ(1, patchedRows.front().tracks.size());
    EXPECT_EQ(u"Updated title"_s, patchedRows.front().tracks.front().title());
}

TEST(FilterPipelineTest, AddingAndRemovingTracksUpdatesMultiCategoryCounts)
{
    const Track oldRock = [] {
        Track track{u"/music/one.flac"_s};
        track.setId(1);
        track.setLibraryId(1);
        track.setGenres({u"Rock"_s, u"Jazz"_s});
        track.setDuration(2000);
        track.setBitrate(100);
        return track;
    }();
    const Track newRock = [] {
        Track track{u"/music/two.flac"_s};
        track.setId(2);
        track.setLibraryId(1);
        track.setGenres({u"Jazz"_s, u"Rock"_s});
        track.setDuration(3000);
        track.setBitrate(200);
        return track;
    }();

    const Filters::FilterColumnList columns{
        {.id            = 0,
         .name          = u"Genre"_s,
         .field         = u"$upper(%<genre>%) (%trackcount% $ifequal(%trackcount%,1,track,tracks))"_s,
         .sortField     = {},
         .groupingField = u"%<genre>%"_s},
        {.id            = 1,
         .name          = u"Duration"_s,
         .field         = u"%playtime% | %avgbitrate%"_s,
         .sortField     = u"$num(%trackcount%,3)"_s,
         .groupingField = u"1"_s},
    };
    const Filters::FilterRowBuildContext context{
        .font          = {},
        .ratingSymbols = {u"*"_s, u"/"_s, u"-"_s},
        .useVarious    = false,
    };

    const Filters::FilterRowList previousRows = Filters::buildFilterRows({}, columns, {oldRock}, context);
    const Filters::FilterRowList patchedRows
        = Filters::patchFilterRows({}, columns, previousRows, {oldRock}, {newRock, oldRock}, {2}, context);

    ASSERT_EQ(2, patchedRows.size());
    EXPECT_EQ((QStringList{u"JAZZ (2 tracks)"_s, u"00:05 | 160"_s}), patchedRows.at(0).columns);
    EXPECT_EQ((QStringList{u"ROCK (2 tracks)"_s, u"00:05 | 160"_s}), patchedRows.at(1).columns);
    EXPECT_EQ((QStringList{u"Jazz"_s, u"002"_s}), patchedRows.at(0).sortColumns);
    EXPECT_EQ((QStringList{u"Rock"_s, u"002"_s}), patchedRows.at(1).sortColumns);
    for(const auto& row : patchedRows) {
        ASSERT_EQ(2, row.tracks.size());
        EXPECT_EQ(2, row.tracks.at(0).id());
        EXPECT_EQ(1, row.tracks.at(1).id());
    }

    const auto removedRows
        = Filters::patchFilterRows({}, columns, patchedRows, {newRock, oldRock}, {oldRock}, {}, context);
    ASSERT_EQ(2, removedRows.size());
    EXPECT_EQ(previousRows.at(0).key, removedRows.at(0).key);
    EXPECT_EQ((QStringList{u"ROCK (1 track)"_s, u"00:02 | 100"_s}), removedRows.at(0).columns);
    EXPECT_EQ((QStringList{u"Rock"_s, u"001"_s}), removedRows.at(0).sortColumns);
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
