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

#include "playlistitem.h"

#include <utils/helpers.h>

namespace Gui::Widgets {
PlaylistItem::PlaylistItem(Type type, Core::LibraryItem* data, PlaylistItem* parent)
    : m_data(data)
    , m_type(type)
    , m_index(0)
    , m_parent(parent)
{ }

PlaylistItem::~PlaylistItem() = default;

void PlaylistItem::appendChild(PlaylistItem* child)
{
    if(!Utils::contains(m_children, child)) {
        m_children.emplace_back(child);
    }
}

void PlaylistItem::setIndex(int idx)
{
    m_index = idx;
}

PlaylistItem* PlaylistItem::child(int number)
{
    if(number < 0 || number >= m_children.size()) {
        return nullptr;
    }

    return m_children.at(number);
}

int PlaylistItem::childCount() const
{
    return static_cast<int>(m_children.size());
}

int PlaylistItem::columnCount()
{
    return 1;
}

Core::LibraryItem* PlaylistItem::data() const
{
    return m_data;
}

PlaylistItem::Type PlaylistItem::type()
{
    return m_type;
}

int PlaylistItem::index() const
{
    return m_index;
}

int PlaylistItem::row() const
{
    if(m_parent) {
        return static_cast<int>(Utils::getIndex(m_parent->m_children, const_cast<PlaylistItem*>(this))); // NOLINT
    }
    return 0;
}

PlaylistItem* PlaylistItem::parent() const
{
    return m_parent;
}
} // namespace Gui::Widgets
