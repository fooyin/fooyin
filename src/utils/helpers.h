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
template <typename Ctnr, typename Element>
constexpr int findIndex(const Ctnr& c, const Element& e)
{
    int index = -1;
    auto eIter   = std::find(c.cbegin(), c.cend(), e);
    if(eIter != c.cend()) {
        index = static_cast<int>(std::distance(c.cbegin(), eIter));
    }
    return index;
}

template <typename Ctnr, typename Element>
constexpr bool contains(const Ctnr& c, const Element& f)
{
    auto it = std::find(c.cbegin(), c.cend(), f);
    return static_cast<bool>(it != c.cend());
}

template <typename Ctnr, typename Key>
constexpr bool hasKey(const Ctnr& c, const Key& key)
{
    return c.count(key);
}

template <typename T, typename Ctnr>
constexpr Ctnr intersection(Ctnr& v1, const Ctnr& v2)
{
    Ctnr result;
    std::unordered_set<T> first(v1.cbegin(), v1.cend());
    for (auto entry : v2)
    {
        if (first.count(entry)) {
            result.emplace_back(entry);
        }
    }
    return result;
}

template <typename Ctnr, typename Pred>
Ctnr filter(const Ctnr &container, Pred pred) {
    Ctnr result;
    std::copy_if(container.cbegin(), container.cend(), std::back_inserter(result), pred);
    return result;
}

} // namespace Fy::Utils
