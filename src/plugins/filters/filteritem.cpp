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

#include <QVariant>

namespace Fy::Filters {
FilterItem::FilterItem(QString title, QString sortTitle, FilterItem* parent, bool isAllNode)
    : TreeItem{parent}
    , m_title{std::move(title)}
    , m_sortTitle{std::move(sortTitle)}
    , m_isAllNode{isAllNode}
{ }

QVariant FilterItem::data(int role) const
{
    switch(role) {
        case(FilterItemRole::Title):
            return m_title;
        case(FilterItemRole::Tracks):
            return QVariant::fromValue(m_tracks);
        case(FilterItemRole::Sorting):
            return m_sortTitle;
        default:
            return {};
    }
}

int FilterItem::trackCount() const
{
    return static_cast<int>(m_tracks.size());
}

void FilterItem::addTrack(const Core::Track& track)
{
    m_tracks.emplace_back(track);
}

bool FilterItem::isAllNode() const
{
    return m_isAllNode;
}

bool FilterItem::hasSortTitle() const
{
    return !m_sortTitle.isEmpty();
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
        const auto lTitle = lhs->hasSortTitle() ? lhs->m_sortTitle : lhs->m_title;
        const auto rTitle = rhs->hasSortTitle() ? rhs->m_sortTitle : rhs->m_title;

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
