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

#include "infoitem.h"

namespace Fy::Gui::Widgets {
InfoItem::InfoItem(Type type, Role role, QString title, InfoItem* parent)
    : TreeItem{parent}
    , m_type{type}
    , m_role{role}
    , m_title{std::move(title)}
{ }

int InfoItem::columnCount() const
{
    return 2;
}

QString InfoItem::data() const
{
    return m_title;
}

InfoItem::Type InfoItem::type() const
{
    return m_type;
}

InfoItem::Role InfoItem::role() const
{
    return m_role;
}
} // namespace Fy::Gui::Widgets
