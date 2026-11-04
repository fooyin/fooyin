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
    static constexpr qint32 Magic   = -0x54454644;
    static constexpr qint32 Version = 2;

    int id{-1};
    int index{-1};
    bool isDefault{false};
    QString name;
    QString scriptField;
    bool multiline{false};
    bool multivalue{false};
    bool isEnabled{true};

    bool operator==(const TagEditorField& other) const = default;

    [[nodiscard]] bool isValid() const
    {
        return id >= 0 && !name.isEmpty() && !scriptField.isEmpty();
    }

    friend QDataStream& operator<<(QDataStream& stream, const TagEditorField& field)
    {
        stream << Magic;
        stream << Version;

        stream << field.id;
        stream << field.index;
        stream << field.name;
        stream << field.scriptField;
        stream << field.multiline;
        stream << field.multivalue;
        stream << field.isEnabled;

        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, TagEditorField& field)
    {
        qint32 magicOrId{-1};
        stream >> magicOrId;

        if(magicOrId == Magic) {
            qint32 version{0};
            stream >> version;

            stream >> field.id;
            stream >> field.index;
            stream >> field.name;
            stream >> field.scriptField;
            stream >> field.multiline;
            stream >> field.multivalue;
            if(version >= 2) {
                stream >> field.isEnabled;
            }
        }
        else {
            field.id = magicOrId;
            stream >> field.index;
            stream >> field.name;
            stream >> field.scriptField;
            stream >> field.multiline;
            stream >> field.multivalue;
        }

        return stream;
    }
};
} // namespace Fooyin::TagEditor
