/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

namespace Fooyin {
struct PlaylistColumn
{
    static constexpr qint32 Magic   = -0x50434F4C;
    static constexpr qint32 Version = 1;

    int id{-1};
    bool enabled{true};
    int index{-1};
    bool isDefault{false};
    QString name;
    QString field;
    QString writeField;
    bool isPixmap{false};

    bool operator==(const PlaylistColumn& other) const
    {
        return std::tie(id, enabled, index, name, field, writeField)
            == std::tie(other.id, other.enabled, other.index, other.name, other.field, other.writeField);
    }

    [[nodiscard]] bool isValid() const
    {
        return id >= 0 && !name.isEmpty() && !field.isEmpty();
    }

    friend QDataStream& operator<<(QDataStream& stream, const PlaylistColumn& column)
    {
        stream << Magic;
        stream << Version;
        stream << column.id;
        stream << column.enabled;
        stream << column.index;
        stream << column.name;
        stream << column.field;
        stream << column.writeField;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, PlaylistColumn& column)
    {
        qint32 magicOrId{-1};
        stream >> magicOrId;

        if(magicOrId == Magic) {
            qint32 version{0};
            stream >> version;

            stream >> column.id;
            stream >> column.enabled;
            stream >> column.index;
            stream >> column.name;
            stream >> column.field;
            stream >> column.writeField;
        }
        else {
            column.id = magicOrId;
            stream >> column.index;
            stream >> column.name;
            stream >> column.field;
        }

        return stream;
    }
};
using PlaylistColumnList = std::vector<PlaylistColumn>;
using ColumnIds          = std::vector<int>;
} // namespace Fooyin
