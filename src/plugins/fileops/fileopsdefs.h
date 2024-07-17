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

namespace Fooyin::FileOps {
enum class Operation : uint8_t
{
    Copy = 0,
    Move,
    Rename,
    Create,
    Remove
};

struct FileOpPreset
{
    Operation op{Operation::Copy};
    QString name;

    QString dest;
    QString filename;

    bool overwrite{false};
    bool wholeDir{false};
    bool removeEmpty{false};

    friend QDataStream& operator<<(QDataStream& stream, const FileOpPreset& preset)
    {
        stream << preset.op;
        stream << preset.name;
        stream << preset.dest;
        stream << preset.filename;
        stream << preset.overwrite;
        stream << preset.wholeDir;
        stream << preset.removeEmpty;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, FileOpPreset& preset)
    {
        stream >> preset.op;
        stream >> preset.name;
        stream >> preset.dest;
        stream >> preset.filename;
        stream >> preset.overwrite;
        stream >> preset.wholeDir;
        stream >> preset.removeEmpty;
        return stream;
    }
};
} // namespace Fooyin::FileOps
