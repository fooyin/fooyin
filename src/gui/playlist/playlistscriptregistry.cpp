/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "playlistscriptregistry.h"

#include "playlistitemmodels.h"

#include <core/constants.h>

using namespace Qt::Literals::StringLiterals;

namespace {
bool isGroupScript(const Fooyin::ScriptValueList& args)
{
    return std::ranges::any_of(std::as_const(args), [](const auto& arg) {
        return arg.value.contains("%gduration%"_L1) || arg.value.contains("%gcount%"_L1)
            || arg.value.contains("%ggenres%"_L1);
    });
}
} // namespace

namespace Fooyin {
PlaylistScriptRegistry::PlaylistScriptRegistry()
    : m_currentContainer{nullptr}
{
    m_vars.emplace(u"gduration"_s, &PlaylistContainerItem::duration);
    m_vars.emplace(u"gcount"_s, &PlaylistContainerItem::trackCount);
    m_vars.emplace(u"ggenres"_s, &PlaylistContainerItem::genres);
    m_vars.emplace(u"gfiletypes"_s, &PlaylistContainerItem::filetypes);
}

bool PlaylistScriptRegistry::varExists(const QString& var) const
{
    return m_vars.contains(var) || ScriptRegistry::varExists(var);
}

ScriptResult PlaylistScriptRegistry::varValue(const QString& var) const
{
    if(!m_vars.contains(var)) {
        return ScriptRegistry::varValue(var);
    }
    if(!m_currentContainer) {
        return {"%"_L1 + var + "%"_L1};
    }
    const FuncRet funcResult = m_vars.at(var)(*m_currentContainer);
    return calculateResult(funcResult);
}

ScriptResult PlaylistScriptRegistry::function(const QString& func, const ScriptValueList& args) const
{
    if(!m_currentContainer && isGroupScript(args)) {
        auto tmpResult = QString{u"$%1("_s}.arg(func);
        for(int i{0}; const auto& arg : args) {
            tmpResult += i++ > 0 ? "," + arg.value : arg.value;
        }
        tmpResult += ")"_L1;
        return {tmpResult};
    }
    return ScriptRegistry::function(func, args);
}

void PlaylistScriptRegistry::changeCurrentContainer(PlaylistContainerItem* container)
{
    m_currentContainer = container;
}
} // namespace Fooyin
