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

#include "scriptregistry.h"

#include "functions/mathfuncs.h"

namespace Fy::Core::Scripting {
Registry::Registry()
{
    addDefaultFunctions();
}

Value Registry::varValue(const QString& var)
{
    if(var.isEmpty() || !m_vars.count(var)) {
        return {"", false};
    }
    return {m_vars.at(var), true};
}

Value Registry::function(const QString& func, const ValueList& args)
{
    if(func.isEmpty() || !m_funcs.count(func)) {
        return {"", false};
    }
    for(const auto& arg : args) {
        if(!arg.cond) {
            return {"", false};
        }
    }
    auto function = m_funcs.at(func);
    if(std::holds_alternative<NativeFunc>(function)) {
        const auto f         = std::get<0>(function);
        const QString result = f(containerCast<StringList>(args));
        if(!result.isEmpty()) {
            return {result, true};
        }
    }
    return {"", false};
}

void Registry::addDefaultFunctions()
{
    m_funcs.emplace("add", NativeFunc(add));
    m_funcs.emplace("sub", NativeFunc(sub));
    m_funcs.emplace("mul", NativeFunc(mul));
    m_funcs.emplace("div", NativeFunc(div));
    m_funcs.emplace("min", NativeFunc(min));
    m_funcs.emplace("max", NativeFunc(max));
}
} // namespace Fy::Core::Scripting
