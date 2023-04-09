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

#include "scriptparser.h"

#include "core/constants.h"

namespace Fy::Core::Scripting {
ParsedScript Parser::parse(const QString& input)
{
    if(input.isEmpty()) {
        return {};
    }

    m_hadError = false;

    ParsedScript result;
    result.input = input;

    m_scanner.setup(input);

    advance();
    while(m_current.type != TokEos) {
        result.expressions.emplace_back(expression());
    }

    consume(TokEos, "Expected end of expression");
    result.valid   = !m_hadError;
    m_parsedScript = result;

    return result;
}

QString Parser::evaluate()
{
    if(!m_parsedScript.valid) {
        return {};
    }
    return evaluate(m_parsedScript);
}

QString Parser::evaluate(const ParsedScript& input)
{
    if(!input.valid) {
        return {};
    }

    m_result.clear();

    for(const auto& expr : input.expressions) {
        const auto evalExpr = evalExpression(expr);
        if(evalExpr.cond) {
            m_result.append(evalExpr.value);
        }
    }
    return m_result;
}

QString Parser::evaluate(const ParsedScript& input, const Core::Track& track)
{
    setMetadata(track);
    return evaluate(input);
}

void Parser::setMetadata(const Core::Track& track)
{
    m_registry.changeCurrentTrack(track);
}

ScriptResult Parser::evalExpression(const Expression& exp) const
{
    switch(exp.type) {
        case(Literal): {
            return evalLiteral(exp);
        }
        case(Variable): {
            return evalVariable(exp);
        }
        case(Function): {
            return evalFunction(exp);
        }
        case(FunctionArg): {
            return evalFunctionArg(exp);
        }
        case(Conditional): {
            return evalConditional(exp);
        }
        case(Null):
            break;
    }
    return {};
}

ScriptResult Parser::evalLiteral(const Expression& exp) const
{
    ScriptResult result;
    result.value = std::get<QString>(exp.value);
    result.cond  = true;
    return result;
}

ScriptResult Parser::evalVariable(const Expression& exp) const
{
    const QString var   = std::get<QString>(exp.value);
    ScriptResult result = m_registry.varValue(var);

    if(result.value.contains(Core::Constants::Separator)) {
        // TODO: Support custom separators
        result.value = result.value.replace(Core::Constants::Separator, ", ");
    }
    return result;
}

ScriptResult Parser::evalFunction(const Expression& exp) const
{
    auto function = std::get<FuncValue>(exp.value);
    ValueList args;
    for(auto& arg : function.args) {
        args.emplace_back(evalExpression(arg));
    }
    return m_registry.function(function.name, args);
}

ScriptResult Parser::evalFunctionArg(const Expression& exp) const
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
            QString values = subExpr.value;
            values         = values.replace(Core::Constants::Separator, ", ");
            result.value += values;
        }
        else {
            result.value += subExpr.value;
        }
    }
    result.cond = allPassed;
    return result;
}

ScriptResult Parser::evalConditional(const Expression& exp) const
{
    ScriptResult result;
    result.cond = true;

    auto arg = std::get<ExpressionList>(exp.value);
    for(auto& subArg : arg) {
        const auto subExpr = evalExpression(subArg);

        // Literals return false
        if(subArg.type != Literal) {
            if(!subExpr.cond || subExpr.value.isEmpty()) {
                // No need to evaluate rest
                result.value = {};
                result.cond  = false;
                return result;
            }
        }
        if(subExpr.value.contains(Core::Constants::Separator)) {
            QString values = subExpr.value;
            values         = values.replace(Core::Constants::Separator, ", ");
            result.value += values;
        }
        else {
            result.value += subExpr.value;
        }
    }
    return result;
}

void Parser::advance()
{
    m_previous = m_current;

    m_current = m_scanner.scanNext();
    if(m_current.type == TokError) {
        errorAtCurrent(m_current.value.toString());
    }
}

