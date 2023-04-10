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

#include "fieldparser.h"

#include <core/constants.h>

namespace Fy::Filters::Scripting {
using Core::Scripting::Expression;
using Core::Scripting::ExpressionList;
using Core::Scripting::ScriptResult;

QStringList evalStringList(const ScriptResult& evalExpr, const QStringList& result)
{
    QStringList listResult;
    const QStringList values = evalExpr.value.split(Core::Constants::Separator);
    const bool isEmpty       = result.empty();

    for(const QString& value : values) {
        if(isEmpty) {
            listResult.append(value);
        }
        else {
            for(const QString& retValue : result) {
                listResult.append(retValue + value);
            }
        }
    }
    return listResult;
}

QString FieldParser::evaluate(const Core::Scripting::ParsedScript& parsedScript)
{
    Core::Scripting::ParsedScript input{parsedScript};
    if(!input.valid) {
        return {};
    }

    m_result.clear();

    const auto expressions = input.expressions;
    for(const auto& expr : expressions) {
        const auto evalExpr = evalExpression(expr);
        const bool isEmpty  = m_result.empty();

        if(evalExpr.value.isNull()) {
            continue;
        }

        if(evalExpr.value.contains(Core::Constants::Separator)) {
            const QStringList evalList = evalStringList(evalExpr, m_result);
            if(!evalList.empty()) {
                m_result = evalList;
            }
        }
        else {
            if(isEmpty) {
                m_result.append(evalExpr.value);
            }
            else {
                for(QString& value : m_result) {
                    value.append(evalExpr.value);
                }
            }
        }
    }
    if(m_result.size() == 1) {
        // Calling join on a QStringList with a single empty string will return a null QString, so return the first
        // result.
        return m_result.constFirst();
    }
    if(m_result.size() > 1) {
        return m_result.join(Core::Constants::Separator);
    }
    return {};
}

ScriptResult FieldParser::evalVariable(const Expression& exp) const
{
    const QString var = std::get<QString>(exp.value);
    return registry().varValue(var);
}

ScriptResult FieldParser::evalFunctionArg(const Expression& exp) const
{
    ScriptResult result;
    bool allPassed{true};

    auto arg = std::get<ExpressionList>(exp.value);
    for(auto& subArg : arg) {
        const auto subExpr = evalExpression(subArg);
        if(!subExpr.cond) {
            allPassed = false;
        }
        if(subExpr.value.contains(Core::Constants::Separator)) {
            QStringList newResult;
            const auto values = subExpr.value.split(Core::Constants::Separator);
            for(const auto& value : values) {
                newResult.emplace_back(result.value + value);
            }
            result.value = newResult.join(Core::Constants::Separator);
        }
        else {
            result.value += subExpr.value;
        }
    }
    result.cond = allPassed;
    return result;
}

ScriptResult FieldParser::evalConditional(const Expression& exp) const
{
    QStringList exprResult;
    ScriptResult result;
    result.cond = true;

    auto arg = std::get<ExpressionList>(exp.value);
    for(auto& subArg : arg) {
        const bool isEmpty = exprResult.empty();
        const auto subExpr = evalExpression(subArg);

        // Literals return false
        if(subArg.type != Core::Scripting::Literal) {
            if(!subExpr.cond || subExpr.value.isEmpty()) {
                // No need to evaluate rest
                result.value = {};
                result.cond  = false;
                return result;
            }
        }
        if(subExpr.value.contains(Core::Constants::Separator)) {
            const QStringList evalList = evalStringList(subExpr, exprResult);
            if(!evalList.empty()) {
                exprResult = evalList;
            }
        }
        else {
            if(isEmpty) {
                exprResult.append(subExpr.value);
            }
            else {
                for(QString& value : exprResult) {
                    value.append(subExpr.value);
                }
            }
        }
    }
    if(!exprResult.empty()) {
        result.value = exprResult.join(Core::Constants::Separator);
    }
    return result;
}
} // namespace Fy::Filters::Scripting
