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
#include <core/scripting/scriptenvironmenthelpers.h>
#include <core/scripting/scriptparser.h>
#include <gui/scripting/richtextutils.h>
#include <gui/scripting/scriptformatter.h>
#include <utils/crypto.h>

#include <algorithm>
#include <map>
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

RichText sortRichText(const QString& text)
{
    RichText richText;
    if(!text.isEmpty()) {
        richText.blocks.push_back({.text = text, .format = {}});
    }
    return richText;
}

RichText placeholderRichText()
{
    RichText richText;
    richText.blocks.push_back({.text = u"?"_s, .format = {}});
    return richText;
}

ColumnData buildColumnData(const QStringList& columns, ScriptFormatter& formatter)
{
    ColumnData data;
    data.plainColumns.reserve(columns.size());
    data.richColumns.reserve(columns.size());

    for(const QString& column : columns) {
        RichText richColumn = trimRichText(formatter.evaluate(column));
        QString plainColumn = richColumn.joinedText();

        if(plainColumn.isEmpty()) {
            RichText placeholder = trimRichText(formatter.evaluate(column + u"?"_s));
            if(!placeholder.empty()) {
                richColumn = std::move(placeholder);
            }
            else {
                richColumn = placeholderRichText();
            }
            plainColumn = richColumn.joinedText();
        }

        data.plainColumns.push_back(plainColumn);
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
    FilterRowList rows;
    if(columns.empty() || tracks.empty()) {
        return rows;
    }

    QStringList fields;
    fields.reserve(columns.size());
    std::ranges::transform(columns, std::back_inserter(fields),
                           [](const FilterColumn& column) { return column.field; });

    const bool hasCustomSortField
        = std::ranges::any_of(columns, [](const FilterColumn& column) { return !column.sortField.isEmpty(); });

    QStringList sortFields;
    if(hasCustomSortField) {
        sortFields.reserve(columns.size());
        std::ranges::transform(columns, std::back_inserter(sortFields), [](const FilterColumn& column) {
            return column.sortField.isEmpty() ? column.field : column.sortField;
        });
    }

    ScriptParser parser;
    ScriptFormatter formatter;
    formatter.setBaseFont(context.font);

    LibraryScriptEnvironment scriptEnvironment{libraryManager};
    scriptEnvironment.setRatingStarSymbols(context.ratingSymbols);
    scriptEnvironment.setEvaluationPolicy(TrackListContextPolicy::Unresolved, {}, false, context.useVarious);

    const ParsedScript script = parser.parse(fields.join("\036"_L1));
    ScriptContext scriptContext;
    scriptContext.environment = &scriptEnvironment;

    std::map<RowKey, FilterRow> items;
    const ParsedScript sortScript = hasCustomSortField ? parser.parse(sortFields.join("\036"_L1)) : ParsedScript{};

    for(const Track& track : tracks) {
        if(!track.isInLibrary()) {
            continue;
        }

        const QString evaluated                     = parser.evaluate(script, track, scriptContext);
        const std::vector<QStringList> columnValues = splitColumnValues(evaluated);

        std::vector<QStringList> sortValues;
        if(hasCustomSortField) {
            sortValues = splitColumnValues(parser.evaluate(sortScript, track, scriptContext));
        }

        auto addColumns = [&columns, &hasCustomSortField, &items, &formatter,
                           &track](const QStringList& columnValues, const QStringList& sortColumnValues) {
            const ColumnData columnData = buildColumnData(columnValues, formatter);
            const RowKey key            = Utils::generateMd5Hash(columnData.plainColumns.join(QString{}));

            auto [it, inserted] = items.try_emplace(key);
            FilterRow& row      = it->second;

            if(inserted) {
                row.key         = key;
                row.columns     = columnData.plainColumns;
                row.richColumns = columnData.richColumns;

                if(hasCustomSortField) {
                    for(int column{0}; column < columnData.plainColumns.size(); ++column) {
                        if(std::cmp_greater_equal(column, columns.size()) || columns.at(column).sortField.isEmpty()) {
                            row.richColumns.push_back(sortRichText(columnData.plainColumns.value(column)));
                        }
                        else {
                            row.richColumns.push_back(
                                sortRichText(sortColumnText(sortColumnValues.value(column), formatter)));
                        }
                    }
                }
            }

            row.trackIds.push_back(track.id());
        };

        for(int index{0}; std::cmp_less(index, columnValues.size()); ++index) {
            QStringList rowSortValues;
            if(hasCustomSortField && !sortValues.empty()) {
                rowSortValues = sortValues.at(std::min(index, static_cast<int>(sortValues.size()) - 1));
            }
            addColumns(columnValues.at(index), rowSortValues);
        }
    }

    rows.reserve(items.size());
    for(auto& row : items | std::views::values) {
        rows.push_back(std::move(row));
    }

    return rows;
}

FilterRowList patchFilterRows(LibraryManager* libraryManager, const FilterColumnList& columns,
                              const FilterRowList& previousRows, const TrackList& previousTracks,
                              const TrackList& tracks, const TrackIds& changedTrackIds,
                              const FilterRowBuildContext& context)
{
    if(columns.empty() || tracks.empty()) {
        return {};
    }

    if(previousRows.empty() || previousTracks.empty()) {
        return buildFilterRows(libraryManager, columns, tracks, context);
    }

    if(changedTrackIds.empty() && previousTracks == tracks) {
        return previousRows;
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

    if(removedTrackIds.empty() && changedTracks.empty()) {
        return previousRows;
    }

    std::map<RowKey, FilterRow> patchedRows;

    for(const FilterRow& previousRow : previousRows) {
        FilterRow row = previousRow;
        std::erase_if(row.trackIds, [&removedTrackIds](int trackId) { return removedTrackIds.contains(trackId); });

        if(!row.trackIds.empty()) {
            patchedRows.emplace(row.key, std::move(row));
        }
    }

    for(FilterRow row : buildFilterRows(libraryManager, columns, changedTracks, context)) {
        auto [it, inserted] = patchedRows.try_emplace(row.key, row);
        if(!inserted) {
            it->second.columns     = row.columns;
            it->second.richColumns = row.richColumns;
            std::ranges::copy(row.trackIds, std::back_inserter(it->second.trackIds));
        }
    }

    std::unordered_map<int, int> trackPositions;
    trackPositions.reserve(tracks.size());
    for(int index{0}; std::cmp_less(index, tracks.size()); ++index) {
        trackPositions.emplace(tracks.at(index).id(), index);
    }

    FilterRowList rows;
    rows.reserve(patchedRows.size());

    for(auto& row : patchedRows | std::views::values) {
        std::ranges::stable_sort(row.trackIds, [&trackPositions](int left, int right) {
            return trackPositions.at(left) < trackPositions.at(right);
        });
        rows.push_back(std::move(row));
    }

    return rows;
}

TrackList filterTracksBySearch(const QString& search, const TrackList& tracks)
{
    if(search.isEmpty() || tracks.empty()) {
        return tracks;
    }

    ScriptParser parser;
    return parser.filter(search, tracks);
}

FilterRowList filterRowsBySearch(const QString& search, const FilterRowList& rows, const TrackList& tracks)
{
    if(search.isEmpty() || rows.empty()) {
        return rows;
    }

    const TrackList filteredTracks = filterTracksBySearch(search, tracks);
    if(filteredTracks.empty()) {
        return {};
    }

    if(filteredTracks.size() == tracks.size()) {
        return rows;
    }

    std::unordered_set<int> matchingTrackIds;
    matchingTrackIds.reserve(filteredTracks.size());

    for(const Track& track : filteredTracks) {
        matchingTrackIds.emplace(track.id());
    }

    FilterRowList filteredRows;
    filteredRows.reserve(rows.size());

    for(const FilterRow& row : rows) {
        TrackIds matchedTrackIds;
        matchedTrackIds.reserve(row.trackIds.size());

        for(const int trackId : row.trackIds) {
            if(matchingTrackIds.contains(trackId)) {
                matchedTrackIds.push_back(trackId);
            }
        }

        if(matchedTrackIds.empty()) {
            continue;
        }

        FilterRow filteredRow = row;
        filteredRow.trackIds  = std::move(matchedTrackIds);
        filteredRows.push_back(std::move(filteredRow));
    }

    return filteredRows;
}
} // namespace Fooyin::Filters