void Parser::consume(TokenType type, const QString& message)
{
    if(m_current.type == type) {
        advance();
        return;
    }
    errorAtCurrent(message);
}

bool Parser::currentToken(TokenType type) const
{
    return m_current.type == type;
}

bool Parser::match(TokenType type)
{
    if(!currentToken(type)) {
        return false;
    }
    advance();
    return true;
}

void Parser::errorAtCurrent(const QString& message)
{
    errorAt(m_current, message);
}

void Parser::errorAt(const Token& token, const QString& message)
{
    if(m_hadError) {
        return;
    }
    m_hadError    = true;
    auto errorMsg = QString{"[%1] Error"}.arg(token.position);

    if(token.type == TokEos) {
        errorMsg += " at end of string";
    }
    else {
        errorMsg += QString(": '%1'").arg(token.value);
    }

    errorMsg += QString(" (%1)").arg(message);

    qDebug() << errorMsg;
}

void Parser::error(const QString& message)
{
    errorAt(m_previous, message);
}

Expression Parser::expression()
{
    advance();
    switch(m_previous.type) {
        case(TokLiteral):
            return literal();
        case(TokVar):
            return variable();
        case(TokFunc):
            return function();
        case(TokQuote):
            return quote();
        case(TokLeftSquare):
            return conditional();
        case(TokEscape):
            advance();
            return literal();
        case(TokComma):
        case(TokLeftParen):
        case(TokRightParen):
        case(TokRightSquare):
        case(TokEos):
        case(TokError):
            break;
    }
    // We shouldn't ever reach here
    return {Null};
}

Expression Parser::literal() const
{
    return {Literal, m_previous.value.toString()};
}

Expression Parser::quote()
{
    Expression expr{Literal};
    QString val;

    while(!currentToken(TokQuote)) {
        advance();
        val.append(m_previous.value.toString());
        if(currentToken(TokEscape)) {
            advance();
            val.append(m_current.value.toString());
            advance();
        }
    }

    expr.value = val;
    consume(TokQuote, "Expected '\"' after expression");
    return expr;
}

Expression Parser::variable()
{
    advance();

    Expression expr{Variable};
    const auto value = QString{m_previous.value.toString()}.toLower();

    if(!m_registry.varExists(value)) {
        error("Variable not found");
    }

    expr.value = value;
    consume(TokVar, "Expected '%' after expression");
    return expr;
}

Expression Parser::function()
{
    advance();

    if(m_previous.type != TokLiteral) {
        error("Expected function name");
    }

    Expression expr{Function};
    FuncValue funcExpr;
    funcExpr.name = QString{m_previous.value.toString()}.toLower();

    if(!m_registry.funcExists(funcExpr.name)) {
        error("Function not found");
    }

    consume(TokLeftParen, "Expected '(' after function call");

    if(!currentToken(TokRightParen)) {
        funcExpr.args.emplace_back(functionArgs());
        while(match(TokComma)) {
            funcExpr.args.emplace_back(functionArgs());
        }
    }

    expr.value = funcExpr;
    consume(TokRightParen, "Expected ')' after function call");
    return expr;
}

Expression Parser::functionArgs()
{
    Expression expr{FunctionArg};
    ExpressionList funcExpr;

    while(!currentToken(TokComma) && !currentToken(TokRightParen) && !currentToken(TokEos)) {
        funcExpr.emplace_back(expression());
    }

    expr.value = funcExpr;
    return expr;
}

Expression Parser::conditional()
{
    Expression expr{Conditional};
    ExpressionList condExpr;

    while(!currentToken(TokRightSquare) && !currentToken(TokEos)) {
        condExpr.emplace_back(expression());
    }

    expr.value = condExpr;
    consume(TokRightSquare, "Expected ']' after expression");
    return expr;
}

const Registry& Parser::registry() const
{
    return m_registry;
}

const ParsedScript& Parser::lastParsedScript() const
{
    return m_parsedScript;
}
} // namespace Fy::Core::Scripting
