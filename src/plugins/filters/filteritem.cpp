/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

namespace Fy::Filters {
FilterItem::FilterItem(QString title, QString sortTitle, FilterItem* parent, bool isAllNode)
    : TreeItem{parent}
    , m_title{std::move(title)}
    , m_sortTitle{std::move(sortTitle)}
    , m_isAllNode{isAllNode}
{ }

QString FilterItem::title() const
{
    return m_title;
}

QString FilterItem::sortTitle() const
{
    return m_sortTitle;
}

Core::TrackList FilterItem::tracks() const
{
    return m_tracks;
}

int FilterItem::trackCount() const
{
    return static_cast<int>(m_tracks.size());
}

void FilterItem::setTitle(const QString& title)
{
    m_title = title;
}

void FilterItem::addTrack(const Core::Track& track)
{
    m_tracks.emplace_back(track);
}

void FilterItem::addTracks(const Core::TrackList& tracks)
{
    std::ranges::copy(tracks, std::back_inserter(m_tracks));
}

void FilterItem::removeTrack(const Core::Track& track)
{
    if(m_tracks.empty()) {
        return;
    }
    m_tracks.erase(std::ranges::find_if(m_tracks, [track](const Core::Track& child) {
        return child.id() == track.id();
    }));
}

bool FilterItem::isAllNode() const
{
    return m_isAllNode;
}

void FilterItem::sortChildren(Qt::SortOrder order)
{
    std::vector<FilterItem*> sortedChildren{m_children};
    std::sort(sortedChildren.begin(), sortedChildren.end(), [order](const FilterItem* lhs, const FilterItem* rhs) {
        if(lhs->m_isAllNode) {
            return true;
        }
        if(rhs->m_isAllNode) {
            return false;
        }
        const auto lTitle = !lhs->m_sortTitle.isEmpty() ? lhs->m_sortTitle : lhs->m_title;
        const auto rTitle = !rhs->m_sortTitle.isEmpty() ? rhs->m_sortTitle : rhs->m_title;

        const auto cmp = QString::localeAwareCompare(lTitle, rTitle);
        if(cmp == 0) {
            return false;
        }
        if(order == Qt::AscendingOrder) {
            return cmp < 0;
        }
        return cmp > 0;
    });
    m_children = sortedChildren;
}
} // namespace Fy::Filters
