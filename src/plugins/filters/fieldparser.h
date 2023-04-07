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

#include <core/scripting/scriptparser.h>

namespace Fy::Filters::Scripting {
class FieldParser : public Core::Scripting::Parser
{
public:
    QString evaluate(const Core::Scripting::ParsedScript& parsedScript) override;

protected:
    Core::Scripting::ScriptResult evalVariable(const Core::Scripting::Expression& exp) const override;
    Core::Scripting::ScriptResult evalFunctionArg(const Core::Scripting::Expression& exp) const override;
    Core::Scripting::ScriptResult evalConditional(const Core::Scripting::Expression& exp) const override;

private:
    QStringList m_result;
};
} // namespace Fy::Filters::Scripting
