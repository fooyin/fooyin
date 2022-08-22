/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include <utils/typedefs.h>

FilterItem::FilterItem(int id, QString name, FilterItem* parent)
    : m_id(id)
    , m_name(std::move(name))
    , m_parentItem(parent)
    , m_childItems(0)
{ }

void FilterItem::appendChild(FilterItem* child)
{
    m_childItems.append(child);
}

FilterItem* FilterItem::child(int number)
{
    if(number < 0 || number >= m_childItems.size()) {
        return nullptr;
    }
    return m_childItems.at(number);
}

FilterItem::~FilterItem()
{
    qDeleteAll(m_childItems);
}

int FilterItem::childCount() const
{
    return static_cast<int>(m_childItems.count());
}

int FilterItem::columnCount()
{
    return 1;
}

QVariant FilterItem::data(int role) const
{
    switch(role) {
        case(FilterRole::Id):
            return m_id;
        case(FilterRole::Name):
            return !m_name.isEmpty() ? m_name : "Unknown";
        default:
            return {};
    }
}

int FilterItem::row() const
{
    if(parentItem()) {
        return static_cast<int>(parentItem()->m_childItems.indexOf(const_cast<FilterItem*>(this)));
    }

    return 0;
}

FilterItem* FilterItem::parentItem() const
{
    return m_parentItem;
}
