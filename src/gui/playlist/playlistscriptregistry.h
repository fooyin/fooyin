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

#pragma once

#include <core/scripting/scriptregistry.h>

namespace Fy::Gui::Widgets::Playlist {
class Container;

class PlaylistScriptRegistry : public Core::Scripting::Registry
{
public:
    PlaylistScriptRegistry();

    bool varExists(const QString& var) const override;
    Core::Scripting::ScriptResult varValue(const QString& var) const override;
    Core::Scripting::ScriptResult function(const QString &func, const Core::Scripting::ValueList &args) const override;

    void changeCurrentContainer(Container* container);

private:
    using ContainerFunc = std::function<FuncRet(const Container&)>;
    using FuncMap = std::unordered_map<QString, ContainerFunc>;

    Container* m_currentContainer;
    FuncMap m_vars;
};
} // namespace Fy::Gui::Widgets::Playlist
