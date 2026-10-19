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

#include "scriptbinder.h"

#include <core/scripting/scriptregistry.h>

#include <core/constants.h>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
BoundExpression bindExpression(const Expression& expr, const ScriptRegistry* registry, ScriptDependencies& dependencies)
{
    BoundExpression bound;
    bound.type = expr.type;

    if(const auto* value = std::get_if<QString>(&expr.value)) {
        bound.value = *value;

        if(expr.type == Expr::Variable || expr.type == Expr::VariableList) {
            bound.variableKind     = registry ? registry->resolveVariable(*value) : VariableKind::Generic;
            const auto* descriptor = registry ? registry->variableDescriptor(*value) : nullptr;
            dependencies.merge(
                descriptor ? descriptor->dependencies
                           : ScriptDependencies{.sources = {ScriptSources::TrackMetadata}, .unclassified = false});
        }
        else if(expr.type == Expr::VariableRaw) {
            dependencies.merge({.sources = {ScriptSources::TrackMetadata}, .unclassified = false});
        }

        return bound;
    }

    if(const auto* func = std::get_if<FuncValue>(&expr.value)) {
        BoundFunctionValue boundFunc;
        boundFunc.name         = func->name;
        boundFunc.functionId   = registry ? registry->resolveFunctionId(func->name) : InvalidScriptFunctionId;
        boundFunc.kind         = registry ? registry->functionKind(boundFunc.functionId) : FunctionKind::Generic;
        const auto* descriptor = registry ? registry->functionDescriptor(boundFunc.functionId) : nullptr;
        dependencies.merge(descriptor ? descriptor->dependencies : ScriptDependencies{});
        boundFunc.args.reserve(func->args.size());

        for(const auto& arg : func->args) {
            boundFunc.args.emplace_back(bindExpression(arg, registry, dependencies));
        }

        bound.value = std::move(boundFunc);
        return bound;
    }

    const auto& exprArgs = std::get<ExpressionList>(expr.value);

    BoundExpressionList args;
    args.reserve(exprArgs.size());

    for(const auto& arg : exprArgs) {
        args.emplace_back(bindExpression(arg, registry, dependencies));
    }

    bound.value = std::move(args);
    return bound;
}
} // namespace

BoundScript bindScript(const ParsedScript& script, const ScriptRegistry* registry, bool combineLiterals)
{
    BoundScript bound;
    bound.input  = script.input;
    bound.errors = script.errors;
    bound.expressions.reserve(script.expressions.size());

    for(const auto& expr : script.expressions) {
        if(combineLiterals && expr.type == Expr::Literal && !bound.expressions.empty()
           && bound.expressions.back().type == Expr::Literal) {
            const auto& value = std::get<QString>(expr.value);
            auto& previous    = std::get<QString>(bound.expressions.back().value);
            // A literal containing the list separator must retain its expansion step
            if(!value.contains(QLatin1StringView{Constants::UnitSeparator})
               && !previous.contains(QLatin1StringView{Constants::UnitSeparator})) {
                previous.append(value);
                continue;
            }
        }
        bound.expressions.emplace_back(bindExpression(expr, registry, bound.dependencies));
    }

    return bound;
}
} // namespace Fooyin
