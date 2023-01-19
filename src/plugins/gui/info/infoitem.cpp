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

#include "infoitem.h"

namespace Gui::Widgets {
InfoItem::InfoItem(Type type, QString title)
    : m_type(type)
    , m_title(std::move(title))
{ }

void InfoItem::appendChild(InfoItem* child)
{
    m_children.append(child);
}

InfoItem::~InfoItem()
{
    qDeleteAll(m_children);
}

InfoItem* InfoItem::child(int number)
{
    if(number < 0 || number >= m_children.size()) {
        return nullptr;
    }

    return m_children.at(number);
}

int InfoItem::childCount() const
{
    return static_cast<int>(m_children.size());
}

int InfoItem::columnCount()
{
    return 2;
}

QString InfoItem::data() const
{
    return m_title;
}

InfoItem::Type InfoItem::type()
{
    return m_type;
}

int InfoItem::row() const
{
    if(m_parent) {
        return static_cast<int>(m_parent->m_children.indexOf(const_cast<InfoItem*>(this))); // NOLINT
    }

    return 0;
}

InfoItem* InfoItem::parent() const
{
    return m_parent;
}
} // namespace Gui::Widgets
