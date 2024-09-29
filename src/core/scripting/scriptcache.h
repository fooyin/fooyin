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

#include <core/scripting/scriptparser.h>

namespace Fooyin {
class ScriptCache
{
public:
    ScriptCache();

    ParsedScript& operator[](const QString& key);

    bool contains(const QString& key) const;
    ParsedScript get(const QString& key) const;
    void insert(const QString& key, const ParsedScript& script);

    [[nodiscard]] int limit() const;
    void setLimit(int limit);
    void clear();

private:
    std::unordered_map<QString, ParsedScript> m_parsedScripts;
    std::vector<QString> m_order;
    int m_cacheLimit;
};
} // namespace Fooyin
