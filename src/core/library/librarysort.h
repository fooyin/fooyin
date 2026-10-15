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
struct SortScript
{
    static constexpr qint32 Magic   = -0x534F5254;
    static constexpr qint32 Version = 1;

    int id{-1};
    int index{-1};
    bool isDefault{false};
    QString name;
    QString script;
    bool isEnabled{true};

    bool operator==(const SortScript& other) const = default;

    [[nodiscard]] bool isValid() const
    {
        return id >= 0 && !name.isEmpty() && !script.isEmpty();
    }

    friend QDataStream& operator<<(QDataStream& stream, const SortScript& sort)
    {
        stream << Magic;
        stream << Version;
        stream << sort.id;
        stream << sort.index;
        stream << sort.name;
        stream << sort.script;
        stream << sort.isEnabled;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, SortScript& sort)
    {
        qint32 magicOrId{-1};
        qint32 version{0};
        stream >> magicOrId;

        if(magicOrId == Magic) {
            stream >> version;
            stream >> sort.id;
        }
        else {
            sort.id = magicOrId;
        }

        stream >> sort.index;
        stream >> sort.name;
        stream >> sort.script;
        sort.isEnabled = true;

        if(version >= 1) {
            stream >> sort.isEnabled;
        }

        return stream;
    }
};
} // namespace Fooyin
