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

namespace Fooyin {
struct LibraryTreeGrouping
{
    int id{-1};
    int index{-1};
    bool isDefault{false};
    QString name;
    QString script;
    QString sortScript;

    bool operator==(const LibraryTreeGrouping& other) const = default;

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
        stream << group.sortScript;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, LibraryTreeGrouping& group)
    {
        stream >> group.id;
        stream >> group.index;
        stream >> group.name;
        stream >> group.script;

        QString sort;

        stream.startTransaction();
        stream >> sort;
        if(stream.commitTransaction()) {
            group.sortScript = sort;
        }

        return stream;
    }
};
} // namespace Fooyin
