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

namespace Fy::Gui::Widgets::Playlist {
using ScriptResult = Core::Scripting::ScriptResult;

PlaylistScriptRegistry::PlaylistScriptRegistry()
    : m_currentContainer{nullptr}
{
    m_vars.emplace("gduration", &Container::duration);
    m_vars.emplace("gcount", &Container::trackCount);
    m_vars.emplace("ggenres", &Container::genres);
}

bool PlaylistScriptRegistry::varExists(const QString& var) const
{
    return m_vars.contains(var) || Core::Scripting::Registry::varExists(var);
}

Core::Scripting::ScriptResult PlaylistScriptRegistry::varValue(const QString& var) const
{
    if(!m_vars.contains(var)) {
        return Core::Scripting::Registry::varValue(var);
    }
    if(!m_currentContainer) {
        return {};
    }
    const FuncRet funcResult = m_vars.at(var)(*m_currentContainer);
    return calculateResult(funcResult);
}

void PlaylistScriptRegistry::changeCurrentContainer(Container* container)
{
    m_currentContainer = container;
}
} // namespace Fy::Gui::Widgets::Playlist
