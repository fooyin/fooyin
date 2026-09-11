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

#include <core/library/libraryfilter.h>

constexpr qint32 Magic   = -0x4C465052;
constexpr qint32 Version = 1;

namespace Fooyin {
bool LibraryFilter::isValid() const
{
    return id >= 0 && !name.isEmpty() && !expression.isEmpty();
}

QDataStream& operator<<(QDataStream& stream, const LibraryFilter& preset)
{
    stream << Magic;
    stream << Version;
    stream << preset.id;
    stream << preset.index;
    stream << preset.name;
    stream << preset.expression;
    stream << preset.enabled;

    return stream;
}

QDataStream& operator>>(QDataStream& stream, LibraryFilter& preset)
{
    qint32 magic{0};
    qint32 version{0};
    stream >> magic >> version;
    if(magic != Magic || version < 1 || version > Version) {
        return stream;
    }

    stream >> preset.id;
    stream >> preset.index;
    stream >> preset.name;
    stream >> preset.expression;
    stream >> preset.enabled;

    return stream;
}
} // namespace Fooyin
