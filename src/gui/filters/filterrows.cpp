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

#include "filterrows.h"

#include <core/constants.h>
#include <core/library/librarymanager.h>
#include <core/scripting/scriptparser.h>
#include <core/scripting/scripttypes.h>
#include <core/scripting/trackqueryfilter.h>
#include <gui/scripting/richtextutils.h>
#include <gui/scripting/scriptformatter.h>
#include <utils/crypto.h>

#include <algorithm>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <utility>

using namespace Qt::StringLiterals;

namespace Fooyin::Filters {
namespace {
struct ColumnData
{
    QStringList plainColumns;
    std::vector<RichText> richColumns;
};

struct CachedColumnData
{
    ColumnData columns;
    RowKey key;
};

struct RowScriptSource
{
    size_t valueIndex;
    size_t valueCount;
    QStringList groupingColumns;
};

bool usesRowVariables(ScriptParser& parser, const ParsedScript& script)
{
    const auto dependencies = parser.dependencies(script);
    return dependencies.unclassified || dependencies.contains(ScriptSources::Playlist);
}

bool usesRowVariables(const FilterColumnList& columns)
{
    if(columns.empty()) {
        return false;
    }

    ScriptParser parser;
    return std::ranges::any_of(columns, [&parser](const FilterColumn& column) {
        return usesRowVariables(parser, parser.parse(column.field))
            || usesRowVariables(parser, parser.parse(column.groupingField))
            || usesRowVariables(parser, parser.parse(column.sortField));
    });
}

ColumnData buildColumnData(const QStringList& columns, ScriptFormatter& formatter)
{
    ColumnData data;
    data.plainColumns.reserve(columns.size());
    data.richColumns.reserve(columns.size());

    for(const QString& column : columns) {
        RichText richColumn = trimRichText(formatter.evaluate(column));
        QString plainColumn = richColumn.joinedText();

        data.plainColumns.push_back(std::move(plainColumn));
        data.richColumns.push_back(std::move(richColumn));
    }

    return data;
}

QString sortColumnText(const QString& column, ScriptFormatter& formatter)
{
    return trimRichText(formatter.evaluate(column)).joinedText();
}

std::vector<QStringList> splitColumnValues(const QString& evaluated)
{
    std::vector<QStringList> values;

    if(evaluated.contains(QLatin1String{Constants::UnitSeparator})) {
        const QStringList rows = evaluated.split(QLatin1String{Constants::UnitSeparator});
        values.reserve(rows.size());
        for(const QString& row : rows) {
            values.push_back(row.split(QLatin1String{Constants::RecordSeparator}));
        }
    }
    else {
        values.push_back(evaluated.split(QLatin1String{Constants::RecordSeparator}));
    }

    return values;
}
} // namespace

FilterRowList buildFilterRows(LibraryManager* libraryManager, const FilterColumnList& columns, const TrackList& tracks,
                              const FilterRowBuildContext& context)
{
    return buildFilterRows(libraryManager ? libraryManager->allLibraries() : LibraryInfoMap{}, columns, tracks,
                           context);
}

FilterRowList buildFilterRows(const LibraryInfoMap& libraries, const FilterColumnList& columns, const TrackList& tracks,
                              const FilterRowBuildContext& context)
{
    FilterRowList rows;
    if(columns.empty() || tracks.empty()) {
        return rows;
    }

    QStringList fields;
    fields.reserve(static_cast<qsizetype>(columns.size()));

    std::ranges::transform(columns, std::back_inserter(fields),
                           [](const FilterColumn& column) { return column.field; });

    const bool hasCustomGroupingField
        = std::ranges::any_of(columns, [](const FilterColumn& column) { return !column.groupingField.isEmpty(); });
    QStringList groupingFields;
    if(hasCustomGroupingField) {
        groupingFields.reserve(static_cast<qsizetype>(columns.size()));
        std::ranges::transform(columns, std::back_inserter(groupingFields), [](const FilterColumn& column) {
            return column.groupingField.isEmpty() ? column.field : column.groupingField;
        });
    }

    const bool hasCustomSortField
        = std::ranges::any_of(columns, [](const FilterColumn& column) { return !column.sortField.isEmpty(); });

    QStringList sortFields;

    sortFields.reserve(static_cast<qsizetype>(columns.size()));
    std::ranges::transform(columns, std::back_inserter(sortFields), [](const FilterColumn& column) {
        if(!column.sortField.isEmpty()) {
            return column.sortField;
        }
        return column.groupingField.isEmpty() ? column.field : column.groupingField;
    });

    ScriptParser parser;
    ScriptFormatter formatter;
    formatter.setBaseFont(context.font);

    const ParsedScript script = parser.parse(fields.join(QLatin1StringView{Constants::RecordSeparator}));
    const ParsedScript groupingScript
        = hasCustomGroupingField ? parser.parse(groupingFields.join(QLatin1StringView{Constants::RecordSeparator}))
                                 : script;
    const ScriptContext scriptContext{.libraries = &libraries};

    std::unordered_map<RowKey, size_t> rowIndexes;
    std::unordered_map<QString, CachedColumnData> columnDataCache;

    const auto cachedColumnData = [&columnDataCache, &formatter](const QStringList& values) -> const CachedColumnData& {
        const QString cacheKey = values.join(QLatin1StringView{Constants::RecordSeparator});
        auto [it, inserted]    = columnDataCache.try_emplace(cacheKey);
        if(inserted) {
            it->second.columns = buildColumnData(values, formatter);
            it->second.key     = Utils::generateMd5Hash(it->second.columns.plainColumns.join(QString{}));
        }
        return it->second;
    };

    const ParsedScript sortScript     = parser.parse(sortFields.join(QLatin1StringView{Constants::RecordSeparator}));
    const bool hasRowDisplayVariables = usesRowVariables(parser, script);
    std::vector<int> rowSortColumns;
    for(int column{0}; column < sortFields.size(); ++column) {
        if(usesRowVariables(parser, parser.parse(sortFields.at(column)))) {
            rowSortColumns.push_back(column);
        }
    }

    const bool hasRowVariables = hasRowDisplayVariables || !rowSortColumns.empty();
    std::vector<RowScriptSource> rowScriptSources;
    const ScriptEvaluationOptions displayOptions{.missingVariableText    = u"?"_s,
                                                 .trackListContextPolicy = TrackListContextPolicy::Placeholder,
                                                 .trackListPlaceholder   = u"1"_s,
                                                 .escapeRichText         = true,
                                                 .useVariousArtists      = context.useVarious,
                                                 .ratingSymbols          = context.ratingSymbols};

    auto sortOptions{displayOptions};
    sortOptions.missingVariableText.clear();

    for(const Track& track : tracks) {
        if(!track.isInLibrary()) {
            continue;
        }

        const QString evaluated                     = parser.evaluate(script, track, scriptContext, displayOptions);
        const std::vector<QStringList> columnValues = splitColumnValues(evaluated);
        const auto groupingValues
            = hasCustomGroupingField
                ? splitColumnValues(parser.evaluate(groupingScript, track, scriptContext, displayOptions))
                : columnValues;

        std::vector<QStringList> sortValues;
        if(hasCustomSortField) {
            sortValues = splitColumnValues(parser.evaluate(sortScript, track, scriptContext, sortOptions));
        }

        auto addColumns = [&](const QStringList& rowGroupingValues, const QStringList& rowColumnValues,
                              const QStringList& sortColumnValues, size_t valueIndex) {
            const auto& groupingData = cachedColumnData(rowGroupingValues);
            if(std::ranges::all_of(groupingData.columns.plainColumns,
                                   [](const QString& column) { return column.isEmpty(); })) {
                return;
            }
            const RowKey& key = groupingData.key;

            auto [it, inserted] = rowIndexes.try_emplace(key, rows.size());
            if(inserted) {
                rows.emplace_back();
            }
            FilterRow& row = rows.at(it->second);

            if(inserted) {
                const ColumnData& columnData = cachedColumnData(rowColumnValues).columns;
                row.key                      = key;
                row.columns                  = columnData.plainColumns;
                row.richColumns              = columnData.richColumns;
                if(hasRowVariables) {
                    rowScriptSources.push_back({.valueIndex      = valueIndex,
                                                .valueCount      = groupingValues.size(),
                                                .groupingColumns = groupingData.columns.plainColumns});
                }

                if(hasCustomSortField || hasCustomGroupingField) {
                    const bool pairSortColumns = sortColumnValues.size() == rowColumnValues.size();
                    row.sortColumns.reserve(columnData.plainColumns.size());
                    for(int column{0}; column < columnData.plainColumns.size(); ++column) {
                        if(!pairSortColumns || std::cmp_greater_equal(column, columns.size())
                           || columns.at(column).sortField.isEmpty()) {
                            row.sortColumns.push_back(groupingData.columns.plainColumns.value(column));
                        }
                        else {
                            row.sortColumns.push_back(sortColumnText(sortColumnValues.value(column), formatter));
                        }
                    }
                }
            }

            row.tracks.push_back(track);
        };

        const bool pairDisplayRows = columnValues.size() == groupingValues.size();
        const bool pairSortRows    = hasCustomSortField && groupingValues.size() == sortValues.size();
        for(int index{0}; std::cmp_less(index, groupingValues.size()); ++index) {
            QStringList rowSortValues;
            if(pairSortRows) {
                rowSortValues = sortValues.at(index);
            }
            addColumns(groupingValues.at(index), columnValues.at(pairDisplayRows ? index : 0), rowSortValues,
                       static_cast<size_t>(index));
        }
    }

    if(hasRowVariables) {
        auto rowDisplayOptions{displayOptions};
        rowDisplayOptions.trackListContextPolicy = TrackListContextPolicy::Fallback;
        auto rowSortOptions{sortOptions};
        rowSortOptions.trackListContextPolicy = TrackListContextPolicy::Fallback;

        for(size_t index{0}; index < rows.size(); ++index) {
            auto& row          = rows.at(index);
            const auto& source = rowScriptSources.at(index);
            const auto& track  = row.tracks.front();
            auto rowContext    = scriptContext;
            rowContext.tracks  = &row.tracks;

            if(hasRowDisplayVariables) {
                const auto values = splitColumnValues(parser.evaluate(script, track, rowContext, rowDisplayOptions));
                const auto& value = values.at(values.size() == source.valueCount ? source.valueIndex : 0);
                auto data         = buildColumnData(value, formatter);
                row.columns       = std::move(data.plainColumns);
                row.richColumns   = std::move(data.richColumns);
            }

            if(!rowSortColumns.empty()) {
                if(row.sortColumns.isEmpty()) {
                    row.sortColumns = source.groupingColumns;
                }
                row.sortColumns.resize(row.columns.size());

                const auto values = splitColumnValues(parser.evaluate(sortScript, track, rowContext, rowSortOptions));
                const bool pairSortRows = values.size() == source.valueCount;
                for(const int column : rowSortColumns) {
                    if(column >= row.sortColumns.size()) {
                        continue;
                    }
                    row.sortColumns[column] = pairSortRows && values.at(source.valueIndex).size() == row.columns.size()
                                                ? sortColumnText(values.at(source.valueIndex).value(column), formatter)
                                                : source.groupingColumns.value(column);
                }
            }
        }
    }

    return rows;
}

FilterRowList patchFilterRows(LibraryManager* libraryManager, const FilterColumnList& columns,
                              const FilterRowList& previousRows, const TrackList& previousTracks,
                              const TrackList& tracks, const TrackIds& changedTrackIds,
                              const FilterRowBuildContext& context)
{
    return patchFilterRows(libraryManager ? libraryManager->allLibraries() : LibraryInfoMap{}, columns, previousRows,
                           previousTracks, tracks, changedTrackIds, context);
}

FilterRowList patchFilterRows(const LibraryInfoMap& libraries, const FilterColumnList& columns,
                              const FilterRowList& previousRows, const TrackList& previousTracks,
                              const TrackList& tracks, const TrackIds& changedTrackIds,
                              const FilterRowBuildContext& context)
{
    if(columns.empty() || tracks.empty()) {
        return {};
    }

    if(previousRows.empty() || previousTracks.empty()) {
        return buildFilterRows(libraries, columns, tracks, context);
    }

    if(changedTrackIds.empty() && previousTracks == tracks) {
        return previousRows;
    }

    const bool hasCustomScripts = std::ranges::any_of(columns, [](const FilterColumn& column) {
        return !column.sortField.isEmpty() || !column.groupingField.isEmpty();
    });
    if(hasCustomScripts || usesRowVariables(columns)) {
        return buildFilterRows(libraries, columns, tracks, context);
    }

    std::unordered_map<int, Track> previousTracksById;
    previousTracksById.reserve(previousTracks.size());
    for(const Track& track : previousTracks) {
        previousTracksById.emplace(track.id(), track);
    }

    std::unordered_map<int, Track> tracksById;
    tracksById.reserve(tracks.size());
    for(const Track& track : tracks) {
        tracksById.emplace(track.id(), track);
    }

    std::unordered_set<int> changedTrackIdSet;
    changedTrackIdSet.reserve(changedTrackIds.size());
    for(int trackId : changedTrackIds) {
        changedTrackIdSet.emplace(trackId);
    }

    TrackList changedTracks;
    changedTracks.reserve(tracks.size());

    std::unordered_set<int> removedTrackIds;
    removedTrackIds.reserve(previousTracks.size());

    for(const Track& previousTrack : previousTracks) {
        const auto trackIt = tracksById.find(previousTrack.id());
        if(trackIt == tracksById.cend()) {
            removedTrackIds.emplace(previousTrack.id());
            continue;
        }

        if(changedTrackIdSet.contains(previousTrack.id())) {
            removedTrackIds.emplace(previousTrack.id());
            changedTracks.push_back(trackIt->second);
        }
    }

    for(const Track& track : tracks) {
        if(!previousTracksById.contains(track.id())) {
            changedTracks.push_back(track);
        }
    }

    if(removedTrackIds.empty() && changedTracks.empty() && previousTracks == tracks) {
        return previousRows;
    }

    std::unordered_map<RowKey, FilterRow> patchedRows;
    patchedRows.reserve(previousRows.size());

    for(const FilterRow& previousRow : previousRows) {
        FilterRow row = previousRow;
        std::erase_if(row.tracks,
                      [&removedTrackIds](const Track& track) { return removedTrackIds.contains(track.id()); });

        if(!row.tracks.empty()) {
            patchedRows.emplace(row.key, std::move(row));
        }
    }

    for(FilterRow row : buildFilterRows(libraries, columns, changedTracks, context)) {
        auto [it, inserted] = patchedRows.try_emplace(row.key, std::move(row));
        if(!inserted) {
            it->second.columns     = row.columns;
            it->second.sortColumns = row.sortColumns;
            it->second.richColumns = row.richColumns;
            std::ranges::copy(row.tracks, std::back_inserter(it->second.tracks));
        }
    }

    std::unordered_map<int, int> trackPositions;
    trackPositions.reserve(tracks.size());
    for(int index{0}; std::cmp_less(index, tracks.size()); ++index) {
        trackPositions.emplace(tracks.at(index).id(), index);
    }

    std::unordered_map<int, std::vector<RowKey>> rowsByFirstTrack;
    for(auto& [key, row] : patchedRows) {
        std::ranges::stable_sort(row.tracks, [&trackPositions](const Track& left, const Track& right) {
            return trackPositions.at(left.id()) < trackPositions.at(right.id());
        });
        rowsByFirstTrack[row.tracks.front().id()].push_back(key);
    }

    FilterRowList rows;
    rows.reserve(patchedRows.size());

    for(const Track& track : tracks) {
        auto firstTrackRows = rowsByFirstTrack.extract(track.id());
        if(firstTrackRows.empty()) {
            continue;
        }

        if(firstTrackRows.mapped().size() == 1) {
            auto row = patchedRows.extract(firstTrackRows.mapped().front());
            rows.push_back(std::move(row.mapped()));
            continue;
        }

        const auto filterRows = buildFilterRows(libraries, columns, {track}, context);
        for(const FilterRow& trackRow : filterRows) {
            const auto row = patchedRows.find(trackRow.key);
            if(row != patchedRows.cend() && row->second.tracks.front().id() == track.id()) {
                rows.push_back(std::move(row->second));
                patchedRows.erase(row);
            }
        }
    }

    if(!patchedRows.empty()) {
        return buildFilterRows(libraries, columns, tracks, context);
    }

    return rows;
}

TrackList filterTracksBySearch(const QString& search, const TrackList& tracks)
{
    if(search.isEmpty() || tracks.empty()) {
        return tracks;
    }

    TrackQueryFilter filter;
    return filter.filter(search, tracks);
}

FilterRowList filterRowsBySearch(const QString& search, const FilterRowList& rows, const TrackList& tracks)
{
    return filterRowsBySearch(search, rows, tracks, TrackQueryFilter::defaultSearchOptions());
}

FilterRowList filterRowsBySearch(const QString& search, const FilterRowList& rows, const TrackList& tracks,
                                 const ScriptSearchOptions& searchOptions)
{
    return filterRowsBySearch(search, rows, tracks, searchOptions, {}, {}, {});
}

FilterRowList filterRowsBySearch(const QString& search, const FilterRowList& rows, const TrackList& tracks,
                                 const ScriptSearchOptions& searchOptions, const LibraryInfoMap& libraries,
                                 const FilterColumnList& columns, const FilterRowBuildContext& context)
{
    if(search.isEmpty() || rows.empty()) {
        return rows;
    }

    TrackQueryFilter filter{searchOptions};
    const TrackList filteredTracks = filter.filter(search, tracks);
    if(filteredTracks.empty()) {
        return {};
    }

    if(filteredTracks.size() == tracks.size()) {
        return rows;
    }

    if(usesRowVariables(columns)) {
        auto rebuiltRows = buildFilterRows(libraries, columns, filteredTracks, context);
        std::unordered_map<RowKey, size_t> rowIndexes;
        for(size_t index{0}; index < rebuiltRows.size(); ++index) {
            rowIndexes.emplace(rebuiltRows.at(index).key, index);
        }

        FilterRowList filteredRows;
        filteredRows.reserve(rebuiltRows.size());
        for(const auto& row : rows) {
            if(const auto it = rowIndexes.find(row.key); it != rowIndexes.end()) {
                filteredRows.push_back(std::move(rebuiltRows.at(it->second)));
            }
        }
        return filteredRows;
    }

    std::unordered_set<int> matchingTrackIds;
    matchingTrackIds.reserve(filteredTracks.size());

    for(const Track& track : filteredTracks) {
        matchingTrackIds.emplace(track.id());
    }

    FilterRowList filteredRows;
    filteredRows.reserve(rows.size());

    for(const FilterRow& row : rows) {
        TrackList matchedTracks;
        matchedTracks.reserve(row.tracks.size());

        for(const Track& track : row.tracks) {
            if(matchingTrackIds.contains(track.id())) {
                matchedTracks.push_back(track);
            }
        }

        if(matchedTracks.empty()) {
            continue;
        }

        FilterRow filteredRow = row;
        filteredRow.tracks    = std::move(matchedTracks);
        filteredRows.push_back(std::move(filteredRow));
    }

    return filteredRows;
}
} // namespace Fooyin::Filters
