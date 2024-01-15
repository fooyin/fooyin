/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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
struct LibraryTreeGrouping
{
    int id{-1};
    int index{-1};
    bool isDefault{false};
    QString name;
    QString script;

    bool operator==(const LibraryTreeGrouping& other) const
    {
        return std::tie(id, index, name, script) == std::tie(other.id, other.index, other.name, other.script);
    }

    [[nodiscard]] bool isValid() const
    {
        return id >= 0 && !name.isEmpty() && !script.isEmpty();
    }

    friend QDataStream& operator<<(QDataStream& stream, const LibraryTreeGrouping& group)
    {
        stream << group.id;
        stream << group.index;
        stream << group.name;
        stream << group.script;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, LibraryTreeGrouping& group)
    {
        stream >> group.id;
        stream >> group.index;
        stream >> group.name;
        stream >> group.script;
        return stream;
    }
};
} // namespace Fooyin
