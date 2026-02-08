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

#include <utils/datastream.h>

#include <QByteArray>
#include <QDataStream>
#include <vector>

namespace Fooyin {
QDataStream& operator<<(QDataStream& stream, const std::vector<int>& vec)
{
    return DataStream::writeVector(stream, vec);
}

QDataStream& operator>>(QDataStream& stream, std::vector<int>& vec)
{
    return DataStream::readVector(stream, vec);
}

QDataStream& operator<<(QDataStream& stream, const std::vector<int16_t>& vec)
{
    return DataStream::writeVector<int16_t, qint16>(stream, vec);
}

QDataStream& operator>>(QDataStream& stream, std::vector<int16_t>& vec)
{
    return DataStream::readVector<int16_t, qint16>(stream, vec);
}

QDataStream& operator<<(QDataStream& stream, const std::vector<uint64_t>& vec)
{
    return DataStream::writeVector<uint64_t, quint64>(stream, vec);
}

QDataStream& operator>>(QDataStream& stream, std::vector<uint64_t>& vec)
{
    return DataStream::readVector<uint64_t, quint64>(stream, vec);
}

QDataStream& operator<<(QDataStream& stream, const std::vector<QByteArray>& vec)
{
    return DataStream::writeVector(stream, vec);
}

QDataStream& operator>>(QDataStream& stream, std::vector<QByteArray>& vec)
{
    return DataStream::readVector(stream, vec);
}
} // namespace Fooyin
