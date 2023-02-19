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

#include "treeitem.h"

#include "helpers.h"

namespace Utils {
TreeItem::TreeItem(TreeItem* parent)
    : m_parent{parent}
{ }

TreeItem::~TreeItem()
{
    m_children.clear();
}

void TreeItem::appendChild(TreeItem* child)
{
    if(!Utils::contains(m_children, child)) {
        m_children.emplace_back(child);
    }
}

TreeItem* TreeItem::child(int index)
{
    if(index < 0 || index >= childCount()) {
        return nullptr;
    }
    return m_children.at(index);
}

int TreeItem::childCount() const
{
    return static_cast<int>(m_children.size());
}

int TreeItem::columnCount()
{
    return 1;
}

int TreeItem::row() const
{
    if(m_parent) {
        return static_cast<int>(Utils::getIndex(m_parent->m_children, const_cast<TreeItem*>(this))); // NOLINT
    }
    return 0;
}

TreeItem* TreeItem::parent() const
{
    return m_parent;
}
} // namespace Utils
