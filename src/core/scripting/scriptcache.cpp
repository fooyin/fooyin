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

BoundScriptCache::BoundScriptCache()
    : m_cacheLimit{DefaultLimit}
{ }

BoundScript* BoundScriptCache::find(uint64_t key)
{
    auto it = m_scripts.find(key);
    if(it != m_scripts.end()) {
        touch(it);
        return &it->second.script;
    }
    return nullptr;
}

const BoundScript* BoundScriptCache::find(uint64_t key) const
{
    if(const auto it = m_scripts.find(key); it != m_scripts.cend()) {
        return &it->second.script;
    }
    return nullptr;
}

void BoundScriptCache::insert(uint64_t key, BoundScript script)
{
    auto it = m_scripts.find(key);
    if(it != m_scripts.end()) {
        it->second.script = std::move(script);
        touch(it);
    }
    else {
        m_order.push_back(key);
        const auto orderIt = std::prev(m_order.end());
        m_scripts.emplace(key, Entry{.script = std::move(script), .orderIt = orderIt});
    }

    if(std::cmp_greater(m_scripts.size(), m_cacheLimit)) {
        const uint64_t oldestKey = m_order.front();
        m_order.pop_front();
        m_scripts.erase(oldestKey);
    }
}

int BoundScriptCache::limit() const
{
    return m_cacheLimit;
}

void BoundScriptCache::setLimit(int limit)
{
    m_cacheLimit = limit;
}

void BoundScriptCache::clear()
{
    m_scripts.clear();
    m_order.clear();
}

void BoundScriptCache::touch(std::unordered_map<uint64_t, Entry>::iterator it)
{
    if(std::next(it->second.orderIt) != m_order.end()) {
        m_order.splice(m_order.end(), m_order, it->second.orderIt);
    }
    it->second.orderIt = std::prev(m_order.end());
}
} // namespace Fooyin
