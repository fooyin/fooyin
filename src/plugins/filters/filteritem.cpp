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

#include "constants.h"

#include <QVariant>

namespace Fy::Filters {
FilterItem::FilterItem(QString title, FilterItem* parent, bool isAllNode)
    : TreeItem{parent}
    , m_title{std::move(title)}
    , m_isAllNode{isAllNode}
{ }

void FilterItem::changeTitle(const QString& title)
{
    m_title = title;
}

QVariant FilterItem::data(int role) const
{
    switch(role) {
        case Constants::Role::Title:
            return m_title;
        case Constants::Role::Tracks:
            return QVariant::fromValue(m_tracks);
        case Constants::Role::Sorting:
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

bool FilterItem::hasSortTitle() const
{
    return !m_sortTitle.isEmpty();
}

void FilterItem::setSortTitle(const QString& title)
{
    m_sortTitle = title;
}

void FilterItem::sortChildren(Qt::SortOrder order)
{
    std::vector<FilterItem*> sortedChildren{m_children};
    std::sort(sortedChildren.begin(), sortedChildren.end(), [order](FilterItem* lhs, FilterItem* rhs) {
        if(lhs->m_isAllNode) {
            return true;
        }
        if(rhs->m_isAllNode) {
            return false;
        }
        const auto lTitle
            = lhs->data(lhs->hasSortTitle() ? Constants::Role::Sorting : Constants::Role::Title).toString();
        const auto rTitle
            = rhs->data(rhs->hasSortTitle() ? Constants::Role::Sorting : Constants::Role::Title).toString();

        if(order == Qt::AscendingOrder) {
            return QString::localeAwareCompare(lTitle, rTitle) <= 0;
        }
        return QString::localeAwareCompare(lTitle, rTitle) > 0;
    });
    m_children = sortedChildren;
}
} // namespace Fy::Filters
