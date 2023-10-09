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

#include <core/scripting/scriptparser.h>

#include <core/constants.h>
#include <core/scripting/scriptscanner.h>

#include <QDebug>
#include <QStringBuilder>

namespace Fy::Core::Scripting {
QStringList evalStringList(const ScriptResult& evalExpr, const QStringList& result)
{
    QStringList listResult;
    const QStringList values = evalExpr.value.split(Constants::Separator);
    const bool isEmpty       = result.empty();

    for(const QString& value : values) {
        if(isEmpty) {
            listResult.append(value);
        }
        else {
            std::ranges::transform(result, std::back_inserter(listResult), [&](const QString& retValue) -> QString {
                return retValue % value;
            });
        }
    }
    return listResult;
}

struct Parser::Private
{
    Scanner scanner;
    Registry* registry;

    Token current;
    Token previous;
    ParsedScript parsedScript;
    QStringList result;

    explicit Private(Registry* registry)
        : scanner{}
        , registry{registry}
    { }

    void advance()
    {
        previous = current;

        current = scanner.scanNext();
        if(current.type == TokError) {
            errorAtCurrent(current.value.toString());
        }
    }

    void consume(TokenType type, const QString& message)
    {
        if(current.type == type) {
            advance();
            return;
        }
        errorAtCurrent(message);
    }

    bool currentToken(TokenType type) const
    {
        return current.type == type;
    }

    bool match(TokenType type)
    {
        if(!currentToken(type)) {
            return false;
        }
        advance();
        return true;
    }

    void errorAtCurrent(const QString& message)
    {
        errorAt(current, message);
    }

    void errorAt(const Token& token, const QString& message)
    {
        auto errorMsg = QString{"[%1] Error"}.arg(token.position);

        if(token.type == TokEos) {
            errorMsg += " at end of string";
        }
        else {
            errorMsg += QString(": '%1'").arg(token.value);
        }

        errorMsg += QString(" (%1)").arg(message);

        Error error;
        error.value    = token.value.toString();
        error.position = token.position;
        error.message  = errorMsg;

        parsedScript.errors.emplace_back(error);
    }

    void error(const QString& message)
    {
        errorAt(previous, message);
    }
};

Parser::Parser(Registry* registry)
    : p{std::make_unique<Private>(registry)}
{ }

Parser::~Parser() = default;

ParsedScript Parser::parse(const QString& input)
{
    if(input.isEmpty() || !p->registry) {
        return {};
    }

    p->parsedScript.errors.clear();
    p->parsedScript.expressions.clear();
    p->parsedScript.input = input;

    p->scanner.setup(input);

    p->advance();
    while(p->current.type != TokEos) {
        p->parsedScript.expressions.emplace_back(expression());
    }

    p->consume(TokEos, "Expected end of expression");

    return p->parsedScript;
}

QString Parser::evaluate()
{
    if(!p->parsedScript.isValid()) {
        return {};
    }
    return evaluate(p->parsedScript);
}

QString Parser::evaluate(const ParsedScript& input)
{
    if(!input.isValid() || !p->registry) {
        return {};
    }

    p->result.clear();

    const ExpressionList expressions = input.expressions;
    for(const auto& expr : expressions) {
        const auto evalExpr = evalExpression(expr);

        if(evalExpr.value.isNull()) {
            continue;
        }

        if(evalExpr.value.contains(Constants::Separator)) {
            const QStringList evalList = evalStringList(evalExpr, p->result);
            if(!evalList.empty()) {
                p->result = evalList;
            }
        }
        else {
            if(p->result.empty()) {
                p->result.push_back(evalExpr.value);
            }
            else {
                std::ranges::transform(p->result, p->result.begin(), [&](const QString& retValue) -> QString {
                    return retValue % evalExpr.value;
                });
            }
        }
    }
    if(p->result.size() == 1) {
        // Calling join on a QStringList with a single empty string will return a null QString, so return the first
        // result.
        return p->result.constFirst();
    }
    if(p->result.size() > 1) {
        return p->result.join(Constants::Separator);
    }
    return {};
}

QString Parser::evaluate(const ParsedScript& input, const Track& track)
{
    setMetadata(track);
    return evaluate(input);
}

void Parser::setMetadata(const Track& track)
{
    if(p->registry) {
        p->registry->changeCurrentTrack(track);
    }
}

ScriptResult Parser::evalExpression(const Expression& exp) const
{
    switch(exp.type) {
        case(Literal):
            return evalLiteral(exp);
        case(Variable):
            return evalVariable(exp);
        case(VariableList):
            return evalVariableList(exp);
        case(Function):
            return evalFunction(exp);
        case(FunctionArg):
            return evalFunctionArg(exp);
        case(Conditional):
            return evalConditional(exp);
        case(Null):
        default:
            return {};
    }
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
    ScriptResult result = p->registry->varValue(var);

    if(result.value.contains(Constants::Separator)) {
        // TODO: Support custom separators
        result.value = result.value.replace(Constants::Separator, ", ");
    }
    return result;
}

ScriptResult Parser::evalVariableList(const Expression& exp) const
{
    const QString var = std::get<QString>(exp.value);
    return p->registry->varValue(var);
}

ScriptResult Parser::evalFunction(const Expression& exp) const
{
    auto func = std::get<FuncValue>(exp.value);
    ValueList args;
    std::ranges::transform(func.args, std::back_inserter(args), [this](const Expression& arg) {
        return evalExpression(arg);
    });
    return p->registry->function(func.name, args);
}

ScriptResult Parser::evalFunctionArg(const Expression& exp) const
{
    ScriptResult result;
    bool allPassed{true};

    auto arg = std::get<ExpressionList>(exp.value);
    for(const Expression& subArg : arg) {
        const auto subExpr = evalExpression(subArg);
        if(!subExpr.cond) {
            allPassed = false;
        }
        if(subExpr.value.contains(Core::Constants::Separator)) {
            QStringList newResult;
            const auto values = subExpr.value.split(Core::Constants::Separator);
            std::ranges::transform(values, std::back_inserter(newResult), [&](const auto& value) {
                return result.value % value;
            });
            result.value = newResult.join(Core::Constants::Separator);
        }
        else {
            result.value = result.value % subExpr.value;
        }
    }
    result.cond = allPassed;
    return result;
}

ScriptResult Parser::evalConditional(const Expression& exp) const
{
    ScriptResult result;
    QStringList exprResult;
    result.cond = true;

    auto arg = std::get<ExpressionList>(exp.value);
    for(const Expression& subArg : arg) {
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
            if(exprResult.empty()) {
                exprResult.append(subExpr.value);
            }
            else {
                std::ranges::transform(exprResult, exprResult.begin(), [&](const QString& retValue) -> QString {
                    return retValue % subExpr.value;
                });
            }
        }
    }
    if(exprResult.size() == 1) {
        result.value = exprResult.constFirst();
    }
    else if(exprResult.size() > 1) {
        result.value = exprResult.join(Constants::Separator);
    }
    return result;
}

