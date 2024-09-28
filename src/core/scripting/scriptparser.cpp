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

#include <core/constants.h>
#include <core/library/tracksort.h>
#include <core/scripting/scriptscanner.h>
#include <core/track.h>
#include <utils/helpers.h>

#include <QDateTime>
#include <QDebug>

using TokenType = Fooyin::ScriptScanner::TokenType;

namespace {
constexpr std::array<const char*, 6> dateFormats{"yyyy-MM-dd hh:mm:ss", "yyyy-MM-dd hh:mm", "yyyy-MM-dd hh",
                                                 "yyyy-MM-dd",          "yyyy-MM",          "yyyy"};

QDateTime evalDate(const QString& str)
{
    for(const auto& format : dateFormats) {
        const QDateTime date = QDateTime::fromString(str, QLatin1String{format});
        if(date.isValid()) {
            return date;
        }
    }

    return {};
}

QDateTime evalDate(const Fooyin::Expression& expr)
{
    if(!std::holds_alternative<QString>(expr.value)) {
        return {};
    }

    const QString value = std::get<QString>(expr.value);
    return evalDate(value);
}

struct DateRange
{
    QDateTime start;
    QDateTime end;
};

DateRange calculateDateRange(const Fooyin::Expression& expr)
{
    if(!std::holds_alternative<QString>(expr.value)) {
        return {};
    }

    const QString dateString = std::get<QString>(expr.value);

    DateRange range;

    for(const auto& format : dateFormats) {
        const auto date = QDateTime::fromString(dateString, QLatin1String{format});
        if(!date.isValid()) {
            continue;
        }

        range.start = date;

        if(strcmp(format, "yyyy") == 0) {
            range.end = range.start.addYears(1).addSecs(-1);
        }
        else if(strcmp(format, "yyyy-MM") == 0) {
            range.end = range.start.addMonths(1).addSecs(-1);
        }
        else if(strcmp(format, "yyyy-MM-dd") == 0) {
            range.end = range.start.addDays(1).addSecs(-1);
        }
        else if(strcmp(format, "yyyy-MM-dd hh") == 0) {
            range.end = range.start.addSecs(3600 - 1);
        }
        else if(strcmp(format, "yyyy-MM-dd hh:mm") == 0) {
            range.end = range.start.addSecs(60 - 1);
        }
        else if(strcmp(format, "yyyy-MM-dd hh:mm:ss") == 0) {
            range.end = range.start;
        }
    }

    return range;
}

QStringList evalStringList(const Fooyin::ScriptResult& evalExpr, const QStringList& result)
{
    QStringList listResult;
    const QStringList values = evalExpr.value.split(QLatin1String{Fooyin::Constants::UnitSeparator});
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

bool matchSearch(const Fooyin::Track& track, const QString& search, bool singleString)
{
    if(search.isEmpty()) {
        return true;
    }

    if(singleString) {
        return track.hasMatch(search);
    }

    const QStringList terms = search.split(u' ', Qt::SkipEmptyParts);
    return std::ranges::all_of(terms, [&track](const QString& term) { return track.hasMatch(term); });
}

bool isQueryExpression(Fooyin::Expr::Type type)
{
    using Type = Fooyin::Expr::Type;
    switch(type) {
        case(Type::Not):
        case(Type::Group):
        case(Type::And):
        case(Type::Or):
        case(Type::XOr):
        case(Type::Equals):
        case(Type::Contains):
        case(Type::Greater):
        case(Type::GreaterEqual):
        case(Type::Less):
        case(Type::LessEqual):
        case(Type::SortAscending):
        case(Type::SortDescending):
        case(Type::All):
        case(Type::Missing):
        case(Type::Present):
        case(Type::Before):
        case(Type::After):
        case(Type::Since):
        case(Type::During):
        case(Type::Date):
            return true;
        case(Type::Null):
        case(Type::Literal):
        case(Type::Variable):
        case(Type::VariableList):
        case(Type::VariableRaw):
        case(Type::Function):
        case(Type::FunctionArg):
        case(Type::Conditional):
        case(Type::QuotedLiteral):
            break;
    }

    return false;
}
} // namespace

namespace Fooyin {
class ScriptParserPrivate
{
public:
    ScriptParserPrivate(ScriptParser* self, ScriptRegistry* registry);

    void advance();
    void consume(TokenType type, const QString& message);
    [[nodiscard]] bool currentToken(TokenType type) const;
    bool match(TokenType type);

    void errorAtCurrent(const QString& message);
    void errorAt(const ScriptScanner::Token& token, const QString& message);
    void error(const QString& message);

    Expression expression();
    Expression literal();
    Expression quote();
    Expression variable();
    Expression function();
    Expression functionArgs();
    Expression conditional();
    Expression group();
    Expression notKeyword(const Expression& key);
    Expression andKeyword(const Expression& key);
    Expression orKeyword(const Expression& key);
    Expression xorKeyword(const Expression& key);
    Expression missingKeyword(const Expression& key);
    Expression presentKeyword(const Expression& key);
    Expression beforeKeyword(const Expression& key);
    Expression afterKeyword(const Expression& key);
    Expression sinceKeyword(const Expression& key);
    Expression duringKeyword(const Expression& key);
    Expression equals(const Expression& key);
    Expression contains(const Expression& key);
    Expression greater(const Expression& key);
    Expression less(const Expression& key);
    Expression sort();

