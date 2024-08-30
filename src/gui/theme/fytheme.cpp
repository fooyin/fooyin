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

#include <gui/theme/fytheme.h>

constexpr auto ThemeVersion = 1;

namespace Fooyin {
QDataStream& operator<<(QDataStream& stream, const PaletteKey& key)
{
    stream << key.group;
    stream << key.role;
    return stream;
}

QDataStream& operator>>(QDataStream& stream, PaletteKey& key)
{
    stream >> key.group;
    stream >> key.role;
    return stream;
}

QDataStream& operator<<(QDataStream& stream, const FyTheme& scheme)
{
    stream << ThemeVersion;
    stream << scheme.id;
    stream << scheme.index;
    stream << scheme.name;
    stream << scheme.isDefault;
    stream << scheme.colours;
    stream << scheme.fonts;
    return stream;
}

QDataStream& operator>>(QDataStream& stream, FyTheme& scheme)
{
    int version{0};
    stream >> version;
    stream >> scheme.id;
    stream >> scheme.index;
    stream >> scheme.name;
    stream >> scheme.isDefault;
    stream >> scheme.colours;
    stream >> scheme.fonts;
    return stream;
}
} // namespace Fooyin
