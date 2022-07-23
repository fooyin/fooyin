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

#include "playlistitem.h"

#include "library/models/libraryitem.h"

PlaylistItem::PlaylistItem(Type type, LibraryItem* data, PlaylistItem* parent)
    : m_data(data)
    , m_type(type)
    , m_index(0)
    , m_parent(parent)
{ }

PlaylistItem::~PlaylistItem() = default;

void PlaylistItem::appendChild(PlaylistItem* child)
{
    if(!m_children.contains(child)) {
        m_children.append(child);
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

LibraryItem* PlaylistItem::data() const
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
        return static_cast<int>(m_parent->m_children.indexOf(const_cast<PlaylistItem*>(this))); // NOLINT
    }

    return 0;
}

PlaylistItem* PlaylistItem::parent() const
{
    return m_parent;
}