    ScriptResult evalExpression(const Expression& exp, const auto& tracks) const;
    ScriptResult evalLiteral(const Expression& exp) const;
    ScriptResult evalVariable(const Expression& exp, const auto& tracks) const;
    ScriptResult evalVariableList(const Expression& exp, const auto& tracks) const;
    ScriptResult evalVariableRaw(const Expression& exp, const auto& tracks) const;
    ScriptResult evalFunction(const Expression& exp, const auto& tracks) const;
    ScriptResult evalFunctionArg(const Expression& exp, const auto& tracks) const;
    ScriptResult evalConditional(const Expression& exp, const auto& tracks) const;
    ScriptResult evalNot(const Expression& exp, const auto& tracks) const;
    ScriptResult evalGroup(const Expression& exp, const auto& tracks) const;
    ScriptResult evalAnd(const Expression& exp, const auto& tracks) const;
    ScriptResult evalOr(const Expression& exp, const auto& tracks) const;
    ScriptResult evalXOr(const Expression& exp, const auto& tracks) const;
    ScriptResult evalMissing(const Expression& exp, const auto& tracks) const;
    ScriptResult evalPresent(const Expression& exp, const auto& tracks) const;
    ScriptResult evalEquals(const Expression& exp, const auto& tracks) const;
    ScriptResult evalContains(const Expression& exp, const auto& tracks) const;

    ParsedScript parse(const QString& input);
    ParsedScript parseQuery(const QString& input);
    QString evaluate(const ParsedScript& input, const auto& tracks);
    TrackList evaluateQuery(const ParsedScript& input, const TrackList& tracks);

    ScriptResult compareValues(const Expression& exp, const auto& tracks, const auto& comparator) const;
    ScriptResult compareDates(const Expression& exp, const auto& tracks, const auto& comparator) const;
    ScriptResult compareDateRange(const Expression& exp, const auto& tracks) const;
    Expression checkOperator(const Expression& expr);

    ScriptParser* m_self;

    ScriptScanner m_scanner;
    std::unique_ptr<ScriptRegistry> m_registry;

    ScriptScanner::Token m_current;
    ScriptScanner::Token m_previous;

