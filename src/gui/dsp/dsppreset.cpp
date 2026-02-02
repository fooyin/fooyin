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

#include "dsppreset.h"

namespace Fooyin {
QDataStream& operator<<(QDataStream& stream, const DspChainPreset& preset)
{
    stream << preset.id;
    stream << preset.index;
    stream << preset.name;
    stream << preset.chain;

    return stream;
}

QDataStream& operator>>(QDataStream& stream, DspChainPreset& preset)
{
    stream >> preset.id;
    stream >> preset.index;
    stream >> preset.name;
    stream >> preset.chain;

    return stream;
}
} // namespace Fooyin
