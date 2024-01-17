/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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
#include <ranges>
#include <unordered_set>
#include <utility>
#include <vector>

#include <QRegularExpression>
#include <QString>

namespace Fooyin::Utils {
template <typename Ctnr, typename Element>
int findIndex(const Ctnr& c, const Element& e)
{
    int index  = -1;
    auto eIter = std::ranges::find(std::as_const(c), e);
    if(eIter != c.cend()) {
        index = static_cast<int>(std::ranges::distance(c.cbegin(), eIter));
    }
    return index;
}

template <typename Element>
bool contains(const std::vector<Element>& c, const Element& f)
{
    return std::ranges::find(std::as_const(c), f) != c.cend();
}

template <typename T, typename Hash, typename Ctnr>
Ctnr intersection(const Ctnr& v1, const Ctnr& v2)
{
    Ctnr result;
    std::unordered_set<T, Hash> first{v1.cbegin(), v1.cend()};

    auto commonElements = v2 | std::views::filter([&first](const auto& elem) { return first.contains(elem); });

    for(const auto& entry : commonElements) {
        result.push_back(entry);
    }
    return result;
}

template <typename Ctnr, typename Pred>
Ctnr filter(const Ctnr& container, Pred pred)
{
    Ctnr result;
    std::ranges::copy_if(std::as_const(container), std::back_inserter(result), pred);
    return result;
}

template <typename T>
void move(std::vector<T>& v, size_t from, size_t to)
{
    if(from > to) {
        std::rotate(v.rend() - from - 1, v.rend() - from, v.rend() - to);
    }
    else {
        std::rotate(v.begin() + from, v.begin() + from + 1, v.begin() + to + 1);
    }
}

template <typename T, typename StringExtractor>
QString findUniqueString(const QString& name, const T& elements, StringExtractor&& extractor)
{
    if(name.isEmpty()) {
        return {};
    }

    QString uniqueName{name};

    auto findCount = [&elements, extractor](const QString& name) -> int {
        const QString regexName    = QRegularExpression::escape(name);
        const QString regexPattern = QString{R"(^%1\s*(\(\d+\))?\s*$)"}.arg(regexName);
        const QRegularExpression pattern{regexPattern};

        return static_cast<int>(std::ranges::count_if(elements, [&pattern, extractor](const auto& element) {
            return pattern.match(extractor(element)).hasMatch();
        }));
    };

    const int count = findCount(name);

    return count > 0 ? name + " (" + QString::number(count) + ")" : name;
}
} // namespace Fooyin::Utils
