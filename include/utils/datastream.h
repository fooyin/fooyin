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

#include "fyutils_export.h"

#include <QDataStream>

#include <set>
#include <vector>

namespace Fooyin {
FYUTILS_EXPORT QDataStream& operator<<(QDataStream& stream, const std::vector<int>& vec);
FYUTILS_EXPORT QDataStream& operator>>(QDataStream& stream, std::vector<int>& vec);

FYUTILS_EXPORT QDataStream& operator<<(QDataStream& stream, const std::set<int>& vec);
FYUTILS_EXPORT QDataStream& operator>>(QDataStream& stream, std::set<int>& vec);

FYUTILS_EXPORT QDataStream& operator<<(QDataStream& stream, const std::vector<int16_t>& vec);
FYUTILS_EXPORT QDataStream& operator>>(QDataStream& stream, std::vector<int16_t>& vec);

FYUTILS_EXPORT QDataStream& operator<<(QDataStream& stream, const std::vector<uint64_t>& vec);
FYUTILS_EXPORT QDataStream& operator>>(QDataStream& stream, std::vector<uint64_t>& vec);

FYUTILS_EXPORT QDataStream& operator<<(QDataStream& stream, const std::vector<QByteArray>& vec);
FYUTILS_EXPORT QDataStream& operator>>(QDataStream& stream, std::vector<QByteArray>& vec);

namespace DataStream {
template <class C>
using value_t = std::ranges::range_value_t<C>;

template <class C>
constexpr bool has_reserve_v = requires(C& c, size_t n) { c.reserve(n); };

template <class C, class V>
constexpr bool has_push_back_v = requires(C& c, V&& v) { c.push_back(std::forward<V>(v)); };

template <class To, class From>
To maybeCast(From&& v)
{
    if constexpr(std::is_same_v<std::remove_cvref_t<To>, std::remove_cvref_t<From>>) {
        return std::forward<From>(v);
    }
    else {
        return static_cast<To>(std::forward<From>(v));
    }
}

template <class C, class V>
void insertValue(C& c, V&& v)
{
    if constexpr(has_push_back_v<C, V>) {
        c.push_back(std::forward<V>(v));
    }
    else {
        c.insert(std::forward<V>(v));
    }
}

template <class Container, class Wire = value_t<Container>>
QDataStream& writeContainer(QDataStream& s, const Container& c)
{
    s << static_cast<quint32>(c.size());
    for(const auto& v : c) {
        s << maybeCast<Wire>(v);
    }
    return s;
}

template <class Container, class Wire = value_t<Container>>
QDataStream& readContainer(QDataStream& s, Container& c)
{
    quint32 n{0};
    s >> n;

    c.clear();
    if constexpr(has_reserve_v<Container>) {
        c.reserve(static_cast<size_t>(n));
    }

    for(quint32 i{0}; i < n; ++i) {
        Wire w{};
        s >> w;
        if(s.status() != QDataStream::Ok) {
            c.clear();
            return s;
        }
        insertValue(c, maybeCast<value_t<Container>>(w));
    }

    return s;
}
} // namespace DataStream
} // namespace Fooyin
