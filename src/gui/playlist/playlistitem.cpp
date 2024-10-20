/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include <utility>

namespace Fooyin {
PlaylistItem::PlaylistItem()
    : PlaylistItem{Root, {}, nullptr}
{ }

PlaylistItem::PlaylistItem(ItemType type, Data data, PlaylistItem* parent)
    : TreeItem{parent}
    , m_pending{false}
    , m_state{State::None}
    , m_type{type}
    , m_data{std::move(data)}
{ }

bool PlaylistItem::pending() const
{
    return m_pending;
}

PlaylistItem::State PlaylistItem::state() const
{
    return m_state;
}

PlaylistItem::ItemType PlaylistItem::type() const
{
    return m_type;
}

Data& PlaylistItem::data() const
{
    return m_data;
}

Md5Hash PlaylistItem::baseKey() const
{
    return m_baseKey;
}

UId PlaylistItem::key() const
{
    return m_key;
}

int PlaylistItem::index() const
{
    if(m_type != PlaylistItem::Track) {
        return -1;
    }
    const auto& trackItem = std::get<PlaylistTrackItem>(m_data);
    return trackItem.index();
}

void PlaylistItem::setPending(bool pending)
{
    m_pending = pending;
}

void PlaylistItem::setState(State state)
{
    m_state = state;
}

void PlaylistItem::setData(const Data& data)
{
    m_data = data;
}

void PlaylistItem::setBaseKey(const Md5Hash& key)
{
    m_baseKey = key;
}

void PlaylistItem::setKey(const UId& key)
{
    m_key = key;
}

void PlaylistItem::setIndex(int index)
{
    if(m_type != PlaylistItem::Track) {
        return;
    }
    auto& trackItem = std::get<PlaylistTrackItem>(m_data);
    trackItem.setIndex(index);
}

void PlaylistItem::removeColumn(int column)
{
    if(m_type != Track) {
        return;
    }

    auto& data = std::get<PlaylistTrackItem>(m_data);
    data.removeColumn(column);
}

void PlaylistItem::appendChild(PlaylistItem* child)
{
    TreeItem::appendChild(child);
    child->setPending(false);
    m_state = State::Update;
}

void PlaylistItem::insertChild(int row, PlaylistItem* child)
{
    TreeItem::insertChild(row, child);
    m_state = State::Update;
}

void PlaylistItem::removeChild(int index)
{
    TreeItem::removeChild(index);
    m_state = childCount() == 0 ? State::Delete : State::Update;
}
} // namespace Fooyin
