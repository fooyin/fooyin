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

#include <utility>

namespace Fy::Gui::Widgets::Playlist {
PlaylistItem::PlaylistItem()
    : PlaylistItem{Root, {}, nullptr}
{ }

PlaylistItem::PlaylistItem(ItemType type, Data data, PlaylistItem* parent)
    : TreeItem{parent}
    , m_pending{true}
    , m_type{type}
    , m_data{std::move(data)}
    , m_baseKey{"0"}
    , m_key{"0"}
    , m_indentation{0}
{ }

bool PlaylistItem::pending() const
{
    return m_pending;
}

PlaylistItem::ItemType PlaylistItem::type() const
{
    return m_type;
}

Data& PlaylistItem::data() const
{
    return m_data;
}

QString PlaylistItem::baseKey() const
{
    return m_baseKey;
}

QString PlaylistItem::key() const
{
    return m_key;
}

int PlaylistItem::indentation() const
{
    return m_indentation;
}

void PlaylistItem::setPending(bool pending)
{
    m_pending = pending;
}

void PlaylistItem::setBaseKey(const QString& key)
{
    m_baseKey = key;
}

void PlaylistItem::setKey(const QString& key)
{
    m_key = key;
}

void PlaylistItem::setIndentation(int indentation)
{
    m_indentation = indentation;
}
} // namespace Fy::Gui::Widgets::Playlist
