/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include <QDataStream>
#include <QString>

#include <tuple>

namespace Fooyin::Filters {
struct FilterColumn
{
    static constexpr qint32 Magic   = -0x46434F4C;
    static constexpr qint32 Version = 2;

    int id{-1};
    int index{-1};
    bool isDefault{false};
    QString name;
    QString field;
    QString sortField;

    bool operator==(const FilterColumn& other) const
    {
        return std::tie(id, index, name, field, sortField)
            == std::tie(other.id, other.index, other.name, other.field, other.sortField);
    }

    [[nodiscard]] bool isValid() const
    {
        return id >= 0 && !name.isEmpty() && !field.isEmpty();
    }

    friend QDataStream& operator<<(QDataStream& stream, const FilterColumn& column)
    {
        stream << Magic;
        stream << Version;
        stream << column.id;
        stream << column.index;
        stream << column.name;
        stream << column.field;
        stream << column.sortField;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, FilterColumn& column)
    {
        qint32 magicOrId{-1};
        stream >> magicOrId;

        if(magicOrId == Magic) {
            qint32 version{0};
            stream >> version;

            stream >> column.id;
            stream >> column.index;
            stream >> column.name;
            stream >> column.field;
            if(version >= 2) {
                stream >> column.sortField;
            }
            else {
                column.sortField.clear();
            }
        }
        else {
            column.id = magicOrId;
            stream >> column.index;
            stream >> column.name;
            stream >> column.field;
            column.sortField.clear();
        }

        return stream;
    }
};
using FilterColumnList = std::vector<FilterColumn>;
using ColumnIds        = std::vector<int>;
} // namespace Fooyin::Filters
