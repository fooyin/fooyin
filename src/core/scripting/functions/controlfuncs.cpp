/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "controlfuncs.h"

namespace Fooyin::Scripting {
ScriptResult cif(const ScriptValueList& vec)
{
    const auto size = vec.size();
    if(size < 2 || size > 3) {
        return {};
    }
    if(vec.at(0).cond) {
        return vec.at(1);
    }
    if(size > 2) {
        return vec.at(2);
    }
    return {};
}

ScriptResult cif2(const ScriptValueList& vec)
{
    const auto size = vec.size();
    if(size < 1 || size > 2) {
        return {};
    }
    if(vec.at(0).cond) {
        return vec.at(0);
    }
    if(size > 1) {
        return vec.at(1);
    }
    return {};
}

ScriptResult ifequal(const ScriptValueList& vec)
{
    const auto size = vec.size();
    if(size != 4) {
        return {};
    }
    if(vec.at(0).value.toDouble() == vec.at(1).value.toDouble()) {
        return vec.at(2);
    }
    return vec.at(3);
}

ScriptResult ifgreater(const ScriptValueList& vec)
{
    const auto size = vec.size();
    if(size < 3 || size > 4) {
        return {};
    }
    if(vec.at(0).value.toDouble() > vec.at(1).value.toDouble()) {
        return vec.at(2);
    }
    if(size == 4) {
        return vec.at(3);
    }
    return {};
}

ScriptResult iflonger(const ScriptValueList& vec)
{
    const auto size = vec.size();
    if(size != 4) {
        return {};
    }

    bool ok{false};
    const auto length = vec.at(1).value.toLongLong(&ok);
    if(!ok) {
        return {};
    }

    if(vec.at(0).value.size() >= length) {
        return vec.at(2);
    }
    return vec.at(3);
}
} // namespace Fooyin::Scripting
