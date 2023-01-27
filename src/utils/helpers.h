/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include <algorithm>
#include <type_traits>
#include <vector>

namespace Utils {
template <typename T>
class asRange
{
public:
    explicit asRange(T& data)
        : m_data{data}
    { }

    auto begin()
    {
        return m_data.keyValueBegin();
    }

    auto end()
    {
        return m_data.keyValueEnd();
    }

private:
    T& m_data;
};

template <typename E>
constexpr auto toUnderlying(E e) noexcept
{
    return static_cast<std::underlying_type_t<E>>(e);
}

template <typename C, typename F>
constexpr int getIndex(C c, F f)
{
    int index = -1;
    auto it   = std::find(c.begin(), c.end(), f);
    if(it != c.end()) {
        index = it - c.begin();
    }
    return index;
}

template <typename C, typename F>
constexpr bool contains(C c, F f)
{
    auto it = std::find(c.begin(), c.end(), f);
    return static_cast<bool>(it != c.end());
}

template <typename C, typename F>
constexpr bool hasKey(C c, F f)
{
    return c.count(f);
}

template <typename C, typename S, typename D>
constexpr void joinSkipEmpty(C c, S& s, D d)
{
    int j{0};
    for(const auto& str : c) {
        if(str.isEmpty()) {
            continue;
        }
        if(j) {
            s.append(d);
        }
        s.append(str);
        ++j;
    }
}
} // namespace Utils
