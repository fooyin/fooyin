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

#include "scriptregistry.h"

#include "functions/controlfuncs.h"
#include "functions/mathfuncs.h"

#include <QVariant>

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
    const QVariant v = m_vars.at(var);
    return {v.toString(), v.toBool()};
}

Value Registry::function(const QString& func, const ValueList& args)
{
    if(func.isEmpty() || !m_funcs.count(func)) {
        return {"", false};
    }
    auto function = m_funcs.at(func);
    if(std::holds_alternative<NativeFunc>(function)) {
        const auto f         = std::get<0>(function);
        const QString result = f(containerCast<StringList>(args));
        return {result, !result.isEmpty()};
    }
    if(std::holds_alternative<NativeCondFunc>(function)) {
        const auto f = std::get<1>(function);
        return f(args);
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
    m_funcs.emplace("if", NativeCondFunc(cif));
}
} // namespace Fy::Core::Scripting
