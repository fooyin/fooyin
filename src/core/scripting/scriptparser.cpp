/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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
#include <core/track.h>

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

    QString currentInput;
    std::unordered_map<QString, ParsedScript> parsedScripts;
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

        parsedScripts[currentInput].errors.emplace_back(currentError);
    }

    void error(const QString& message)
    {
        errorAt(previous, message);
    }

    ScriptResult evalExpression(const Expression& exp, const auto& tracks) const
    {
        switch(exp.type) {
            case(Expr::Literal):
                return evalLiteral(exp);
            case(Expr::Variable):
                return evalVariable(exp, tracks);
            case(Expr::VariableList):
                return evalVariableList(exp, tracks);
            case(Expr::Function):
                return evalFunction(exp, tracks);
            case(Expr::FunctionArg):
                return evalFunctionArg(exp, tracks);
            case(Expr::Conditional):
                return evalConditional(exp, tracks);
            case(Expr::Null):
            default:
                return {};
        }
    }

    ScriptResult evalLiteral(const Expression& exp) const
    {
        ScriptResult result;
        result.value = std::get<QString>(exp.value);
        result.cond  = true;
        return result;
    }

    ScriptResult evalVariable(const Expression& exp, const auto& tracks) const
    {
        const QString var   = std::get<QString>(exp.value);
        ScriptResult result = registry->value(var, tracks);

        if(result.value.contains(Constants::Separator)) {
            // TODO: Support custom separators
            result.value = result.value.replace(Constants::Separator, ", "_L1);
        }
        return result;
    }

    ScriptResult evalVariableList(const Expression& exp, const auto& tracks) const
    {
        const QString var = std::get<QString>(exp.value);
        return registry->value(var, tracks);
    }

    ScriptResult evalFunction(const Expression& exp, const auto& tracks) const
    {
        auto func = std::get<FuncValue>(exp.value);
        ScriptValueList args;
        std::ranges::transform(func.args, std::back_inserter(args),
                               [this, &tracks](const Expression& arg) { return evalExpression(arg, tracks); });
        return registry->function(func.name, args);
    }

    ScriptResult evalFunctionArg(const Expression& exp, const auto& tracks) const
    {
        ScriptResult result;
        bool allPassed{true};

        auto arg = std::get<ExpressionList>(exp.value);
        for(const Expression& subArg : arg) {
            const auto subExpr = evalExpression(subArg, tracks);
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

    ScriptResult evalConditional(const Expression& exp, const auto& tracks) const
    {
        ScriptResult result;
        QStringList exprResult;
        result.cond = true;

        auto arg = std::get<ExpressionList>(exp.value);
        for(const Expression& subArg : arg) {
            const auto subExpr = evalExpression(subArg, tracks);

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
                    std::ranges::transform(exprResult, exprResult.begin(), [&](const QString& retValue) -> QString {
                        return retValue + subExpr.value;
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

    Expression expression(const auto& tracks)
    {
        advance();
        switch(previous.type) {
            case(TokenType::TokVar):
                return variable(tracks);
            case(TokenType::TokFunc):
                return function(tracks);
            case(TokenType::TokQuote):
                return quote();
            case(TokenType::TokLeftSquare):
                return conditional(tracks);
            case(TokenType::TokEscape):
                advance();
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

    Expression literal() const
    {
        return {Expr::Literal, previous.value.toString()};
    }

    Expression quote()
    {
        Expression expr{Expr::Literal};
        QString val;

        while(!currentToken(TokenType::TokQuote) && !currentToken(TokenType::TokEos)) {
            advance();
            val.append(previous.value.toString());
            if(currentToken(TokenType::TokEscape)) {
                advance();
                val.append(current.value.toString());
                advance();
            }
        }

        expr.value = val;
        consume(TokenType::TokQuote, uR"(Expected '"' after expression)"_s);
        return expr;
    }

    Expression variable(const auto& tracks)
    {
        advance();

        Expression expr;
        QString value;

        if(previous.type == TokenType::TokLeftAngle) {
            advance();
            expr.type = Expr::VariableList;
            value     = QString{previous.value.toString()}.toLower();
            consume(TokenType::TokRightAngle, u"Expected '>' after expression"_s);
        }
        else {
            expr.type = Expr::Variable;
            value     = QString{previous.value.toString()}.toLower();
        }

        if(!registry->isVariable(value, tracks)) {
            error(u"Variable not found"_s);
        }

        expr.value = value;
        consume(TokenType::TokVar, u"Expected '%' after expression"_s);
        return expr;
    }

    Expression function(const auto& tracks)
    {
        advance();

        if(previous.type != TokenType::TokLiteral) {
            error(u"Expected function name"_s);
        }

        Expression expr{Expr::Function};
        FuncValue funcExpr;
        funcExpr.name = QString{previous.value.toString()}.toLower();

        if(!registry->isFunction(funcExpr.name)) {
            error(u"Function not found"_s);
        }

        consume(TokenType::TokLeftParen, u"Expected '(' after function call"_s);

        if(!currentToken(TokenType::TokRightParen)) {
            funcExpr.args.emplace_back(functionArgs(tracks));
            while(match(TokenType::TokComma)) {
                funcExpr.args.emplace_back(functionArgs(tracks));
            }
        }

        expr.value = funcExpr;
        consume(TokenType::TokRightParen, u"Expected ')' after function call"_s);
        return expr;
    }

    Expression functionArgs(const auto& tracks)
    {
        Expression expr{Expr::FunctionArg};
        ExpressionList funcExpr;

        while(!currentToken(TokenType::TokComma) && !currentToken(TokenType::TokRightParen)
              && !currentToken(TokenType::TokEos)) {
            funcExpr.emplace_back(expression(tracks));
        }

        expr.value = funcExpr;
        return expr;
    }

    Expression conditional(const auto& tracks)
    {
        Expression expr{Expr::Conditional};
        ExpressionList condExpr;

        while(!currentToken(TokenType::TokRightSquare) && !currentToken(TokenType::TokEos)) {
            condExpr.emplace_back(expression(tracks));
        }

        expr.value = condExpr;
        consume(TokenType::TokRightSquare, u"Expected ']' after expression"_s);
        return expr;
    }

    ParsedScript parse(const QString& input, const auto& tracks)
    {
        if(input.isEmpty() || !registry) {
            return {};
        }

        if(parsedScripts.contains(input)) {
            return parsedScripts.at(input);
        }

        currentInput = input;
        auto& script = parsedScripts[input];
        script.input = input;

        scanner.setup(input);

        advance();
        while(current.type != TokenType::TokEos) {
            script.expressions.emplace_back(expression(tracks));
        }

        consume(TokenType::TokEos, u"Expected end of expression"_s);

        return script;
    }

    QString evaluate(const ParsedScript& input, const auto& tracks)
    {
        if(!input.isValid() || !registry) {
            return {};
        }

        result.clear();

        const ExpressionList expressions = input.expressions;
        for(const auto& expr : expressions) {
            const auto evalExpr = evalExpression(expr, tracks);

            if(evalExpr.value.isNull()) {
                continue;
            }

            if(evalExpr.value.contains(Constants::Separator)) {
                const QStringList evalList = evalStringList(evalExpr, result);
                if(!evalList.empty()) {
                    result = evalList;
                }
            }
            else {
                if(result.empty()) {
                    result.push_back(evalExpr.value);
                }
                else {
                    std::ranges::transform(result, result.begin(), [&](const QString& retValue) -> QString {
                        return retValue + evalExpr.value;
                    });
                }
            }
        }

        if(result.size() == 1) {
            // Calling join on a QStringList with a single empty string will return a null QString, so return the first
            // result.
            return result.constFirst();
        }

        if(result.size() > 1) {
            return result.join(Constants::Separator);
        }

        return {};
    }
};

ScriptParser::ScriptParser(ScriptRegistry* registry)
    : p{std::make_unique<Private>(registry)}
{ }

ScriptParser::~ScriptParser() = default;

ParsedScript ScriptParser::parse(const QString& input)
{
    return p->parse(input, Track{});
}

ParsedScript ScriptParser::parse(const QString& input, const Track& track)
{
    return p->parse(input, track);
}

ParsedScript ScriptParser::parse(const QString& input, const TrackList& tracks)
{
    return p->parse(input, tracks);
}

QString ScriptParser::evaluate(const QString& input)
{
    return evaluate(input, Track{});
}

QString ScriptParser::evaluate(const ParsedScript& input)
{
    return evaluate(input, Track{});
}

QString ScriptParser::evaluate(const QString& input, const Track& track)
{
    const auto script = parse(input, track);
    return p->evaluate(script, track);
}

QString ScriptParser::evaluate(const ParsedScript& input, const Track& track)
{
    return p->evaluate(input, track);
}

QString ScriptParser::evaluate(const QString& input, const TrackList& tracks)
{
    const auto script = parse(input, tracks);
    return p->evaluate(script, tracks);
}

QString ScriptParser::evaluate(const ParsedScript& input, const TrackList& tracks)
{
    return p->evaluate(input, tracks);
}

void ScriptParser::clearCache()
{
    p->parsedScripts.clear();
}
} // namespace Fooyin
