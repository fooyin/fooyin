/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "filteritem.h"

#include <core/library/musiclibrary.h>
#include <core/library/tracksort.h>
#include <core/track.h>
#include <utils/utils.h>

#include <utility>

namespace Fooyin::Filters {
namespace {
RichText plainTextToRichText(const QString& text)
{
    RichText richText;
    if(text.isEmpty()) {
        return richText;
    }

    RichTextBlock block;
    block.text = text;
    richText.blocks.push_back(std::move(block));
    return richText;
}

std::vector<RichText> plainColumnsToRichText(const QStringList& columns)
{
    std::vector<RichText> richColumns;
    richColumns.reserve(columns.size());

    for(const QString& column : columns) {
        richColumns.push_back(plainTextToRichText(column));
    }

    return richColumns;
}

QStringList orderedColumns(const QStringList& columns, const std::vector<int>& columnOrder)
{
    QStringList ordered;

    if(!columnOrder.empty()) {
        ordered = Utils::sortByIndexes(columns, columnOrder);
    }
    else {
        ordered = columns;
    }

    ordered.removeIf([](const QString& column) { return column.isEmpty(); });
    return ordered;
}
} // namespace

FilterItem::FilterItem(Md5Hash key, QStringList columns, FilterItem* parent)
    : TreeItem{parent}
    , m_key{std::move(key)}
    , m_columns{std::move(columns)}
    , m_isSummary{false}
{
    m_richColumns = plainColumnsToRichText(m_columns);
}

Md5Hash FilterItem::key() const
{
    return m_key;
}

const QStringList& FilterItem::columns() const
{
    return m_columns;
}

QString FilterItem::column(int column) const
{
    if(column < 0 || column >= m_columns.size()) {
        return {};
    }
    return m_columns.at(column);
}

const RichText& FilterItem::richColumn(int column) const
{
    static constexpr RichText EmptyText;

    if(column < 0 || std::cmp_greater_equal(column, m_richColumns.size())) {
        return EmptyText;
    }

    return m_richColumns.at(column);
}

QString FilterItem::iconLabel(const std::vector<int>& columnOrder) const
{
    if(!m_iconLabelCacheValid || m_cachedIconLabelOrder != columnOrder) {
        m_cachedIconLabel      = orderedColumns(m_columns, columnOrder).join(QChar::LineSeparator);
        m_cachedIconLabelOrder = columnOrder;
        m_iconLabelCacheValid  = true;
    }

    return m_cachedIconLabel;
}

IconCaptionLineList FilterItem::iconCaptionLines(const std::vector<int>& columnOrder,
                                                 const std::vector<Qt::Alignment>& columnAlignments) const
{
    updateIconCaptionColumns(columnOrder);

    IconCaptionLineList lines;
    lines.reserve(m_cachedIconCaptionColumns.size());

    for(const int index : m_cachedIconCaptionColumns) {
        IconCaptionLine line;
        line.text = m_richColumns.at(index);

        if(index >= 0 && std::cmp_less(index, columnAlignments.size())) {
            line.alignment = columnAlignments.at(index);
        }

        lines.push_back(std::move(line));
    }

    return lines;
}

const TrackIds& FilterItem::trackIds() const
{
    return m_trackIds;
}

int FilterItem::trackCount() const
{
    return static_cast<int>(m_trackIds.size());
}

int FilterItem::firstTrackId() const
{
    return m_trackIds.empty() ? -1 : m_trackIds.front();
}

void FilterItem::invalidateIconCaches()
{
    m_iconLabelCacheValid          = false;
    m_iconCaptionColumnsCacheValid = false;
}

void FilterItem::updateIconCaptionColumns(const std::vector<int>& columnOrder) const
{
    if(m_iconCaptionColumnsCacheValid && m_cachedIconCaptionOrder == columnOrder) {
        return;
    }

    m_cachedIconCaptionOrder = columnOrder;
    m_cachedIconCaptionColumns.clear();

    auto appendIfNotEmpty = [this](int index) {
        if(index < 0 || std::cmp_greater_equal(index, m_richColumns.size())) {
            return;
        }

        if(m_richColumns.at(index).joinedText().isEmpty()) {
            return;
        }

        m_cachedIconCaptionColumns.push_back(index);
    };

    if(!columnOrder.empty()) {
        for(const int index : columnOrder) {
            appendIfNotEmpty(index);
        }
    }
    else {
        const auto richCount = static_cast<int>(m_richColumns.size());
        for(int i{0}; i < richCount; ++i) {
            appendIfNotEmpty(i);
        }
    }

    m_iconCaptionColumnsCacheValid = true;
}

void FilterItem::setColumns(const QStringList& columns)
{
    m_columns     = columns;
    m_richColumns = plainColumnsToRichText(m_columns);
    invalidateIconCaches();
}

void FilterItem::setRichColumns(const std::vector<RichText>& columns)
{
    m_richColumns = columns;
    invalidateIconCaches();
}

void FilterItem::removeColumn(int column)
{
    m_columns.remove(column);

    if(column >= 0 && std::cmp_less(column, m_richColumns.size())) {
        m_richColumns.erase(m_richColumns.begin() + column);
    }

    invalidateIconCaches();
}

bool FilterItem::isSummary() const
{
    return m_isSummary;
}

void FilterItem::setIsSummary(bool isSummary)
{
    m_isSummary = isSummary;
}

void FilterItem::addTrack(const Track& track)
{
    m_trackIds.emplace_back(track.id());
}

void FilterItem::addTracks(const TrackIds& trackIds)
{
    m_trackIds.reserve(m_trackIds.size() + trackIds.size());
    std::ranges::copy(trackIds, std::back_inserter(m_trackIds));
}

void FilterItem::removeTrack(const Track& track)
{
    if(m_trackIds.empty()) {
        return;
    }
    std::erase(m_trackIds, track.id());
}

void FilterItem::sortTracks(MusicLibrary* library, TrackSorter& sorter, const ParsedScript& script)
{
    if(m_trackIds.empty() || !library) {
        return;
    }

    const TrackList sortedTracks = sorter.calcSortTracks(script, library->tracksForIds(m_trackIds));
    m_trackIds                   = Track::trackIdsForTracks(sortedTracks);
}
} // namespace Fooyin::Filters