Expression Parser::expression()
{
    p->advance();
    switch(p->previous.type) {
        case(TokVar):
            return variable();
        case(TokFunc):
            return function();
        case(TokQuote):
            return quote();
        case(TokLeftSquare):
            return conditional();
        case(TokEscape):
            p->advance();
            return literal();
        case(TokLeftAngle):
        case(TokRightAngle):
        case(TokComma):
        case(TokLeftParen):
        case(TokRightParen):
        case(TokRightSquare):
        case(TokLiteral):
            return literal();
        case(TokEos):
        case(TokError):
            break;
    }
    // We shouldn't ever reach here
    return {Null};
}

Expression Parser::literal() const
{
    return {Literal, p->previous.value.toString()};
}

Expression Parser::quote()
{
    Expression expr{Literal};
    QString val;

    while(!p->currentToken(TokQuote)) {
        p->advance();
        val.append(p->previous.value.toString());
        if(p->currentToken(TokEscape)) {
            p->advance();
            val.append(p->current.value.toString());
            p->advance();
        }
    }

    expr.value = val;
    p->consume(TokQuote, "Expected '\"' after expression");
    return expr;
}

Expression Parser::variable()
{
    p->advance();

    Expression expr;
    QString value;

    if(p->previous.type == TokLeftAngle) {
        p->advance();
        expr.type = VariableList;
        value     = QString{p->previous.value.toString()}.toLower();
        p->consume(TokRightAngle, "Expected '>' after expression");
    }
    else {
        expr.type = Variable;
        value     = QString{p->previous.value.toString()}.toLower();
    }

    if(!p->registry->varExists(value)) {
        p->error("Variable not found");
    }

    expr.value = value;
    p->consume(TokVar, "Expected '%' after expression");
    return expr;
}

Expression Parser::function()
{
    p->advance();

    if(p->previous.type != TokLiteral) {
        p->error("Expected function name");
    }

    Expression expr{Function};
    FuncValue funcExpr;
    funcExpr.name = QString{p->previous.value.toString()}.toLower();

    if(!p->registry->funcExists(funcExpr.name)) {
        p->error("Function not found");
    }

    p->consume(TokLeftParen, "Expected '(' after function call");

    if(!p->currentToken(TokRightParen)) {
        funcExpr.args.emplace_back(functionArgs());
        while(p->match(TokComma)) {
            funcExpr.args.emplace_back(functionArgs());
        }
    }

    expr.value = funcExpr;
    p->consume(TokRightParen, "Expected ')' after function call");
    return expr;
}

Expression Parser::functionArgs()
{
    Expression expr{FunctionArg};
    ExpressionList funcExpr;

    while(!p->currentToken(TokComma) && !p->currentToken(TokRightParen) && !p->currentToken(TokEos)) {
        funcExpr.emplace_back(expression());
    }

    expr.value = funcExpr;
    return expr;
}

Expression Parser::conditional()
{
    Expression expr{Conditional};
    ExpressionList condExpr;

    while(!p->currentToken(TokRightSquare) && !p->currentToken(TokEos)) {
        condExpr.emplace_back(expression());
    }

    expr.value = condExpr;
    p->consume(TokRightSquare, "Expected ']' after expression");
    return expr;
}

const ParsedScript& Parser::lastParsedScript() const
{
    return p->parsedScript;
}
} // namespace Fy::Core::Scripting
