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

using namespace Qt::Literals::StringLiterals;
using TokenType = Fooyin::ScriptScanner::TokenType;

namespace {
QStringList evalStringList(const Fooyin::ScriptResult& evalExpr, const QStringList& result)
{
    QStringList listResult;
    const QStringList values = evalExpr.value.split(Fooyin::Constants::Separator);
    const bool isEmpty       = result.empty();

    for(const QString& value : values) {
        if(isEmpty) {
            listResult.append(value);
        }
        else {
            std::ranges::transform(result, std::back_inserter(listResult),
                                   [&](const QString& retValue) -> QString { return retValue + value; });
        }
    }
    return listResult;
}
} // namespace

namespace Fooyin {
struct ScriptParser::Private
{
    ScriptScanner scanner;
    ScriptRegistry* registry;

    ScriptScanner::Token current;
    ScriptScanner::Token previous;
    ParsedScript parsedScript;
    QStringList result;

    explicit Private(ScriptRegistry* registry)
        : scanner{}
        , registry{registry}
    { }

    void advance()
    {
        previous = current;

        current = scanner.scanNext();
        if(current.type == TokenType::TokError) {
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

    [[nodiscard]] bool currentToken(TokenType type) const
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

    void errorAt(const ScriptScanner::Token& token, const QString& message)
    {
        QString errorMsg = "[" + QString::number(token.position) + "] Error";

        if(token.type == TokenType::TokEos) {
            errorMsg += u" at end of string"_s;
        }
        else {
            errorMsg += ": '" + token.value.toString() + "'";
        }

        errorMsg += " (" + message + ")";

        ScriptError currentError;
        currentError.value    = token.value.toString();
        currentError.position = token.position;
        currentError.message  = errorMsg;

        parsedScript.errors.emplace_back(currentError);
    }

    void error(const QString& message)
    {
        errorAt(previous, message);
    }
};

ScriptParser::ScriptParser(ScriptRegistry* registry)
    : p{std::make_unique<Private>(registry)}
{ }

ScriptParser::~ScriptParser() = default;

ParsedScript ScriptParser::parse(const QString& input)
{
    if(input.isEmpty() || !p->registry) {
        return {};
    }

    p->parsedScript.errors.clear();
    p->parsedScript.expressions.clear();
    p->parsedScript.input = input;

    p->scanner.setup(input);

    p->advance();
    while(p->current.type != TokenType::TokEos) {
        p->parsedScript.expressions.emplace_back(expression());
    }

    p->consume(TokenType::TokEos, u"Expected end of expression"_s);

    return p->parsedScript;
}

QString ScriptParser::evaluate()
{
    if(!p->parsedScript.isValid()) {
        return {};
    }
    return evaluate(p->parsedScript);
}

QString ScriptParser::evaluate(const Expression& input, const Track& track)
{
    ParsedScript script;
    script.expressions = {input};

    setMetadata(track);

    return evaluate(script);
}

QString ScriptParser::evaluate(const ParsedScript& input)
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
                std::ranges::transform(p->result, p->result.begin(),
                                       [&](const QString& retValue) -> QString { return retValue + evalExpr.value; });
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

QString ScriptParser::evaluate(const ParsedScript& input, const Track& track)
{
    setMetadata(track);
    return evaluate(input);
}

void ScriptParser::setMetadata(const Track& track)
{
    if(p->registry) {
        p->registry->changeCurrentTrack(track);
    }
}

ScriptResult ScriptParser::evalExpression(const Expression& exp) const
{
    switch(exp.type) {
        case(Expr::Literal):
            return evalLiteral(exp);
        case(Expr::Variable):
            return evalVariable(exp);
        case(Expr::VariableList):
            return evalVariableList(exp);
        case(Expr::Function):
            return evalFunction(exp);
        case(Expr::FunctionArg):
            return evalFunctionArg(exp);
        case(Expr::Conditional):
            return evalConditional(exp);
        case(Expr::Null):
        default:
            return {};
    }
}

ScriptResult ScriptParser::evalLiteral(const Expression& exp) const
{
    ScriptResult result;
    result.value = std::get<QString>(exp.value);
    result.cond  = true;
    return result;
}

ScriptResult ScriptParser::evalVariable(const Expression& exp) const
{
    const QString var   = std::get<QString>(exp.value);
    ScriptResult result = p->registry->varValue(var);

    if(result.value.contains(Constants::Separator)) {
        // TODO: Support custom separators
        result.value = result.value.replace(Constants::Separator, ", "_L1);
    }
    return result;
}

ScriptResult ScriptParser::evalVariableList(const Expression& exp) const
{
    const QString var = std::get<QString>(exp.value);
    return p->registry->varValue(var);
}

ScriptResult ScriptParser::evalFunction(const Expression& exp) const
{
    auto func = std::get<FuncValue>(exp.value);
    ScriptValueList args;
    std::ranges::transform(func.args, std::back_inserter(args),
                           [this](const Expression& arg) { return evalExpression(arg); });
    return p->registry->function(func.name, args);
}

ScriptResult ScriptParser::evalFunctionArg(const Expression& exp) const
{
    ScriptResult result;
    bool allPassed{true};

    auto arg = std::get<ExpressionList>(exp.value);
    for(const Expression& subArg : arg) {
        const auto subExpr = evalExpression(subArg);
        if(!subExpr.cond) {
            allPassed = false;
        }
        if(subExpr.value.contains(Constants::Separator)) {
            QStringList newResult;
            const auto values = subExpr.value.split(Constants::Separator);
            std::ranges::transform(values, std::back_inserter(newResult),
                                   [&](const auto& value) { return result.value + value; });
            result.value = newResult.join(Constants::Separator);
        }
        else {
            result.value = result.value + subExpr.value;
        }
    }
    result.cond = allPassed;
    return result;
}

ScriptResult ScriptParser::evalConditional(const Expression& exp) const
{
    ScriptResult result;
    QStringList exprResult;
    result.cond = true;

    auto arg = std::get<ExpressionList>(exp.value);
    for(const Expression& subArg : arg) {
        const auto subExpr = evalExpression(subArg);

        // Literals return false
        if(subArg.type != Expr::Literal) {
            if(!subExpr.cond || subExpr.value.isEmpty()) {
                // No need to evaluate rest
                result.value = {};
                result.cond  = false;
                return result;
            }
        }
        if(subExpr.value.contains(Constants::Separator)) {
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
                std::ranges::transform(exprResult, exprResult.begin(),
                                       [&](const QString& retValue) -> QString { return retValue + subExpr.value; });
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

Expression ScriptParser::expression()
{
    p->advance();
    switch(p->previous.type) {
        case(TokenType::TokVar):
            return variable();
        case(TokenType::TokFunc):
            return function();
        case(TokenType::TokQuote):
            return quote();
        case(TokenType::TokLeftSquare):
            return conditional();
        case(TokenType::TokEscape):
            p->advance();
            return literal();
        case(TokenType::TokLeftAngle):
        case(TokenType::TokRightAngle):
        case(TokenType::TokComma):
        case(TokenType::TokLeftParen):
        case(TokenType::TokRightParen):
        case(TokenType::TokRightSquare):
        case(TokenType::TokLiteral):
            return literal();
        case(TokenType::TokEos):
        case(TokenType::TokError):
            break;
    }
    // We shouldn't ever reach here
    return {Expr::Null};
}

Expression ScriptParser::literal() const
{
    return {Expr::Literal, p->previous.value.toString()};
}

Expression ScriptParser::quote()
{
    Expression expr{Expr::Literal};
    QString val;

    while(!p->currentToken(TokenType::TokQuote) && !p->currentToken(TokenType::TokEos)) {
        p->advance();
        val.append(p->previous.value.toString());
        if(p->currentToken(TokenType::TokEscape)) {
            p->advance();
            val.append(p->current.value.toString());
            p->advance();
        }
    }

    expr.value = val;
    p->consume(TokenType::TokQuote, uR"(Expected '"' after expression)"_s);
    return expr;
}

Expression ScriptParser::variable()
{
    p->advance();

    Expression expr;
    QString value;

    if(p->previous.type == TokenType::TokLeftAngle) {
        p->advance();
        expr.type = Expr::VariableList;
        value     = QString{p->previous.value.toString()}.toLower();
        p->consume(TokenType::TokRightAngle, u"Expected '>' after expression"_s);
    }
    else {
        expr.type = Expr::Variable;
        value     = QString{p->previous.value.toString()}.toLower();
    }

    if(!p->registry->varExists(value)) {
        p->error(u"Variable not found"_s);
    }

    expr.value = value;
    p->consume(TokenType::TokVar, u"Expected '%' after expression"_s);
    return expr;
}

Expression ScriptParser::function()
{
    p->advance();

    if(p->previous.type != TokenType::TokLiteral) {
        p->error(u"Expected function name"_s);
    }

    Expression expr{Expr::Function};
    FuncValue funcExpr;
    funcExpr.name = QString{p->previous.value.toString()}.toLower();

    if(!p->registry->funcExists(funcExpr.name)) {
        p->error(u"Function not found"_s);
    }

    p->consume(TokenType::TokLeftParen, u"Expected '(' after function call"_s);

    if(!p->currentToken(TokenType::TokRightParen)) {
        funcExpr.args.emplace_back(functionArgs());
        while(p->match(TokenType::TokComma)) {
            funcExpr.args.emplace_back(functionArgs());
        }
    }

    expr.value = funcExpr;
    p->consume(TokenType::TokRightParen, u"Expected ')' after function call"_s);
    return expr;
}

Expression ScriptParser::functionArgs()
{
    Expression expr{Expr::FunctionArg};
    ExpressionList funcExpr;

    while(!p->currentToken(TokenType::TokComma) && !p->currentToken(TokenType::TokRightParen)
          && !p->currentToken(TokenType::TokEos)) {
        funcExpr.emplace_back(expression());
    }

    expr.value = funcExpr;
    return expr;
}

Expression ScriptParser::conditional()
{
    Expression expr{Expr::Conditional};
    ExpressionList condExpr;

    while(!p->currentToken(TokenType::TokRightSquare) && !p->currentToken(TokenType::TokEos)) {
        condExpr.emplace_back(expression());
    }

    expr.value = condExpr;
    p->consume(TokenType::TokRightSquare, u"Expected ']' after expression"_s);
    return expr;
}

const ParsedScript& ScriptParser::lastParsedScript() const
{
    return p->parsedScript;
}
} // namespace Fooyin
