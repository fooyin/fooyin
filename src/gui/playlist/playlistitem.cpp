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

namespace Fy::Gui::Widgets {
PlaylistItem::PlaylistItem(Type type, Core::MusicItem* data, PlaylistItem* parent)
    : TreeItem{parent}
    , m_data{data}
    , m_type{type}
{ }

void PlaylistItem::setKey(const QString& key)
{
    m_key = key;
}

Core::MusicItem* PlaylistItem::data() const
{
    return m_data;
}

PlaylistItem::Type PlaylistItem::type()
{
    return m_type;
}

QString PlaylistItem::key() const
{
    return m_key;
}
} // namespace Fy::Gui::Widgets
