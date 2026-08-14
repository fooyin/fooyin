/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "fycore_export.h"

#include <core/scripting/expression.h>
#include <core/scripting/scriptparser.h>

namespace Fooyin {
class ScriptRegistry;

struct BoundExpression;
using BoundExpressionList = std::vector<BoundExpression>;

struct FYCORE_EXPORT BoundFunctionValue
{
    QString name;
    FunctionKind kind{FunctionKind::Generic};
    ScriptFunctionId functionId{InvalidScriptFunctionId};
    BoundExpressionList args;
};

using BoundExpressionValue = std::variant<QString, BoundFunctionValue, BoundExpressionList>;

struct FYCORE_EXPORT BoundExpression
{
    Expr::Type type{Expr::Null};
    VariableKind variableKind{VariableKind::Generic};
    BoundExpressionValue value{QString{}};
};

struct FYCORE_EXPORT BoundScript
{
    QString input;
    BoundExpressionList expressions;
    ErrorList errors;

    [[nodiscard]] bool isValid() const
    {
        return errors.empty() && !expressions.empty();
    }
};

[[nodiscard]] FYCORE_EXPORT VariableKind resolveBuiltInVariableKind(const QString& var);
[[nodiscard]] FYCORE_EXPORT FunctionKind resolveBuiltInFunctionKind(const QString& name);

[[nodiscard]] FYCORE_EXPORT BoundScript bindScript(const ParsedScript& script, const ScriptRegistry* registry);
} // namespace Fooyin
