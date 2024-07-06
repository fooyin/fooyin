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

namespace {
template <typename T, typename QType>
QDataStream& writeVector(QDataStream& stream, const std::vector<T>& vec)
{
    stream << static_cast<quint32>(vec.size());
    for(const auto& value : vec) {
        stream << static_cast<QType>(value);
    }
    return stream;
}

template <typename T, typename QtType>
QDataStream& readVector(QDataStream& stream, std::vector<T>& vec)
{
    quint32 size;
    stream >> size;

    vec.clear();
    vec.reserve(size);

    for(quint32 i{0}; i < size; ++i) {
        QtType value;
        stream >> value;
        vec.emplace_back(static_cast<T>(value));
    }
    return stream;
}
} // namespace

namespace Fooyin {
QDataStream& operator<<(QDataStream& stream, const std::vector<int>& vec)
{
    return writeVector<int, int>(stream, vec);
}

QDataStream& operator>>(QDataStream& stream, std::vector<int>& vec)
{
    return readVector<int, int>(stream, vec);
}

QDataStream& operator<<(QDataStream& stream, const std::vector<int16_t>& vec)
{
    return writeVector<int16_t, qint16>(stream, vec);
}

QDataStream& operator>>(QDataStream& stream, std::vector<int16_t>& vec)
{
    return readVector<int16_t, qint16>(stream, vec);
}

QDataStream& operator<<(QDataStream& stream, const std::vector<uint64_t>& vec)
{
    return writeVector<uint64_t, quint64>(stream, vec);
}

QDataStream& operator>>(QDataStream& stream, std::vector<uint64_t>& vec)
{
    return readVector<uint64_t, quint64>(stream, vec);
}
} // namespace Fooyin
