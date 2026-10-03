/*
 * Fooyin
 * Copyright © 2026, Piotr Wicijowski <piotr@wicijowski.pl>
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

#include <gui/scripting/scriptvariableregistry.h>

#include <core/scripting/scriptparser.h>

namespace Fooyin {
void ScriptVariableRegistry::registerProvider(const ScriptVariableProvider& provider)
{
    ScriptParser::addGlobalProvider(provider);
}

std::vector<ScriptVariableDescriptor> ScriptVariableRegistry::variables() const
{
    return ScriptParser::globalVariables();
}
} // namespace Fooyin
