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

#include "scriptcache.h"

constexpr auto DefaultLimit = 20;

namespace Fooyin {
ScriptCache::ScriptCache()
    : m_cacheLimit{DefaultLimit}
{ }

ParsedScript& ScriptCache::operator[](const QString& key)
{
    auto it = m_parsedScripts.find(key);
    if(it != m_parsedScripts.end()) {
        auto orderIt = std::ranges::find(m_order, key);
        if(orderIt != m_order.cend()) {
            m_order.erase(orderIt);
        }
        m_order.push_back(key);
        return it->second;
    }

    insert(key, {});
    return m_parsedScripts.at(key);
}

bool ScriptCache::contains(const QString& key) const
{
    return m_parsedScripts.contains(key);
}

ParsedScript ScriptCache::get(const QString& key) const
{
    if(m_parsedScripts.contains(key)) {
        return m_parsedScripts.at(key);
    }
    return {};
}

void ScriptCache::insert(const QString& key, const ParsedScript& script)
{
    auto it = m_parsedScripts.find(key);
    if(it != m_parsedScripts.end()) {
        it->second = script;

        auto orderIt = std::ranges::find(m_order, key);
        if(orderIt != m_order.cend()) {
            m_order.erase(orderIt);
        }
        m_order.push_back(key);
    }
    else {
        m_order.push_back(key);
        m_parsedScripts[key] = script;
    }

    if(std::cmp_greater(m_parsedScripts.size(), m_cacheLimit)) {
        const QString& oldestKey = m_order.front();
        m_order.erase(m_order.begin());
        m_parsedScripts.erase(oldestKey);
    }
}

int ScriptCache::limit() const
{
    return m_cacheLimit;
}

void ScriptCache::setLimit(int limit)
{
    m_cacheLimit = limit;
}

void ScriptCache::clear()
{
    m_parsedScripts.clear();
    m_order.clear();
}
} // namespace Fooyin
