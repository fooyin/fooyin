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

#pragma once

#include <QDataStream>
#include <QString>

namespace Fy::Core::Library::Sorting {
struct SortScript
{
    int id{-1};
    int index{-1};
    QString name;
    QString script;

    bool operator==(const SortScript& other) const
    {
        return std::tie(id, index, name, script) == std::tie(other.id, other.index, other.name, other.script);
    }

    [[nodiscard]] bool isValid() const
    {
        return id >= 0 && !name.isEmpty() && !script.isEmpty();
    }

    friend QDataStream& operator<<(QDataStream& stream, const SortScript& sort)
    {
        stream << sort.id;
        stream << sort.index;
        stream << sort.name;
        stream << sort.script;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, SortScript& sort)
    {
        stream >> sort.id;
        stream >> sort.index;
        stream >> sort.name;
        stream >> sort.script;
        return stream;
    }
};
} // namespace Fy::Core::Library
