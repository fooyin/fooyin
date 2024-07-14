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

#include <core/scripting/scriptparser.h>

#include <core/scripting/scriptscanner.h>
#include <core/track.h>

#include <QDebug>

using TokenType = Fooyin::ScriptScanner::TokenType;

namespace {
QStringList evalStringList(const Fooyin::ScriptResult& evalExpr, const QStringList& result)
{
    QStringList listResult;
    const QStringList values = evalExpr.value.split(QStringLiteral("\037"));
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
class ScriptParserPrivate
{
public:
    explicit ScriptParserPrivate(ScriptParser* self);
    ScriptParserPrivate(ScriptParser* self, ScriptRegistry* registry);

    void advance();
    void consume(TokenType type, const QString& message);
    [[nodiscard]] bool currentToken(TokenType type) const;
    bool match(TokenType type);

    void errorAtCurrent(const QString& message);
    void errorAt(const ScriptScanner::Token& token, const QString& message);
    void error(const QString& message);

    Expression expression(const auto& tracks);
    Expression literal() const;
    Expression quote();
    Expression variable();
    Expression function(const auto& tracks);
    Expression functionArgs(const auto& tracks);
    Expression conditional(const auto& tracks);

    ScriptResult evalExpression(const Expression& exp, const auto& tracks) const;
    ScriptResult evalLiteral(const Expression& exp) const;
    ScriptResult evalVariable(const Expression& exp, const auto& tracks) const;
    ScriptResult evalVariableList(const Expression& exp, const auto& tracks) const;
    ScriptResult evalFunction(const Expression& exp, const auto& tracks) const;
    ScriptResult evalFunctionArg(const Expression& exp, const auto& tracks) const;
    ScriptResult evalConditional(const Expression& exp, const auto& tracks) const;

    ParsedScript parse(const QString& input, const auto& tracks);
    QString evaluate(const ParsedScript& input, const auto& tracks);

    ScriptParser* m_self;

    ScriptScanner m_scanner;
    std::unique_ptr<ScriptRegistry> m_defaultRegistry;
    ScriptRegistry* m_registry;

    ScriptScanner::Token m_current;
    ScriptScanner::Token m_previous;

    QString m_currentInput;
    std::unordered_map<QString, ParsedScript> m_parsedScripts;
    QStringList m_currentResult;
};

ScriptParserPrivate::ScriptParserPrivate(ScriptParser* self)
    : m_self{self}
    , m_defaultRegistry{std::make_unique<ScriptRegistry>()}
    , m_registry{m_defaultRegistry.get()}
{ }

ScriptParserPrivate::ScriptParserPrivate(ScriptParser* self, ScriptRegistry* registry)
    : m_self{self}
    , m_registry{registry}
{ }

void ScriptParserPrivate::advance()
{
    m_previous = m_current;

    m_current = m_scanner.next();
    if(m_current.type == TokenType::TokError) {
        errorAtCurrent(m_current.value.toString());
    }
}

void ScriptParserPrivate::consume(TokenType type, const QString& message)
{
    if(m_current.type == type) {
        advance();
        return;
    }
    errorAtCurrent(message);
}

bool ScriptParserPrivate::currentToken(TokenType type) const
{
    return m_current.type == type;
}

bool ScriptParserPrivate::match(TokenType type)
{
    if(!currentToken(type)) {
        return false;
    }
    advance();
    return true;
}

void ScriptParserPrivate::errorAtCurrent(const QString& message)
{
    errorAt(m_current, message);
}

void ScriptParserPrivate::errorAt(const ScriptScanner::Token& token, const QString& message)
{
    QString errorMsg = QStringLiteral("[%1] Error").arg(token.position);

    if(token.type == TokenType::TokEos) {
        errorMsg += QStringLiteral(" at end of string");
    }
    else {
        errorMsg += QStringLiteral(": '") + token.value.toString() + QStringLiteral("'");
    }

    errorMsg += QStringLiteral(" (%1)").arg(message);

    ScriptError currentError;
    currentError.value    = token.value.toString();
    currentError.position = token.position;
    currentError.message  = errorMsg;

    m_parsedScripts[m_currentInput].errors.emplace_back(currentError);
}

void ScriptParserPrivate::error(const QString& message)
{
    errorAt(m_previous, message);
}

Expression ScriptParserPrivate::expression(const auto& tracks)
{
    advance();
    switch(m_previous.type) {
        case(TokenType::TokVar):
            return variable();
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
        case(TokenType::TokSlash):
        case(TokenType::TokColon):
        case(TokenType::TokEquals):
        case(TokenType::TokLiteral):
            return literal();
        case(TokenType::TokEos):
        case(TokenType::TokError):
            break;
    }
    // We shouldn't ever reach here
    return {Expr::Null};
}

Expression ScriptParserPrivate::literal() const
{
    return {Expr::Literal, m_previous.value.toString()};
}

Expression ScriptParserPrivate::quote()
{
    Expression expr{Expr::Literal};
    QString val;

    while(!currentToken(TokenType::TokQuote) && !currentToken(TokenType::TokEos)) {
        advance();
        val.append(m_previous.value.toString());
        if(currentToken(TokenType::TokEscape)) {
            advance();
            val.append(m_current.value.toString());
            advance();
        }
    }

    expr.value = val;
    consume(TokenType::TokQuote, QString::fromUtf8(R"(Expected '"' after expression)"));
    return expr;
}

Expression ScriptParserPrivate::variable()
{
    advance();

    Expression expr;
    QString value;

    if(m_previous.type == TokenType::TokLeftAngle) {
        advance();
        expr.type = Expr::VariableList;
        value     = QString{m_previous.value.toString()}.toLower();
        consume(TokenType::TokRightAngle, QStringLiteral("Expected '>' after expression"));
    }
    else {
        expr.type = Expr::Variable;
        value     = QString{m_previous.value.toString()}.toLower();
    }

    expr.value = value;
    consume(TokenType::TokVar, QStringLiteral("Expected '%' after expression"));
    return expr;
}

Expression ScriptParserPrivate::function(const auto& tracks)
{
    advance();

    if(m_previous.type != TokenType::TokLiteral) {
        error(QStringLiteral("Expected function name"));
    }

    Expression expr{Expr::Function};
    FuncValue funcExpr;
    funcExpr.name = QString{m_previous.value.toString()}.toLower();

    if(!m_registry->isFunction(funcExpr.name)) {
        error(QStringLiteral("Function not found"));
    }

    consume(TokenType::TokLeftParen, QStringLiteral("Expected '(' after function call"));

    if(!currentToken(TokenType::TokRightParen)) {
        funcExpr.args.emplace_back(functionArgs(tracks));
        while(match(TokenType::TokComma)) {
            funcExpr.args.emplace_back(functionArgs(tracks));
        }
    }

    expr.value = funcExpr;
    consume(TokenType::TokRightParen, QStringLiteral("Expected ')' after function call"));
    return expr;
}

Expression ScriptParserPrivate::functionArgs(const auto& tracks)
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

Expression ScriptParserPrivate::conditional(const auto& tracks)
{
    Expression expr{Expr::Conditional};
    ExpressionList condExpr;

    while(!currentToken(TokenType::TokRightSquare) && !currentToken(TokenType::TokEos)) {
        condExpr.emplace_back(expression(tracks));
    }

    expr.value = condExpr;
    consume(TokenType::TokRightSquare, QStringLiteral("Expected ']' after expression"));
    return expr;
}

ScriptResult ScriptParserPrivate::evalExpression(const Expression& exp, const auto& tracks) const
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

ScriptResult ScriptParserPrivate::evalLiteral(const Expression& exp) const
{
    ScriptResult result;
    result.value = std::get<QString>(exp.value);
    result.cond  = true;
    return result;
}

ScriptResult ScriptParserPrivate::evalVariable(const Expression& exp, const auto& tracks) const
{
    const QString var   = std::get<QString>(exp.value);
    ScriptResult result = m_registry->value(var, tracks);

    if(!result.cond) {
        return {};
    }

    if(result.value.contains(u"\037")) {
        result.value = result.value.replace(QStringLiteral("\037"), QStringLiteral(", "));
    }

    return result;
}

ScriptResult ScriptParserPrivate::evalVariableList(const Expression& exp, const auto& tracks) const
{
    const QString var = std::get<QString>(exp.value);
    return m_registry->value(var, tracks);
}

ScriptResult ScriptParserPrivate::evalFunction(const Expression& exp, const auto& tracks) const
{
    auto func = std::get<FuncValue>(exp.value);
    ScriptValueList args;
    std::ranges::transform(func.args, std::back_inserter(args),
                           [this, &tracks](const Expression& arg) { return evalExpression(arg, tracks); });
    return m_registry->function(func.name, args, tracks);
}

ScriptResult ScriptParserPrivate::evalFunctionArg(const Expression& exp, const auto& tracks) const
{
    ScriptResult result;
    bool allPassed{true};

    auto arg = std::get<ExpressionList>(exp.value);
    for(const Expression& subArg : arg) {
        const auto subExpr = evalExpression(subArg, tracks);
        if(!subExpr.cond) {
            allPassed = false;
        }
        if(subExpr.value.contains(u"\037")) {
            QStringList newResult;
            const auto values = subExpr.value.split(QStringLiteral("\037"));
            std::ranges::transform(values, std::back_inserter(newResult),
                                   [&](const auto& value) { return result.value + value; });
            result.value = newResult.join(u"\037");
        }
        else {
            result.value = result.value + subExpr.value;
        }
    }
    result.cond = allPassed;
    return result;
}

ScriptResult ScriptParserPrivate::evalConditional(const Expression& exp, const auto& tracks) const
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
                result.value.clear();
                result.cond = false;
                return result;
            }
        }
        if(subExpr.value.contains(u"\037")) {
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
        result.value = exprResult.join(u"\037");
    }
    return result;
}

ParsedScript ScriptParserPrivate::parse(const QString& input, const auto& tracks)
{
    if(input.isEmpty() || !m_registry) {
        return {};
    }

    if(m_parsedScripts.contains(input)) {
        return m_parsedScripts.at(input);
    }

    m_currentInput = input;
    auto& script   = m_parsedScripts[input];
    script.input   = input;

    m_scanner.setup(input);

    advance();
    while(m_current.type != TokenType::TokEos) {
        script.expressions.emplace_back(expression(tracks));
    }

    consume(TokenType::TokEos, QStringLiteral("Expected end of expression"));

    return script;
}

QString ScriptParserPrivate::evaluate(const ParsedScript& input, const auto& tracks)
{
    if(!input.isValid() || !m_registry) {
        return {};
    }

    m_currentResult.clear();

    const ExpressionList expressions = input.expressions;
    for(const auto& expr : expressions) {
        const auto evalExpr = evalExpression(expr, tracks);

        if(evalExpr.value.isNull()) {
            continue;
        }

        if(evalExpr.value.contains(u"\037")) {
            const QStringList evalList = evalStringList(evalExpr, m_currentResult);
            if(!evalList.empty()) {
                m_currentResult = evalList;
            }
        }
        else {
            if(m_currentResult.empty()) {
                m_currentResult.push_back(evalExpr.value);
            }
            else {
                std::ranges::transform(m_currentResult, m_currentResult.begin(),
                                       [&](const QString& retValue) -> QString { return retValue + evalExpr.value; });
            }
        }
    }

    if(m_currentResult.size() == 1) {
        // Calling join on a QStringList with a single empty string will return a null QString, so return the first
        // result.
        return m_currentResult.constFirst();
    }

    if(m_currentResult.size() > 1) {
        return m_currentResult.join(u"\037");
    }

    return {};
}

ScriptParser::ScriptParser()
    : p{std::make_unique<ScriptParserPrivate>(this)}
{ }

ScriptParser::ScriptParser(ScriptRegistry* registry)
    : p{std::make_unique<ScriptParserPrivate>(this, registry)}
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
    if(tracks.empty()) {
        return {};
    }

    return p->parse(input, tracks);
}

QString ScriptParser::evaluate(const QString& input)
{
    return evaluate(input, Track{});
}

QString ScriptParser::evaluate(const ParsedScript& input)
{
    if(!input.isValid()) {
        return {};
    }

    return evaluate(input, Track{});
}

QString ScriptParser::evaluate(const QString& input, const Track& track)
{
    if(input.isEmpty()) {
        return {};
    }

    const auto script = parse(input, track);
    return p->evaluate(script, track);
}

QString ScriptParser::evaluate(const ParsedScript& input, const Track& track)
{
    if(!input.isValid()) {
        return {};
    }

    return p->evaluate(input, track);
}

QString ScriptParser::evaluate(const QString& input, const TrackList& tracks)
{
    if(input.isEmpty()) {
        return {};
    }

    const auto script = parse(input, tracks);
    return p->evaluate(script, tracks);
}

QString ScriptParser::evaluate(const ParsedScript& input, const TrackList& tracks)
{
    if(!input.isValid()) {
        return {};
    }

    return p->evaluate(input, tracks);
}

void ScriptParser::clearCache()
{
    p->m_parsedScripts.clear();
}
} // namespace Fooyin
