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

#include <core/models/trackfwd.h>

#include <QObject>

#include <deque>

namespace Fy::Filters {
struct FilterField
{
    int id{-1};
    int index{-1};
    QString name;
    QString field;
    QString sortField;

    bool operator==(const FilterField& other) const
    {
        return std::tie(id, index, name, field, sortField)
            == std::tie(other.id, other.index, other.name, other.field, other.sortField);
    }

    [[nodiscard]] bool isValid() const
    {
        return id >= 0 && !name.isEmpty() && !field.isEmpty();
    }

    friend QDataStream& operator<<(QDataStream& stream, const FilterField& field)
    {
        stream << field.id;
        stream << field.index;
        stream << field.name;
        stream << field.field;
        stream << field.sortField;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, FilterField& field)
    {
        stream >> field.id;
        stream >> field.index;
        stream >> field.name;
        stream >> field.field;
        stream >> field.sortField;
        return stream;
    }
};

struct LibraryFilter
{
    FilterField field;
    int index{-1};
    Core::TrackList tracks;
};

using FilterList = std::deque<LibraryFilter>;
} // namespace Fy::Filters
