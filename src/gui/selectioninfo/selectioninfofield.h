/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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
struct SelectionInfoField
{
    static constexpr qint32 Magic   = -0x53494644;
    static constexpr qint32 Version = 1;

    int id{-1};
    int index{-1};
    bool isDefault{false};
    QString name;
    QString scriptField;
    bool enabled{true};

    bool operator==(const SelectionInfoField& other) const = default;

    [[nodiscard]] bool isValid() const
    {
        return id >= 0 && !name.isEmpty() && !scriptField.isEmpty();
    }

    friend QDataStream& operator<<(QDataStream& stream, const SelectionInfoField& field)
    {
        stream << Magic << Version;
        stream << field.id << field.index << field.name << field.scriptField << field.enabled;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, SelectionInfoField& field)
    {
        qint32 magic{0};
        stream >> magic;
        if(magic != Magic) {
            return stream;
        }

        qint32 version{0};
        stream >> version;
        if(version != Version) {
            return stream;
        }

        stream >> field.id >> field.index >> field.name >> field.scriptField >> field.enabled;
        return stream;
    }
};
} // namespace Fooyin
