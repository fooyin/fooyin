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

#include "fielditem.h"

namespace Fy::Filters::Settings {
FieldItem::FieldItem(FilterField field, FieldItem* parent)
    : TreeItem{parent}
    , m_field{std::move(field)}
{ }

FilterField FieldItem::field() const
{
    return m_field;
}

void FieldItem::changeField(const FilterField& field)
{
    m_field = field;
}
} // namespace Fy::Filters::Settings
