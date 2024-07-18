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

#include <core/library/tracksort.h>
#include <core/track.h>

namespace Fooyin::Filters {
FilterItem::FilterItem(Md5Hash key, QStringList columns, FilterItem* parent)
    : TreeItem{parent}
    , m_key{std::move(key)}
    , m_columns{std::move(columns)}
    , m_isSummary{false}
{
    for(QString& str : m_columns) {
        if(str.isEmpty()) {
            str = QStringLiteral("?");
        }
    }
}

Md5Hash FilterItem::key() const
{
    return m_key;
}

QStringList FilterItem::columns() const
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

TrackList FilterItem::tracks() const
{
    return m_tracks;
}

int FilterItem::trackCount() const
{
    return static_cast<int>(m_tracks.size());
}

void FilterItem::setColumns(const QStringList& columns)
{
    m_columns = columns;
}

void FilterItem::removeColumn(int column)
{
    m_columns.remove(column);
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
    m_tracks.emplace_back(track);
}

void FilterItem::addTracks(const TrackList& tracks)
{
    std::ranges::copy(tracks, std::back_inserter(m_tracks));
}

void FilterItem::removeTrack(const Track& track)
{
    if(m_tracks.empty()) {
        return;
    }
    std::erase_if(m_tracks, [track](const Track& child) { return child.id() == track.id(); });
}

void FilterItem::replaceTrack(const Track& track)
{
    if(m_tracks.empty()) {
        return;
    }
    std::ranges::replace_if(m_tracks, [track](const Track& child) { return child.id() == track.id(); }, track);
}

void FilterItem::sortTracks()
{
    m_tracks = TrackSorter::sortTracks(m_tracks);
}
} // namespace Fooyin::Filters