    bool m_isQuery{false};
    QString m_currentInput;
    std::unordered_map<QString, ParsedScript> m_parsedScripts;
    QStringList m_currentResult;
};

ScriptParserPrivate::ScriptParserPrivate(ScriptParser* self, ScriptRegistry* registry)
    : m_self{self}
{
    if(registry) {
        m_registry.reset(registry);
    }
    else {
        m_registry = std::make_unique<ScriptRegistry>();
    }
}

void ScriptParserPrivate::advance()
{
    m_previous = m_current;

    m_current = m_scanner.next();
    if(m_current.type == TokenType::TokError) {
        errorAtCurrent(m_current.value);
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
        errorMsg += QStringLiteral(": '") + token.value + QStringLiteral("'");
    }

    errorMsg += QStringLiteral(" (%1)").arg(message);

    ScriptError currentError;
    currentError.value    = token.value;
    currentError.position = token.position;
    currentError.message  = errorMsg;

    m_parsedScripts[m_currentInput].errors.emplace_back(currentError);
}

void ScriptParserPrivate::error(const QString& message)
{
    errorAt(m_previous, message);
}

Expression ScriptParserPrivate::expression()
{
    advance();

    switch(m_previous.type) {
        case(TokenType::TokVar):
            return variable();
        case(TokenType::TokFunc):
            return function();
        case(TokenType::TokQuote):
            return quote();
        case(TokenType::TokLeftSquare):
            return conditional();
        case(TokenType::TokEscape):
            advance();
            return literal();
        case(TokenType::TokLeftParen):
            return m_isQuery ? group() : literal();
        case(TokenType::TokNot):
            return m_isQuery ? notKeyword({}) : literal();
        case(TokenType::TokAll):
            return m_isQuery ? Expression{Expr::All} : literal();
        case(TokenType::TokAnd):
        case(TokenType::TokOr):
        case(TokenType::TokXOr):
        case(TokenType::TokMissing):
        case(TokenType::TokPresent):
        case(TokenType::TokColon):
        case(TokenType::TokEquals):
        case(TokenType::TokLeftAngle):
        case(TokenType::TokRightAngle):
        case(TokenType::TokRightParen):
        case(TokenType::TokComma):
        case(TokenType::TokSort):
            return m_isQuery ? sort() : literal();
        case(TokenType::TokBy):
        case(TokenType::TokBefore):
        case(TokenType::TokAfter):
        case(TokenType::TokSince):
        case(TokenType::TokDuring):
        case(TokenType::TokLast):
        case(TokenType::TokSecond):
        case(TokenType::TokMinute):
        case(TokenType::TokHour):
        case(TokenType::TokDay):
        case(TokenType::TokWeek):
        case(TokenType::TokRightSquare):
        case(TokenType::TokAscending):
        case(TokenType::TokDescending):
        case(TokenType::TokSlash):
        case(TokenType::TokLiteral):
            return literal();
        case(TokenType::TokEos):
        case(TokenType::TokError):
            break;
    }

    return {Expr::Null};
}

Expression ScriptParserPrivate::literal()
{
    QString value = m_previous.value;

    if(m_isQuery) {
        value = value.trimmed();
        if(value.isEmpty()) {
            return {};
        }
    }

    return {Expr::Literal, value};
}

Expression ScriptParserPrivate::quote()
{
    Expression expr{Expr::QuotedLiteral};
    QString val;

    while(!currentToken(TokenType::TokQuote) && !currentToken(TokenType::TokEos)) {
        advance();
        val.append(m_previous.value);
        if(currentToken(TokenType::TokEscape)) {
            advance();
            val.append(m_current.value);
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
        value     = QString{m_previous.value}.toLower();
        consume(TokenType::TokRightAngle, QStringLiteral("Expected '>' after expression"));
    }
    else {
        expr.type = Expr::Variable;
        value     = m_previous.value;
        if(m_isQuery) {
            value = value.trimmed();
        }
    }

    expr.value = value;
    consume(TokenType::TokVar, QStringLiteral("Expected '%' after expression"));
    return expr;
}

Expression ScriptParserPrivate::function()
{
    advance();

    if(m_previous.type != TokenType::TokLiteral) {
        error(QStringLiteral("Expected function name"));
    }

    Expression expr{Expr::Function};
    FuncValue funcExpr;
    funcExpr.name = QString{m_previous.value}.toLower();

    if(!m_registry->isFunction(funcExpr.name)) {
        error(QStringLiteral("Function not found"));
    }

    consume(TokenType::TokLeftParen, QStringLiteral("Expected '(' after function call"));

    if(!currentToken(TokenType::TokRightParen)) {
        funcExpr.args.emplace_back(functionArgs());
        while(match(TokenType::TokComma)) {
            funcExpr.args.emplace_back(functionArgs());
        }
    }

    expr.value = funcExpr;
    consume(TokenType::TokRightParen, QStringLiteral("Expected ')' after function call"));
    return expr;
}

Expression ScriptParserPrivate::functionArgs()
{
    Expression expr{Expr::FunctionArg};
    ExpressionList funcExpr;

    while(!currentToken(TokenType::TokComma) && !currentToken(TokenType::TokRightParen)
          && !currentToken(TokenType::TokEos)) {
        const Expression argExpr = expression();
        if(argExpr.type != Expr::Null) {
            funcExpr.emplace_back(argExpr);
        }
    }

    expr.value = funcExpr;
    return expr;
}

Expression ScriptParserPrivate::conditional()
{
    Expression expr{Expr::Conditional};
    ExpressionList condExpr;

    while(!currentToken(TokenType::TokRightSquare) && !currentToken(TokenType::TokEos)) {
        const Expression argExpr = expression();
        if(argExpr.type != Expr::Null) {
            condExpr.emplace_back(argExpr);
        }
    }

    expr.value = condExpr;
    consume(TokenType::TokRightSquare, QStringLiteral("Expected ']' after expression"));
    return expr;
}

Expression ScriptParserPrivate::group()
{
    Expression expr{Expr::Group};
    ExpressionList args;

    while(!currentToken(TokenType::TokRightParen) && !currentToken(TokenType::TokEos)) {
        Expression prevExpr = expression();
        Expression argExpr  = checkOperator(prevExpr);

        while(argExpr.type != prevExpr.type) {
            prevExpr = argExpr;
            argExpr  = checkOperator(prevExpr);
        }

        if(argExpr.type != Expr::Null) {
            args.emplace_back(argExpr);
        }
    }

    if(args.size() == 1) {
        const auto& firstArg = args.front();
        if(firstArg.type == Expr::Literal || firstArg.type == Expr::QuotedLiteral) {
            expr.type  = firstArg.type;
            expr.value = QStringLiteral("(%1)").arg(std::get<QString>(firstArg.value));
            consume(TokenType::TokRightParen, QStringLiteral("Expected ')' after expression"));
            return expr;
        }
    }

    expr.value = args;
    consume(TokenType::TokRightParen, QStringLiteral("Expected ')' after expression"));
    return expr;
}

Expression ScriptParserPrivate::notKeyword(const Expression& key)
{
    Expression expr{Expr::Not};
    ExpressionList args;

    if(currentToken(TokenType::TokNot)) {
        consume(TokenType::TokNot, QStringLiteral("Expected '!' after expression"));

        if(currentToken(TokenType::TokEquals)) {
            consume(TokenType::TokEquals, QStringLiteral("Expected '=' after expression"));

            Expression equals{Expr::Equals};
            ExpressionList equalsArgs;
            Expression field{key};
            if(field.type == Expr::Literal) {
                field.type = Expr::Variable;
            }
            equalsArgs.emplace_back(field);

            const Expression argExpr = expression();
            if(argExpr.type != Expr::Null) {
                equalsArgs.emplace_back(argExpr);
            }

            equals.value = equalsArgs;
            args.emplace_back(equals);
        }
    }
    else {
        const Expression argExpr = currentToken(TokenType::TokLeftParen) ? expression() : checkOperator(expression());
        if(argExpr.type != Expr::Null) {
            args.emplace_back(argExpr);
        }
    }

    expr.value = args;
    return expr;
}

Expression ScriptParserPrivate::andKeyword(const Expression& key)
{
    Expression expr{Expr::And};
    ExpressionList args;

    advance();
    args.emplace_back(key);

    if(!currentToken(TokenType::TokEos)) {
        const Expression argExpr = checkOperator(expression());
        if(argExpr.type != Expr::Null) {
            args.emplace_back(argExpr);
        }
    }

    expr.value = args;
    return expr;
}

Expression ScriptParserPrivate::orKeyword(const Expression& key)
{
    Expression expr{Expr::Or};
    ExpressionList args;

    advance();
    args.emplace_back(key);

    if(!currentToken(TokenType::TokEos)) {
        const Expression argExpr = checkOperator(expression());
        if(argExpr.type != Expr::Null) {
            args.emplace_back(argExpr);
        }
    }

    expr.value = args;
    return expr;
}

Expression ScriptParserPrivate::xorKeyword(const Expression& key)
{
    Expression expr{Expr::XOr};
    ExpressionList args;

    advance();
    args.emplace_back(key);

    if(!currentToken(TokenType::TokEos)) {
        const Expression argExpr = checkOperator(expression());
        if(argExpr.type != Expr::Null) {
            args.emplace_back(argExpr);
        }
    }

    expr.value = args;
    return expr;
}

Expression ScriptParserPrivate::missingKeyword(const Expression& key)
{
    Expression expr{Expr::Missing};
    ExpressionList args;

    advance();

    Expression field{key};
    if(field.type == Expr::Literal) {
        field.type = Expr::Variable;
    }
    args.emplace_back(field);

    expr.value = args;
    return expr;
}

Expression ScriptParserPrivate::presentKeyword(const Expression& key)
{
    Expression expr{Expr::Present};
    ExpressionList args;

    advance();

    Expression field{key};
    if(field.type == Expr::Literal) {
        field.type = Expr::Variable;
    }
    args.emplace_back(field);

    expr.value = args;
    return expr;
}

Expression ScriptParserPrivate::beforeKeyword(const Expression& key)
{
    Expression expr{Expr::Before};
    ExpressionList args;

    advance();

    Expression field{key};
    if(field.type == Expr::Literal) {
        field.type = Expr::Variable;
    }
    args.emplace_back(field);

    if(!currentToken(TokenType::TokEos)) {
        Expression argExpr   = expression();
        const QDateTime date = evalDate(argExpr);
        if(date.isValid()) {
            argExpr.type  = Expr::Date;
            argExpr.value = QString::number(date.toMSecsSinceEpoch());
            args.emplace_back(argExpr);
        }
    }

    expr.value = args;
    return expr;
}

Expression ScriptParserPrivate::afterKeyword(const Expression& key)
{
    Expression expr{Expr::After};
    ExpressionList args;

    advance();

    Expression field{key};
    if(field.type == Expr::Literal) {
        field.type = Expr::Variable;
    }
    args.emplace_back(field);

    if(!currentToken(TokenType::TokEos)) {
        Expression argExpr   = expression();
        const QDateTime date = evalDate(argExpr);
        if(date.isValid()) {
            argExpr.type  = Expr::Date;
            argExpr.value = QString::number(date.toMSecsSinceEpoch());
            args.emplace_back(argExpr);
        }
    }

    expr.value = args;
    return expr;
}

Expression ScriptParserPrivate::sinceKeyword(const Expression& key)
{
    Expression expr{Expr::Since};
    ExpressionList args;

    advance();

    Expression field{key};
    if(field.type == Expr::Literal) {
        field.type = Expr::Variable;
    }
    args.emplace_back(field);

    if(!currentToken(TokenType::TokEos)) {
        Expression argExpr   = expression();
        const QDateTime date = evalDate(argExpr);
        if(date.isValid()) {
            argExpr.type  = Expr::Date;
            argExpr.value = QString::number(date.toMSecsSinceEpoch());
            args.emplace_back(argExpr);
        }
    }

    expr.value = args;
    return expr;
}

Expression ScriptParserPrivate::duringKeyword(const Expression& key)
{
    Expression expr{Expr::During};
    ExpressionList args;

    advance();

    Expression field{key};
    if(field.type == Expr::Literal) {
        field.type = Expr::Variable;
    }
    args.emplace_back(field);

    if(currentToken(TokenType::TokLast)) {
        advance();

        bool valid{false};
        bool validUserCount{true};
        int count{1};
        QDateTime date = QDateTime::currentDateTime();

        if(currentToken(TokenType::TokLiteral)) {
            const auto countExpr = evalLiteral(expression());
            if(!countExpr.cond) {
                return {};
            }

            const int userCount = countExpr.value.toInt();
            if(userCount > 0 && userCount < std::numeric_limits<int>::max()) {
                count = userCount;
            }
            else {
                validUserCount = false;
            }
        }

        if(currentToken(TokenType::TokWeek)) {
            date  = date.addDays(-7LL * count);
            valid = true;
        }
        else if(currentToken(TokenType::TokDay)) {
            date  = date.addDays(-1LL * count);
            valid = true;
        }
        else if(currentToken(TokenType::TokHour)) {
            date  = date.addSecs(-3600LL * count);
            valid = true;
        }
        else if(currentToken(TokenType::TokMinute)) {
            date  = date.addSecs(-60LL * count);
            valid = true;
        }
        else if(currentToken(TokenType::TokSecond)) {
            date  = date.addSecs(-1LL * count);
            valid = true;
        }

        if(valid) {
            advance();
            if(validUserCount) {
                args.emplace_back(Expr::Date, QString::number(date.toMSecsSinceEpoch()));
                args.emplace_back(Expr::Date, QString::number(QDateTime::currentDateTime().toMSecsSinceEpoch()));
            }
            else {
                return {};
            }
        }
    }
    else if(!currentToken(TokenType::TokEos)) {
        const Expression argExpr  = expression();
        const DateRange dateRange = calculateDateRange(argExpr);
        if(dateRange.start.isValid() && dateRange.end.isValid()) {
            args.emplace_back(Expr::Date, QString::number(dateRange.start.toMSecsSinceEpoch()));
            args.emplace_back(Expr::Date, QString::number(dateRange.end.toMSecsSinceEpoch()));
        }
    }

    expr.value = args;
    return expr;
}

Expression ScriptParserPrivate::equals(const Expression& key)
{
    Expression expr{Expr::Equals};
    ExpressionList args;

    advance();

    Expression field{key};
    if(field.type == Expr::Literal) {
        field.type = Expr::Variable;
    }
    args.emplace_back(field);

    if(!currentToken(TokenType::TokEos)) {
        const Expression argExpr = expression();
        if(argExpr.type != Expr::Null) {
            args.emplace_back(argExpr);
        }
    }

    expr.value = args;
    return expr;
}

Expression ScriptParserPrivate::contains(const Expression& key)
{
    Expression expr{Expr::Contains};
    ExpressionList args;

    advance();

    Expression field{key};
    if(field.type == Expr::Literal) {
        field.type = Expr::Variable;
    }
    args.emplace_back(field);

    if(!currentToken(TokenType::TokEos)) {
        const Expression argExpr = expression();
        if(argExpr.type != Expr::Null) {
            args.emplace_back(argExpr);
        }
    }

    expr.value = args;
    return expr;
}

Expression ScriptParserPrivate::greater(const Expression& key)
{
    Expression expr{Expr::Greater};
    ExpressionList args;

    advance();

    Expression field{key};
    if(field.type == Expr::Literal) {
        field.type = Expr::Variable;
    }
    args.emplace_back(field);

    if(match(TokenType::TokEquals)) {
        expr.type = Expr::GreaterEqual;
    }

    if(!currentToken(TokenType::TokEos)) {
        const Expression argExpr = expression();
        if(argExpr.type != Expr::Null) {
            args.emplace_back(argExpr);
        }
    }

    expr.value = args;
    return expr;
}

Expression ScriptParserPrivate::less(const Expression& key)
{
    Expression expr{Expr::Less};
    ExpressionList args;

    advance();

    Expression field{key};
    if(field.type == Expr::Literal) {
        field.type = Expr::Variable;
    }
    args.emplace_back(field);

    if(match(TokenType::TokEquals)) {
        expr.type = Expr::LessEqual;
    }

    if(!currentToken(TokenType::TokEos)) {
        const Expression argExpr = expression();
        if(argExpr.type != Expr::Null) {
            args.emplace_back(argExpr);
        }
    }

    expr.value = args;
    return expr;
}

Expression ScriptParserPrivate::sort()
{
    Expression expr;
    QString val;

    if(currentToken(TokenType::TokBy)) {
        advance();
        expr.type = Expr::SortAscending;
    }
    else if(currentToken(TokenType::TokAscending)) {
        advance();
        expr.type = Expr::SortAscending;
        consume(TokenType::TokBy, QStringLiteral("Expected 'BY' after 'ASCENDING'"));
    }
    else if(currentToken(TokenType::TokDescending)) {
        advance();
        expr.type = Expr::SortDescending;
        consume(TokenType::TokBy, QStringLiteral("Expected 'BY' after 'DESCENDING'"));
    }

    while(!currentToken(TokenType::TokEos)) {
        advance();
        val.append(m_previous.value);
    }

    expr.value = val;
    consume(TokenType::TokEos, QString::fromUtf8(R"(Expected EOS after sort expression)"));
    return expr;
}

ScriptResult ScriptParserPrivate::evalExpression(const Expression& exp, const auto& tracks) const
{
    switch(exp.type) {
        case(Expr::Literal):
        case(Expr::QuotedLiteral):
            return evalLiteral(exp);
        case(Expr::Variable):
            return evalVariable(exp, tracks);
        case(Expr::VariableList):
            return evalVariableList(exp, tracks);
        case(Expr::VariableRaw):
            return evalVariableRaw(exp, tracks);
        case(Expr::Function):
            return evalFunction(exp, tracks);
        case(Expr::FunctionArg):
            return evalFunctionArg(exp, tracks);
        case(Expr::Conditional):
            return evalConditional(exp, tracks);
        case(Expr::Not):
            return evalNot(exp, tracks);
        case(Expr::Group):
            return evalGroup(exp, tracks);
        case(Expr::And):
            return evalAnd(exp, tracks);
        case(Expr::Or):
            return evalOr(exp, tracks);
        case(Expr::XOr):
            return evalXOr(exp, tracks);
        case(Expr::Missing):
            return evalMissing(exp, tracks);
        case(Expr::Present):
            return evalPresent(exp, tracks);
        case(Expr::Equals):
            return evalEquals(exp, tracks);
        case(Expr::Contains):
            return evalContains(exp, tracks);
        case(Expr::Greater):
            return compareValues(exp, tracks, std::greater<>());
        case(Expr::GreaterEqual):
            return compareValues(exp, tracks, std::greater_equal<>());
        case(Expr::Less):
            return compareValues(exp, tracks, std::less<>());
        case(Expr::LessEqual):
            return compareValues(exp, tracks, std::less_equal<>());
        case(Expr::Before):
            return compareDates(exp, tracks, std::less<>());
        case(Expr::After):
            return compareDates(exp, tracks, std::greater<>());
        case(Expr::Since):
            return compareDates(exp, tracks, std::greater_equal<>());
        case(Expr::During):
            return compareDateRange(exp, tracks);
        case(Expr::All):
        case(Expr::SortAscending):
        case(Expr::SortDescending):
            return ScriptResult{.value = {}, .cond = true};
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
    ScriptResult result = m_registry->value(var.toLower(), tracks);

    if(!result.cond) {
        return {};
    }

    if(result.value.contains(QLatin1String{Constants::UnitSeparator})) {
        result.value = result.value.replace(QLatin1String{Constants::UnitSeparator}, QStringLiteral(", "));
    }

    return result;
}

ScriptResult ScriptParserPrivate::evalVariableList(const Expression& exp, const auto& tracks) const
{
    const QString var = std::get<QString>(exp.value);
    return m_registry->value(var.toLower(), tracks);
}

ScriptResult ScriptParserPrivate::evalVariableRaw(const Expression& exp, const auto& tracks) const
{
    const QString var = std::get<QString>(exp.value);

    ScriptResult result;
    if constexpr(std::is_same_v<std::decay_t<decltype(tracks)>, Track>) {
        result.value = tracks.metaValue(var);
    }
    else if constexpr(std::is_same_v<std::decay_t<decltype(tracks)>, TrackList>) {
        result.value = tracks.front().metaValue(var);
    }
    result.cond = !result.value.isEmpty();

    if(!result.cond) {
        return {};
    }

    if(result.value.contains(QLatin1String{Constants::UnitSeparator})) {
        result.value = result.value.replace(QLatin1String{Constants::UnitSeparator}, QStringLiteral(", "));
    }

    return result;
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
        if(subExpr.value.contains(QLatin1String{Constants::UnitSeparator})) {
            QStringList newResult;
            const auto values = subExpr.value.split(QLatin1String{Constants::UnitSeparator});
            std::ranges::transform(values, std::back_inserter(newResult),
                                   [&](const auto& value) { return result.value + value; });
            result.value = newResult.join(QLatin1String{Constants::UnitSeparator});
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
        if(subArg.type != Expr::Literal && subArg.type != Expr::QuotedLiteral) {
            if(!subExpr.cond || subExpr.value.isEmpty()) {
                // No need to evaluate rest
                result.value.clear();
                result.cond = false;
                return result;
            }
        }
        if(subExpr.value.contains(QLatin1String{Constants::UnitSeparator})) {
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
        result.value = exprResult.join(QLatin1String{Constants::UnitSeparator});
    }
    return result;
}

ScriptResult ScriptParserPrivate::evalNot(const Expression& exp, const auto& tracks) const
{
    auto args = std::get<ExpressionList>(exp.value);

    ScriptResult result;
    result.cond = true;

    for(const Expression& arg : args) {
        const auto subExpr = evalExpression(arg, tracks);
        result.cond        = !subExpr.cond;
        return result;
    }

    return result;
}

ScriptResult ScriptParserPrivate::evalGroup(const Expression& exp, const auto& tracks) const
{
    auto args = std::get<ExpressionList>(exp.value);

    ScriptResult result;
    result.cond = true;

    for(const Expression& arg : args) {
        const auto subExpr = evalExpression(arg, tracks);
        if(!subExpr.cond) {
            result.cond = false;
            return result;
        }
    }

    return result;
}

ScriptResult ScriptParserPrivate::evalAnd(const Expression& exp, const auto& tracks) const
{
    const auto args = std::get<ExpressionList>(exp.value);
    if(args.size() < 2) {
        return {};
    }

    const ScriptResult first = evalExpression(args.at(0), tracks);
    if(!first.cond) {
        return {};
    }

    const ScriptResult second = evalExpression(args.at(1), tracks);

    ScriptResult result;
    result.cond = second.cond;
    return result;
}

ScriptResult ScriptParserPrivate::evalOr(const Expression& exp, const auto& tracks) const
{
    const auto args = std::get<ExpressionList>(exp.value);
    if(args.size() < 2) {
        return {};
    }

    const ScriptResult first  = evalExpression(args.at(0), tracks);
    const ScriptResult second = evalExpression(args.at(1), tracks);

    ScriptResult result;
    result.cond = first.cond | second.cond;
    return result;
}

ScriptResult ScriptParserPrivate::evalXOr(const Expression& exp, const auto& tracks) const
{
    const auto args = std::get<ExpressionList>(exp.value);
    if(args.size() < 2) {
        return {};
    }

    const ScriptResult first  = evalExpression(args.at(0), tracks);
    const ScriptResult second = evalExpression(args.at(1), tracks);

    ScriptResult result;
    result.cond = first.cond ^ second.cond;
    return result;
}

ScriptResult ScriptParserPrivate::evalMissing(const Expression& exp, const auto& tracks) const
{
    const auto args = std::get<ExpressionList>(exp.value);
    if(args.size() != 1) {
        return {};
    }

    const Expression meta{Expr::VariableRaw, args.front().value};
    ScriptResult result = evalExpression(meta, tracks);
    result.cond         = !result.cond;
    return result;
}

ScriptResult ScriptParserPrivate::evalPresent(const Expression& exp, const auto& tracks) const
{
    const auto args = std::get<ExpressionList>(exp.value);
    if(args.size() != 1) {
        return {};
    }

    const Expression meta{Expr::VariableRaw, args.front().value};
    ScriptResult result = evalExpression(meta, tracks);
    return result;
}

ScriptResult ScriptParserPrivate::evalEquals(const Expression& exp, const auto& tracks) const
{
    const auto args = std::get<ExpressionList>(exp.value);
    if(args.size() < 2) {
        return {};
    }

    const ScriptResult first = evalExpression(args.at(0), tracks);
    if(!first.cond) {
        return {};
    }

    const ScriptResult second = evalExpression(args.at(1), tracks);
    if(!second.cond) {
        return {};
    }

    ScriptResult result;
    if(first.value.compare(second.value, Qt::CaseInsensitive) == 0) {
        result.cond = true;
    }

    return result;
}

ScriptResult ScriptParserPrivate::evalContains(const Expression& exp, const auto& tracks) const
{
    const auto args = std::get<ExpressionList>(exp.value);
    if(args.size() < 2) {
        return {};
    }

    const ScriptResult first = evalExpression(args.at(0), tracks);
    if(!first.cond) {
        return {};
    }

    const ScriptResult second = evalExpression(args.at(1), tracks);
    if(!second.cond) {
        return {};
    }

    ScriptResult result;
    if(first.value.contains(second.value, Qt::CaseInsensitive)) {
        result.cond = true;
    }

    return result;
}

ParsedScript ScriptParserPrivate::parse(const QString& input)
{
    if(input.isEmpty() || !m_registry) {
        return {};
    }

    m_isQuery = false;
    m_scanner.setSkipWhitespace(false);

    if(m_parsedScripts.contains(input)) {
        return m_parsedScripts.at(input);
    }

    m_currentInput = input;
    auto& script   = m_parsedScripts[input];
    script.input   = input;

    m_scanner.setup(input);

    advance();
    while(m_current.type != TokenType::TokEos) {
        const Expression expr = expression();
        if(expr.type != Expr::Null) {
            script.expressions.emplace_back(expr);
        }
    }

    consume(TokenType::TokEos, QStringLiteral("Expected end of expression"));

    return script;
}

ParsedScript ScriptParserPrivate::parseQuery(const QString& input)
{
    if(input.isEmpty() || !m_registry) {
        return {};
    }

    m_isQuery = true;
    m_scanner.setSkipWhitespace(true);

    if(m_parsedScripts.contains(input)) {
        return m_parsedScripts.at(input);
    }

    m_currentInput = input;
    auto& script   = m_parsedScripts[input];
    script.input   = input;

    m_scanner.setup(input);

    advance();
    while(m_current.type != TokenType::TokEos) {
        Expression prevExpr = expression();
        Expression expr     = checkOperator(prevExpr);

        while(expr.type != prevExpr.type) {
            prevExpr = expr;
            expr     = checkOperator(prevExpr);
        }

        if(expr.type != Expr::Null) {
            script.expressions.emplace_back(expr);
        }
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

        if(evalExpr.value.contains(QLatin1String{Constants::UnitSeparator})) {
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
        return m_currentResult.join(QLatin1String{Constants::UnitSeparator});
    }

    return {};
}

TrackList ScriptParserPrivate::evaluateQuery(const ParsedScript& input, const TrackList& tracks)
{
    if(!input.isValid() || !m_registry) {
        return {};
    }

    m_currentResult.clear();

    TrackList filteredTracks;

    const ExpressionList expressions = input.expressions;

    if(expressions.size() == 1) {
        const auto& firstExpr = expressions.front();
        if(firstExpr.type == Expr::Literal || firstExpr.type == Expr::QuotedLiteral) {
            // Simple search query - just match all terms in metadata/filepath
            const QString search = std::get<QString>(expressions.front().value);
            return Utils::filter(tracks, [&search, &firstExpr](const Track& track) {
                return matchSearch(track, search, firstExpr.type == Expr::QuotedLiteral);
            });
        }
        if(!isQueryExpression(firstExpr.type)) {
            return {};
        }
    }

    for(const Track& track : tracks) {
        if(std::ranges::all_of(expressions,
                               [this, &track](const auto& expr) { return evalExpression(expr, track).cond; })) {
            filteredTracks.emplace_back(track);
        }
    }

    const Expression& lastExpr = expressions.back();
    if(lastExpr.type == Expr::SortAscending || lastExpr.type == Expr::SortDescending) {
        TrackSorter m_sorter;
        filteredTracks
            = m_sorter.calcSortTracks(std::get<QString>(lastExpr.value), filteredTracks,
                                      lastExpr.type == Expr::SortAscending ? Qt::AscendingOrder : Qt::DescendingOrder);
    }

    return filteredTracks;
}

ScriptResult ScriptParserPrivate::compareValues(const Expression& exp, const auto& tracks, const auto& comparator) const
{
    const auto args = std::get<ExpressionList>(exp.value);
    if(args.size() < 2) {
        return {};
    }

    const ScriptResult first = evalExpression(args.at(0), tracks);
    if(!first.cond) {
        return {};
    }

    const ScriptResult second = evalExpression(args.at(1), tracks);
    if(!second.cond) {
        return {};
    }

    bool ok{false};
    const double firstValue = first.value.toDouble(&ok);
    if(!ok) {
        return {};
    }

    const double secondValue = second.value.toDouble(&ok);
    if(!ok) {
        return {};
    }

    ScriptResult result;
    result.cond = comparator(firstValue, secondValue);
    return result;
}

ScriptResult ScriptParserPrivate::compareDates(const Expression& exp, const auto& tracks, const auto& comparator) const
{
    const auto args = std::get<ExpressionList>(exp.value);
    if(args.size() < 2) {
        return {};
    }

    uint64_t first;

    const QString var = std::get<QString>(args.at(0).value);
    if constexpr(std::is_same_v<std::decay_t<decltype(tracks)>, Track>) {
        first = tracks.metaValue(var).toULongLong();
    }
    else if constexpr(std::is_same_v<std::decay_t<decltype(tracks)>, TrackList>) {
        first = tracks.front().metaValue(var).toULongLong();
    }

    const auto second = std::get<QString>(args.at(1).value).toULongLong();

    ScriptResult result;
    result.cond = comparator(first, second);

    return result;
}

ScriptResult ScriptParserPrivate::compareDateRange(const Expression& exp, const auto& tracks) const
{
    const auto args = std::get<ExpressionList>(exp.value);
    if(args.size() < 3) {
        return {};
    }

    uint64_t first;

    const QString var = std::get<QString>(args.at(0).value);
    if constexpr(std::is_same_v<std::decay_t<decltype(tracks)>, Track>) {
        first = tracks.metaValue(var).toULongLong();
    }
    else if constexpr(std::is_same_v<std::decay_t<decltype(tracks)>, TrackList>) {
        first = tracks.front().metaValue(var).toULongLong();
    }

    const auto min = std::get<QString>(args.at(1).value).toULongLong();
    const auto max = std::get<QString>(args.at(2).value).toULongLong();

    ScriptResult result;
    result.cond = first > min && first < max;

    return result;
}

Expression ScriptParserPrivate::checkOperator(const Expression& expr)
{
    if(!m_isQuery) {
        return expr;
    }

    switch(m_current.type) {
        case(TokenType::TokColon):
            return contains(expr);
        case(TokenType::TokEquals):
            return equals(expr);
        case(TokenType::TokNot):
            return notKeyword(expr);
        case(TokenType::TokAnd):
            return andKeyword(expr);
        case(TokenType::TokOr):
            return orKeyword(expr);
        case(TokenType::TokXOr):
            return xorKeyword(expr);
        case(TokenType::TokMissing):
            return missingKeyword(expr);
        case(TokenType::TokPresent):
            return presentKeyword(expr);
        case(TokenType::TokLeftAngle):
            return less(expr);
        case(TokenType::TokRightAngle):
            return greater(expr);
        case(TokenType::TokBefore):
            return beforeKeyword(expr);
        case(TokenType::TokAfter):
            return afterKeyword(expr);
        case(TokenType::TokSince):
            return sinceKeyword(expr);
        case(TokenType::TokDuring):
            return duringKeyword(expr);
        default:
            break;
    }

    return expr;
}

ScriptParser::ScriptParser()
    : p{std::make_unique<ScriptParserPrivate>(this, nullptr)}
{ }

ScriptParser::ScriptParser(ScriptRegistry* registry)
    : p{std::make_unique<ScriptParserPrivate>(this, registry)}
{ }

ScriptRegistry* ScriptParser::registry() const
{
    return p->m_registry.get();
}

ScriptParser::~ScriptParser() = default;

ParsedScript ScriptParser::parse(const QString& input)
{
    if(input.isEmpty()) {
        return {};
    }

    return p->parse(input);
}

ParsedScript ScriptParser::parseQuery(const QString& input)
{
    if(input.isEmpty()) {
        return {};
    }

    return p->parseQuery(input);
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

    const auto script = parse(input);
    return p->evaluate(script, track);
}

QString ScriptParser::evaluate(const ParsedScript& input, const Track& track)
{
    if(!input.isValid()) {
        return {};
    }

    p->m_isQuery = false;

    return p->evaluate(input, track);
}

QString ScriptParser::evaluate(const QString& input, const TrackList& tracks)
{
    if(input.isEmpty()) {
        return {};
    }

    const auto script = parse(input);
    return p->evaluate(script, tracks);
}

QString ScriptParser::evaluate(const ParsedScript& input, const TrackList& tracks)
{
    if(!input.isValid()) {
        return {};
    }

    p->m_isQuery = false;

    return p->evaluate(input, tracks);
}

TrackList ScriptParser::filter(const QString& input, const TrackList& tracks)
{
    if(input.isEmpty()) {
        return {};
    }

    const auto script = parseQuery(input);
    return p->evaluateQuery(script, tracks);
}

TrackList ScriptParser::filter(const ParsedScript& input, const TrackList& tracks)
{
    if(!input.isValid()) {
        return {};
    }

    p->m_isQuery = true;

    return p->evaluateQuery(input, tracks);
}

void ScriptParser::clearCache()
{
    p->m_parsedScripts.clear();
}
} // namespace Fooyin
