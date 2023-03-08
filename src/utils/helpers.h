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
#include <unordered_set>

namespace Fy::Utils {
template <typename C, typename F>
constexpr int getIndex(C c, F f)
{
    int index = -1;
    auto it   = std::find(c.cbegin(), c.cend(), f);
    if(it != c.cend()) {
        index = it - c.cbegin();
    }
    return index;
}

template <typename C, typename F>
constexpr bool contains(C c, F f)
{
    auto it = std::find(c.cbegin(), c.cend(), f);
    return static_cast<bool>(it != c.cend());
}

template <typename C, typename F>
constexpr bool hasKey(C c, F f)
{
    return c.count(f);
}

template <typename E, typename T>
constexpr void intersection(T& v1, const T& v2, T& result)
{
    std::unordered_set<E> first(v1.cbegin(), v1.cend());
    for (auto entry : v2)
        if (first.count(entry)) {
            result.emplace_back(entry);
        }
}

} // namespace Fy::Utils
