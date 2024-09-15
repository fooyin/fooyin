/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

namespace Fooyin::TagEditor {
struct TagEditorField
{
    int id{-1};
    int index{-1};
    bool isDefault{false};
    QString name;
    QString scriptField;
    bool multiline{false};
    bool multivalue{false};

    bool operator==(const TagEditorField& other) const
    {
        return std::tie(id, index, name, scriptField, multiline, multivalue)
            == std::tie(other.id, other.index, other.name, other.scriptField, other.multiline, other.multivalue);
    }

    [[nodiscard]] bool isValid() const
    {
        return id >= 0 && !name.isEmpty() && !scriptField.isEmpty();
    }

    friend QDataStream& operator<<(QDataStream& stream, const TagEditorField& field)
    {
        stream << field.id;
        stream << field.index;
        stream << field.name;
        stream << field.scriptField;
        stream << field.multiline;
        stream << field.multivalue;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, TagEditorField& field)
    {
        stream >> field.id;
        stream >> field.index;
        stream >> field.name;
        stream >> field.scriptField;
        stream >> field.multiline;
        stream >> field.multivalue;
        return stream;
    }
};
} // namespace Fooyin::TagEditor
