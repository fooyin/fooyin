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

#pragma once

#include <memory>
#include <vector>

#include <utils/helpers.h>

namespace Utils {
template <class Item>
class TreeItem
{
public:
    explicit TreeItem(Item* parent = nullptr)
        : m_parent{parent}
    { }

    virtual ~TreeItem()
    {
        m_children.clear();
    }

    virtual std::vector<Item*> children() const
    {
        return m_children;
    }

    virtual void appendChild(Item* child)
    {
        if(!Utils::contains(m_children, child)) {
            m_children.emplace_back(child);
        }
    }

    virtual void removeChild(int index)
    {
        if(index < 0 && index >= childCount()) {
            return;
        }
        m_children.erase(m_children.cbegin() + index);
    }

    virtual Item* child(int index) const
    {
        if(index < 0 || index >= childCount()) {
            return nullptr;
        }
        return m_children.at(index);
    }

    [[nodiscard]] virtual int childCount() const
    {
        return static_cast<int>(m_children.size());
    }

    [[nodiscard]] virtual int row() const
    {
        if(m_parent) {
            return Utils::getIndex(m_parent->m_children, this);
        }
        return 0;
    }

    [[nodiscard]] virtual int columnCount() const
    {
        return 1;
    }

    [[nodiscard]] virtual Item* parent() const
    {
        return m_parent;
    }

private:
    Item* m_parent;                // Not owned
    std::vector<Item*> m_children; // Not owned
};
} // namespace Utils
