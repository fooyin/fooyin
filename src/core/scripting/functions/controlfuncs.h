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

#pragma once

#include <core/scripting/scriptvalue.h>

namespace Fooyin::Scripting {
ScriptResult cif(const ScriptValueList& vec);
ScriptResult cif2(const ScriptValueList& vec);
ScriptResult ifequal(const ScriptValueList& vec);
ScriptResult ifgreater(const ScriptValueList& vec);
ScriptResult iflonger(const ScriptValueList& vec);
} // namespace Fooyin::Scripting
