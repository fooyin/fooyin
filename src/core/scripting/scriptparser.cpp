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

#include "scriptbinder.h"
#include "scriptcache.h"
#include "scriptregistry.h"

#include <core/constants.h>
#include <core/library/tracksort.h>
#include <core/scripting/scriptscanner.h>
#include <core/track.h>
#include <utils/helpers.h>
#include <utils/stringutils.h>
#include <utils/utils.h>

#include <QDateTime>
#include <QDebug>

#include <atomic>

using namespace Qt::StringLiterals;

using TokenType = Fooyin::ScriptScanner::TokenType;

namespace {
std::atomic_uint64_t& nextParsedScriptId()
{
    static std::atomic_uint64_t nextParsedScriptId{1};
    return nextParsedScriptId;
}

QDateTime evalDate(const Fooyin::Expression& expr)
{
    if(!std::holds_alternative<QString>(expr.value)) {
        return {};
    }

    const QString value = std::get<QString>(expr.value);
    return Fooyin::Utils::dateStringToDate(value);
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

    const auto formats = Fooyin::Utils::dateFormats();
    for(const auto& format : formats) {
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
        case(Type::Limit):
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
    explicit ScriptParserPrivate(ScriptParser* self);

    void advance();
    void consume(TokenType type, const QString& message);
    [[nodiscard]] bool currentToken(TokenType type) const;
    bool match(TokenType type);

    void errorAtCurrent(const QString& message);
    void errorAt(const ScriptScanner::Token& token, const QString& message);
    void error(const QString& message);

    Expression expression();
    Expression literal() const;
    Expression quote();
    Expression variable();
    Expression function();
    Expression functionArgs();
    Expression conditional();
    Expression group();
    Expression notKeyword(const Expression& key);
    Expression logicalOperator(const Expression& key, Expr::Type type);
    Expression relationalOperator(const Expression& key, Expr::Type type, Expr::Type equalsType = Expr::Null);
    Expression metadataKeyword(const Expression& key, Expr::Type type);
    Expression timeKeyword(const Expression& key, Expr::Type type);
    Expression duringKeyword(const Expression& key);
    Expression sort();
    Expression limit();

    const BoundScript& bind(const ParsedScript& input);

    ScriptResult evalExpression(const BoundExpression& exp, const auto& tracks);
    ScriptResult evalLiteral(const BoundExpression& exp);
    ScriptResult evalVariable(const BoundExpression& exp, const auto& tracks);
    ScriptResult evalVariableList(const BoundExpression& exp, const auto& tracks);
    ScriptResult evalVariableRaw(const BoundExpression& exp, const auto& tracks);
    ScriptResult evalFunction(const BoundExpression& exp, const auto& tracks);
    ScriptResult evalFunctionArg(const BoundExpression& exp, const auto& tracks);
    ScriptResult evalConditional(const BoundExpression& exp, const auto& tracks);
    ScriptResult evalNot(const BoundExpression& exp, const auto& tracks);
    ScriptResult evalGroup(const BoundExpression& exp, const auto& tracks);
    ScriptResult evalAnd(const BoundExpression& exp, const auto& tracks);
    ScriptResult evalOr(const BoundExpression& exp, const auto& tracks);
    ScriptResult evalXOr(const BoundExpression& exp, const auto& tracks);
    ScriptResult evalMissing(const BoundExpression& exp, const auto& tracks);
    ScriptResult evalPresent(const BoundExpression& exp, const auto& tracks);
    ScriptResult evalEquals(const BoundExpression& exp, const auto& tracks);
    ScriptResult evalContains(const BoundExpression& exp, const auto& tracks);
    ScriptResult evalContains(const BoundExpression& exp, const Track& track);
    ScriptResult evalLimit(const BoundExpression& exp);
    ScriptResult evalSort(const BoundExpression& exp);

    ParsedScript parse(const QString& input);
    ParsedScript parseQuery(const QString& input);
    QString evaluate(const ParsedScript& input, const auto& tracks);

    template <typename TrackListType>
    TrackListType evaluateQuery(const ParsedScript& input, const TrackListType& tracks);

    ScriptResult compareValues(const BoundExpression& exp, const auto& tracks, const auto& comparator);
    ScriptResult compareDates(const BoundExpression& exp, const auto& tracks, const auto& comparator);
    ScriptResult compareDateRange(const BoundExpression& exp, const auto& tracks);
    Expression checkOperator(const Expression& expr);
    Expression normaliseQueryField(Expression field) const;

    void reset();

    ScriptParser* m_self;

    ScriptScanner m_scanner;
    std::unique_ptr<ScriptRegistry> m_registry;

    ScriptScanner::Token m_current;
    ScriptScanner::Token m_previous;

    bool m_isQuery{false};
    QString m_currentInput;
    ParsedScript m_currentScript;
    ScriptCache m_scriptCache;
    ScriptCache m_queryCache;
    BoundScriptCache m_boundScriptCache;
    BoundScriptCache m_boundQueryCache;
    BoundScript m_currentBoundScript;
    QStringList m_currentResult;

    QString m_sortScript;
    Qt::SortOrder m_sortOrder{Qt::AscendingOrder};
    int m_limit{0};
    int m_filteredCount{0};
    std::unordered_map<QString, QString> m_variables;
};

ScriptParserPrivate::ScriptParserPrivate(ScriptParser* self)
    : m_self{self}
    , m_registry{std::make_unique<ScriptRegistry>()}
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
    QString errorMsg = u"[%1] Error"_s.arg(token.position);

    if(token.type == TokenType::TokEos) {
        errorMsg += u" at end of string"_s;
    }
    else {
        errorMsg += u": '"_s + token.value.toString() + u"'"_s;
    }

    errorMsg += u" (%1)"_s.arg(message);

    ScriptError currentError;
    currentError.value    = token.value.toString();
    currentError.position = token.position;
    currentError.message  = errorMsg;

    m_currentScript.errors.emplace_back(currentError);
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
        case(TokenType::TokSort):
            return m_isQuery ? sort() : literal();
        case(TokenType::TokLimit):
            return limit();
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
        case(TokenType::TokPlus):
        case(TokenType::TokMinus):
            return literal();
        case(TokenType::TokEos):
        case(TokenType::TokError):
            break;
    }

    return {Expr::Null};
}

Expression ScriptParserPrivate::literal() const
{
    QString value = m_previous.value.toString();

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
    consume(TokenType::TokQuote, QObject::tr("Expected %1 to close quote").arg("'\'"_L1));
    return expr;
}

Expression ScriptParserPrivate::variable()
{
    advance();

    Expression expr;
    QString value;

    const auto consumeVar = [this, &value](TokenType type) {
        value.append(m_previous.value);
        while(!currentToken(type) && !currentToken(TokenType::TokEos)) {
            advance();
            value.append(m_previous.value);
        }
    };

    if(m_previous.type == TokenType::TokLeftAngle) {
        advance();
        expr.type = Expr::VariableList;
        consumeVar(TokenType::TokRightAngle);
        value = value.toUpper();
        consume(TokenType::TokRightAngle, QObject::tr("Expected %1 to close variable list").arg("'>'"_L1));
    }
    else {
        expr.type = Expr::Variable;
        consumeVar(TokenType::TokVar);
        value = m_isQuery ? value.trimmed().toUpper() : value.toUpper();
    }

    expr.value = value;
    consume(TokenType::TokVar, QObject::tr("Expected %1 to close variable").arg("'%'"_L1));
    return expr;
}

Expression ScriptParserPrivate::function()
{
    advance();

    if(m_previous.type != TokenType::TokLiteral) {
        error(u"Expected function name"_s);
    }

    Expression expr{Expr::Function};
    FuncValue funcExpr;
    funcExpr.name = m_previous.value.toString().toLower();

    if(resolveBuiltInFunctionKind(funcExpr.name) == FunctionKind::Generic && !m_registry->isFunction(funcExpr.name)) {
        error(u"Function not found"_s);
    }

    consume(TokenType::TokLeftParen, QObject::tr("Expected %1 after function name").arg("'('"_L1));

    if(!currentToken(TokenType::TokRightParen)) {
        funcExpr.args.emplace_back(functionArgs());
        while(match(TokenType::TokComma)) {
            funcExpr.args.emplace_back(functionArgs());
        }
    }

    expr.value = funcExpr;
    consume(TokenType::TokRightParen, QObject::tr("Expected %1 at end of function").arg("')'"_L1));
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
    consume(TokenType::TokRightSquare, QObject::tr("Expected %1 to close conditional").arg("']'"_L1));
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
            expr.value = u"(%1)"_s.arg(std::get<QString>(firstArg.value));
            consume(TokenType::TokRightParen, QObject::tr("Expected %1 to close group").arg("')'"_L1));
            return expr;
        }
    }

    expr.value = args;
    consume(TokenType::TokRightParen, QObject::tr("Expected %1 to close group").arg("')'"_L1));
    return expr;
}

Expression ScriptParserPrivate::notKeyword(const Expression& key)
{
    Expression expr{Expr::Not};
    ExpressionList args;

    if(currentToken(TokenType::TokNot)) {
        advance();
        if(currentToken(TokenType::TokEquals)) {
            advance();

            Expression equals{Expr::Equals};
            ExpressionList equalsArgs;
            equalsArgs.emplace_back(normaliseQueryField(key));

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

Expression ScriptParserPrivate::logicalOperator(const Expression& key, Expr::Type type)
{
    Expression expr{type};
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

Expression ScriptParserPrivate::relationalOperator(const Expression& key, Expr::Type type, Expr::Type equalsType)
{
    Expression expr{type};
    ExpressionList args;

    advance();

    args.emplace_back(normaliseQueryField(key));

    if(equalsType != Expr::Null && match(TokenType::TokEquals)) {
        expr.type = equalsType;
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

Expression ScriptParserPrivate::metadataKeyword(const Expression& key, Expr::Type type)
{
    Expression expr{type};
    ExpressionList args;

    advance();

    args.emplace_back(normaliseQueryField(key));

    expr.value = args;
    return expr;
}

Expression ScriptParserPrivate::timeKeyword(const Expression& key, Expr::Type type)
{
    Expression expr{type};
    ExpressionList args;

    advance();

    args.emplace_back(normaliseQueryField(key));

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

    args.emplace_back(normaliseQueryField(key));

    if(currentToken(TokenType::TokLast)) {
        advance();

        bool valid{false};
        bool validUserCount{true};
        int count{1};
        QDateTime date = QDateTime::currentDateTime();

        if(currentToken(TokenType::TokLiteral)) {
            const auto countExpr = expression();
            if(countExpr.type == Expr::Null || !std::holds_alternative<QString>(countExpr.value)) {
                return {};
            }

            const int userCount = std::get<QString>(countExpr.value).toInt();
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
                args.emplace_back(Expr::Date, QString::number(QDateTime::currentMSecsSinceEpoch()));
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

Expression ScriptParserPrivate::normaliseQueryField(Expression field) const
{
    if(field.type != Expr::Literal) {
        return field;
    }

    auto& value = std::get<QString>(field.value);
    value       = value.trimmed().toUpper();
    field.type  = Expr::Variable;

    return field;
}

Expression ScriptParserPrivate::sort()
{
    Expression expr;
    QString val;

    if(currentToken(TokenType::TokBy) || currentToken(TokenType::TokPlus)) {
        advance();
        expr.type = Expr::SortAscending;
    }
    else if(currentToken(TokenType::TokMinus)) {
        advance();
        expr.type = Expr::SortDescending;
    }
    else if(currentToken(TokenType::TokAscending)) {
        advance();
        expr.type = Expr::SortAscending;
        consume(TokenType::TokBy, QObject::tr("Expected %1 after %2").arg("'BY'"_L1, "'ASCENDING'"_L1));
    }
    else if(currentToken(TokenType::TokDescending)) {
        advance();
        expr.type = Expr::SortDescending;
        consume(TokenType::TokBy, QObject::tr("Expected %1 after %2").arg("'BY'"_L1, "'DESCENDING'"_L1));
    }

    while(!currentToken(TokenType::TokLimit) && !currentToken(TokenType::TokEos)) {
        advance();
        val.append(m_previous.value);
    }

    expr.value = val.trimmed();
    return expr;
}

Expression ScriptParserPrivate::limit()
{
    Expression expr{Expr::Limit};

    if(!currentToken(TokenType::TokEos)) {
        advance();
        expr.value = m_previous.value.toString();
    }

    return expr;
}

ScriptResult ScriptParserPrivate::evalExpression(const BoundExpression& exp, const auto& tracks)
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
        case(Expr::Limit):
            return evalLimit(exp);
        case(Expr::SortAscending):
        case(Expr::SortDescending):
            return evalSort(exp);
        case(Expr::All):
            return ScriptResult{.value = {}, .cond = true};
        case(Expr::Null):
        default:
            return {};
    }
}

ScriptResult ScriptParserPrivate::evalLiteral(const BoundExpression& exp)
{
    ScriptResult result;
    result.value = std::get<QString>(exp.value);
    result.cond  = true;
    return result;
}

ScriptResult ScriptParserPrivate::evalVariable(const BoundExpression& exp, const auto& tracks)
{
    const auto& var     = std::get<QString>(exp.value);
    ScriptResult result = m_registry->value(exp.variableKind, var, makeScriptSubject(tracks));

    if(!result.cond) {
        return {};
    }

    if(result.value.contains(QLatin1String{Constants::UnitSeparator})) {
        result.value = result.value.replace(QLatin1String{Constants::UnitSeparator}, u", "_s);
    }

    return result;
}

ScriptResult ScriptParserPrivate::evalVariableList(const BoundExpression& exp, const auto& tracks)
{
    const auto& var = std::get<QString>(exp.value);
    return m_registry->value(exp.variableKind, var, makeScriptSubject(tracks));
}

ScriptResult ScriptParserPrivate::evalVariableRaw(const BoundExpression& exp, const auto& tracks)
{
    const auto& var = std::get<QString>(exp.value);

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
        result.value = result.value.replace(QLatin1String{Constants::UnitSeparator}, u", "_s);
    }

    return result;
}

ScriptResult ScriptParserPrivate::evalFunction(const BoundExpression& exp, const auto& tracks)
{
    const auto& func = std::get<BoundFunctionValue>(exp.value);
    auto toResult    = [](QString value) {
        const bool cond = !value.isEmpty();
        return ScriptResult{.value = std::move(value), .cond = cond};
    };

    switch(func.kind) {
        case(FunctionKind::Get): {
            if(func.args.size() != 1) {
                return {};
            }

            const QString variableName = evalExpression(func.args.at(0), tracks).value.trimmed().toLower();
            if(variableName.isEmpty()) {
                return {};
            }

            if(const auto it = m_variables.find(variableName); it != m_variables.cend()) {
                return {.value = it->second, .cond = !it->second.isEmpty()};
            }

            return {};
        }
        case(FunctionKind::Put):
        case(FunctionKind::Puts): {
            if(func.args.size() != 2) {
                return {};
            }

            const QString variableName = evalExpression(func.args.at(0), tracks).value.trimmed().toLower();
            if(variableName.isEmpty()) {
                return {};
            }

            const ScriptResult value = evalExpression(func.args.at(1), tracks);
            m_variables.insert_or_assign(variableName, value.value);

            if(func.kind == FunctionKind::Put) {
                return {.value = value.value, .cond = !value.value.isEmpty()};
            }

            return {.value = QString{}, .cond = false};
        }
        case(FunctionKind::If): {
            const auto size = func.args.size();
            if(size < 2 || size > 3) {
                return {};
            }

            const ScriptResult condition = evalExpression(func.args.at(0), tracks);
            if(condition.cond) {
                return evalExpression(func.args.at(1), tracks);
            }
            if(size == 3) {
                return evalExpression(func.args.at(2), tracks);
            }
            return {};
        }
        case(FunctionKind::If2): {
            const auto size = func.args.size();
            if(size < 1 || size > 2) {
                return {};
            }

            const ScriptResult first = evalExpression(func.args.at(0), tracks);
            if(first.cond) {
                return first;
            }
            if(size == 2) {
                return evalExpression(func.args.at(1), tracks);
            }
            return {};
        }
        case(FunctionKind::IfEqual): {
            if(func.args.size() != 4) {
                return {};
            }

            const ScriptResult first  = evalExpression(func.args.at(0), tracks);
            const ScriptResult second = evalExpression(func.args.at(1), tracks);
            if(first.value.toDouble() == second.value.toDouble()) {
                return evalExpression(func.args.at(2), tracks);
            }
            return evalExpression(func.args.at(3), tracks);
        }
        case(FunctionKind::IfGreater): {
            const auto size = func.args.size();
            if(size < 3 || size > 4) {
                return {};
            }

            const ScriptResult first  = evalExpression(func.args.at(0), tracks);
            const ScriptResult second = evalExpression(func.args.at(1), tracks);
            if(first.value.toDouble() > second.value.toDouble()) {
                return evalExpression(func.args.at(2), tracks);
            }
            if(size == 4) {
                return evalExpression(func.args.at(3), tracks);
            }
            return {};
        }
        case(FunctionKind::IfLonger): {
            if(func.args.size() != 4) {
                return {};
            }

            const ScriptResult first  = evalExpression(func.args.at(0), tracks);
            const ScriptResult second = evalExpression(func.args.at(1), tracks);

            bool ok{false};
            const auto length = second.value.toLongLong(&ok);
            if(!ok) {
                return {};
            }

            if(first.value.size() >= length) {
                return evalExpression(func.args.at(2), tracks);
            }
            return evalExpression(func.args.at(3), tracks);
        }
        case(FunctionKind::Add):
        case(FunctionKind::Sub):
        case(FunctionKind::Mul):
        case(FunctionKind::Div): {
            const auto size = static_cast<qsizetype>(func.args.size());
            if(size < 2) {
                return {};
            }

            bool ok{false};
            double total = evalExpression(func.args.front(), tracks).value.toDouble(&ok);
            if(!ok) {
                return {};
            }

            for(qsizetype i{1}; i < size; ++i) {
                const double num = evalExpression(func.args.at(i), tracks).value.toDouble(&ok);
                if(!ok) {
                    continue;
                }

                if(func.kind == FunctionKind::Add) {
                    total += num;
                }
                else if(func.kind == FunctionKind::Sub) {
                    total -= num;
                }
                else if(func.kind == FunctionKind::Mul) {
                    total *= num;
                }
                else {
                    total /= num;
                }
            }

            return toResult(QString::number(total));
        }
        case(FunctionKind::Mod): {
            const qsizetype size = static_cast<qsizetype>(func.args.size());
            if(size < 2) {
                return {};
            }

            int total = evalExpression(func.args.front(), tracks).value.toInt();
            for(qsizetype i{1}; i < size; ++i) {
                total %= evalExpression(func.args.at(i), tracks).value.toInt();
            }
            return toResult(QString::number(total));
        }
        case(FunctionKind::Num): {
            const auto size = func.args.size();
            if(size < 1 || size > 2) {
                return {};
            }

            const ScriptResult first = evalExpression(func.args.at(0), tracks);
            if(size == 1) {
                return toResult(first.value);
            }

            bool isInt{false};
            const int number = first.value.toInt(&isInt);
            if(!isInt) {
                return toResult(first.value);
            }

            const int prefix = evalExpression(func.args.at(1), tracks).value.toInt(&isInt);
            if(!isInt) {
                return toResult(first.value);
            }

            return toResult(Utils::addLeadingZero(number, prefix));
        }
        case(FunctionKind::Pad):
        case(FunctionKind::PadRight): {
            const auto size = func.args.size();
            if(size < 2 || size > 3) {
                return {};
            }

            const QString str = evalExpression(func.args.at(0), tracks).value;
            bool ok{false};
            const int len = evalExpression(func.args.at(1), tracks).value.toInt(&ok);
            if(!ok || len < 0) {
                return {};
            }

            QChar padChar = u' ';
            if(size == 3) {
                const QString customPad = evalExpression(func.args.at(2), tracks).value;
                if(!customPad.isEmpty()) {
                    padChar = customPad.front();
                }
            }

            return toResult(func.kind == FunctionKind::Pad ? str.leftJustified(len, padChar)
                                                           : str.rightJustified(len, padChar));
        }
        case(FunctionKind::Generic):
            break;
    }

    ScriptValueList args;
    args.reserve(func.args.size());
    std::ranges::transform(func.args, std::back_inserter(args),
                           [this, &tracks](const BoundExpression& arg) { return evalExpression(arg, tracks); });
    return m_registry->function(func.functionId, func.name, args, makeScriptSubject(tracks));
}

ScriptResult ScriptParserPrivate::evalFunctionArg(const BoundExpression& exp, const auto& tracks)
{
    const auto& arg = std::get<BoundExpressionList>(exp.value);
    if(arg.empty()) {
        return {.value = QString{}, .cond = false};
    }

    if(arg.size() == 1) {
        return evalExpression(arg.front(), tracks);
    }

    ScriptResult result;
    bool allPassed{true};

    for(const BoundExpression& subArg : arg) {
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

ScriptResult ScriptParserPrivate::evalConditional(const BoundExpression& exp, const auto& tracks)
{
    const auto& arg = std::get<BoundExpressionList>(exp.value);
    if(arg.empty()) {
        return {};
    }

    if(arg.size() == 1) {
        const BoundExpression& subArg = arg.front();
        const ScriptResult subExpr    = evalExpression(subArg, tracks);

        if(subArg.type != Expr::Literal && subArg.type != Expr::QuotedLiteral) {
            if(!subExpr.cond || subExpr.value.isEmpty()) {
                return {};
            }
        }

        ScriptResult result;
        result.value = subExpr.value;
        result.cond  = true;
        return result;
    }

    ScriptResult result;
    QStringList exprResult;
    result.cond = true;

    for(const BoundExpression& subArg : arg) {
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

ScriptResult ScriptParserPrivate::evalNot(const BoundExpression& exp, const auto& tracks)
{
    const auto& args = std::get<BoundExpressionList>(exp.value);

    ScriptResult result;
    result.cond = true;

    for(const BoundExpression& arg : args) {
        const auto subExpr = evalExpression(arg, tracks);
        result.cond        = !subExpr.cond;
        return result;
    }

    return result;
}

ScriptResult ScriptParserPrivate::evalGroup(const BoundExpression& exp, const auto& tracks)
{
    const auto& args = std::get<BoundExpressionList>(exp.value);

    ScriptResult result;
    result.cond = true;

    for(const BoundExpression& arg : args) {
        const auto subExpr = evalExpression(arg, tracks);
        if(!subExpr.cond) {
            result.cond = false;
            return result;
        }
    }

    return result;
}

ScriptResult ScriptParserPrivate::evalAnd(const BoundExpression& exp, const auto& tracks)
{
    const auto& args = std::get<BoundExpressionList>(exp.value);
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

ScriptResult ScriptParserPrivate::evalOr(const BoundExpression& exp, const auto& tracks)
{
    const auto& args = std::get<BoundExpressionList>(exp.value);
    if(args.size() < 2) {
        return {};
    }

    const ScriptResult first  = evalExpression(args.at(0), tracks);
    const ScriptResult second = evalExpression(args.at(1), tracks);

    ScriptResult result;
    result.cond = first.cond | second.cond;
    return result;
}

ScriptResult ScriptParserPrivate::evalXOr(const BoundExpression& exp, const auto& tracks)
{
    const auto& args = std::get<BoundExpressionList>(exp.value);
    if(args.size() < 2) {
        return {};
    }

    const ScriptResult first  = evalExpression(args.at(0), tracks);
    const ScriptResult second = evalExpression(args.at(1), tracks);

    ScriptResult result;
    result.cond = first.cond ^ second.cond;
    return result;
}

ScriptResult ScriptParserPrivate::evalMissing(const BoundExpression& exp, const auto& tracks)
{
    const auto& args = std::get<BoundExpressionList>(exp.value);
    if(args.size() != 1) {
        return {};
    }

    BoundExpression meta;
    meta.type         = Expr::VariableRaw;
    meta.variableKind = VariableKind::Generic;
    meta.value        = std::get<QString>(args.front().value);

    ScriptResult result = evalExpression(meta, tracks);
    result.cond         = !result.cond;
    return result;
}

ScriptResult ScriptParserPrivate::evalPresent(const BoundExpression& exp, const auto& tracks)
{
    const auto& args = std::get<BoundExpressionList>(exp.value);
    if(args.size() != 1) {
        return {};
    }

    BoundExpression meta;
    meta.type         = Expr::VariableRaw;
    meta.variableKind = VariableKind::Generic;
    meta.value        = std::get<QString>(args.front().value);

    ScriptResult result = evalExpression(meta, tracks);
    return result;
}

ScriptResult ScriptParserPrivate::evalEquals(const BoundExpression& exp, const auto& tracks)
{
    const auto& args = std::get<BoundExpressionList>(exp.value);
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

ScriptResult ScriptParserPrivate::evalContains(const BoundExpression& exp, const auto& tracks)
{
    const auto& args = std::get<BoundExpressionList>(exp.value);
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
    result.cond = first.value.contains(second.value, Qt::CaseInsensitive);

    return result;
}

ScriptResult ScriptParserPrivate::evalContains(const BoundExpression& exp, const Track& track)
{
    const auto& args = std::get<BoundExpressionList>(exp.value);
    if(args.size() < 2) {
        return {};
    }

    const BoundExpression& field = args.at(0);
    const BoundExpression& value = args.at(1);

    const ScriptResult first = evalExpression(field, track);
    if(!first.cond) {
        return {};
    }

    const ScriptResult second = evalExpression(value, track);
    if(!second.cond) {
        return {};
    }

    ScriptResult result;

    if(field.type == Expr::All) {
        result.cond = matchSearch(track, second.value, value.type == Expr::QuotedLiteral);
    }
    else {
        result.cond = first.value.contains(second.value, Qt::CaseInsensitive);
    }

    return result;
}

ScriptResult ScriptParserPrivate::evalLimit(const BoundExpression& exp)
{
    ScriptResult result;
    result.cond = true;

    if(m_limit > 0) {
        return result;
    }

    const QString limit = std::get<QString>(exp.value);
    m_limit             = limit.toInt();

    return result;
}

ScriptResult ScriptParserPrivate::evalSort(const BoundExpression& exp)
{
    ScriptResult result;
    result.cond = true;

    if(!m_sortScript.isEmpty()) {
        return result;
    }

    m_sortScript = std::get<QString>(exp.value);
    m_sortOrder  = exp.type == Expr::SortAscending ? Qt::AscendingOrder : Qt::DescendingOrder;

    return result;
}

ParsedScript ScriptParserPrivate::parse(const QString& input)
{
    if(input.isEmpty()) {
        return {};
    }

    m_isQuery = false;
    m_scanner.setWhitespaceMode(ScriptScanner::WhitespaceMode::IgnoreLayout);
    m_currentScript = {};

    if(m_scriptCache.contains(input)) {
        return m_scriptCache.get(input);
    }

    m_currentInput          = input;
    m_currentScript.input   = input;
    m_currentScript.cacheId = nextParsedScriptId().fetch_add(1, std::memory_order_relaxed);
    m_scanner.setup(input);

    m_scanner.setup(input);

    advance();
    while(m_current.type != TokenType::TokEos) {
        const Expression expr = expression();
        if(expr.type != Expr::Null) {
            m_currentScript.expressions.emplace_back(expr);
        }
    }

    consume(TokenType::TokEos, QObject::tr("Expected end of script"));
    m_scriptCache.insert(input, m_currentScript);

    return m_currentScript;
}

ParsedScript ScriptParserPrivate::parseQuery(const QString& input)
{
    if(input.isEmpty()) {
        return {};
    }

    m_isQuery = true;
    m_scanner.setWhitespaceMode(ScriptScanner::WhitespaceMode::IgnoreAll);
    m_currentScript = {};

    if(m_queryCache.contains(input)) {
        return m_queryCache.get(input);
    }

    m_currentInput          = input;
    m_currentScript.input   = input;
    m_currentScript.cacheId = nextParsedScriptId().fetch_add(1, std::memory_order_relaxed);
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
            m_currentScript.expressions.emplace_back(expr);
        }
    }

    consume(TokenType::TokEos, QObject::tr("Expected end of script"));
    m_queryCache.insert(input, m_currentScript);

    return m_currentScript;
}

const BoundScript& ScriptParserPrivate::bind(const ParsedScript& input)
{
    BoundScriptCache& cache = m_isQuery ? m_boundQueryCache : m_boundScriptCache;

    if(input.cacheId != 0) {
        if(const BoundScript* cached = cache.find(input.cacheId); cached != nullptr) {
            return *cached;
        }
    }

    BoundScript bound = bindScript(input, m_registry.get());

    if(input.cacheId == 0) {
        m_currentBoundScript = std::move(bound);
        return m_currentBoundScript;
    }

    cache.insert(input.cacheId, std::move(bound));
    return *cache.find(input.cacheId);
}

QString ScriptParserPrivate::evaluate(const ParsedScript& input, const auto& tracks)
{
    if(!input.isValid()) {
        return {};
    }

    const BoundScript& bound = bind(input);
    if(!bound.isValid()) {
        return {};
    }

    reset();

    const auto& expressions = bound.expressions;
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

template <typename TrackListType>
TrackListType ScriptParserPrivate::evaluateQuery(const ParsedScript& input, const TrackListType& tracks)
{
    if(!input.isValid()) {
        return {};
    }

    const BoundScript& bound = bind(input);
    if(!bound.isValid()) {
        return {};
    }

    reset();
    TrackListType filteredTracks;

    if(bound.expressions.size() == 1) {
        const auto& firstExpr = bound.expressions.front();
        if(firstExpr.type == Expr::Literal || firstExpr.type == Expr::QuotedLiteral) {
            // Simple search query - just match all terms in metadata/filepath
            const auto& search     = std::get<QString>(firstExpr.value);
            TrackListType filtered = Utils::filter(tracks, [&search, &firstExpr](const auto& track) {
                if constexpr(std::is_same_v<TrackListType, PlaylistTrackList>) {
                    return matchSearch(track.track, search, firstExpr.type == Expr::QuotedLiteral);
                }
                else {
                    return matchSearch(track, search, firstExpr.type == Expr::QuotedLiteral);
                }
            });
            return filtered;
        }
        if(!isQueryExpression(firstExpr.type)) {
            return {};
        }
    }

    int count{0};
    for(const auto& track : tracks) {
        if(m_limit > 0 && count >= m_limit) {
            break;
        }

        const bool matches = std::ranges::all_of(bound.expressions, [&](const auto& expr) {
            if constexpr(std::is_same_v<TrackListType, PlaylistTrackList>) {
                return evalExpression(expr, track.track).cond;
            }
            else {
                return evalExpression(expr, track).cond;
            }
        });

        if(matches) {
            filteredTracks.emplace_back(track);
            ++count;
        }
    }

    if(!m_sortScript.isEmpty()) {
        TrackSorter m_sorter;
        ParsedScript sort = parse(m_sortScript);
        if(sort.expressions.size() == 1) {
            auto& sortExpr = sort.expressions.front();
            if(sortExpr.type == Expr::Literal) {
                sortExpr = normaliseQueryField(sortExpr);
            }
        }
        if constexpr(std::is_same_v<TrackListType, PlaylistTrackList>) {
            filteredTracks = m_sorter.calcSortTracks(sort, filteredTracks, PlaylistTrack::extractor, m_sortOrder);
        }
        else {
            filteredTracks = m_sorter.calcSortTracks(sort, filteredTracks, m_sortOrder);
        }
    }

    return filteredTracks;
}

ScriptResult ScriptParserPrivate::compareValues(const BoundExpression& exp, const auto& tracks, const auto& comparator)
{
    const auto& args = std::get<BoundExpressionList>(exp.value);
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

ScriptResult ScriptParserPrivate::compareDates(const BoundExpression& exp, const auto& tracks, const auto& comparator)
{
    const auto& args = std::get<BoundExpressionList>(exp.value);
    if(args.size() < 2) {
        return {};
    }

    std::optional<int64_t> first;

    const auto& var = std::get<QString>(args.at(0).value);
    if constexpr(std::is_same_v<std::decay_t<decltype(tracks)>, Track>) {
        first = tracks.dateValue(var);
    }
    else if constexpr(std::is_same_v<std::decay_t<decltype(tracks)>, TrackList>) {
        first = tracks.front().dateValue(var);
    }

    if(!first) {
        return {};
    }

    const auto second = std::get<QString>(args.at(1).value).toLongLong();

    ScriptResult result;
    result.cond = comparator(first.value(), second);

    return result;
}

ScriptResult ScriptParserPrivate::compareDateRange(const BoundExpression& exp, const auto& tracks)
{
    const auto& args = std::get<BoundExpressionList>(exp.value);
    if(args.size() < 3) {
        return {};
    }

    std::optional<int64_t> first;

    const auto& var = std::get<QString>(args.at(0).value);
    if constexpr(std::is_same_v<std::decay_t<decltype(tracks)>, Track>) {
        first = tracks.dateValue(var);
    }
    else if constexpr(std::is_same_v<std::decay_t<decltype(tracks)>, TrackList>) {
        first = tracks.front().dateValue(var);
    }

    if(!first) {
        return {};
    }

    const auto min = std::get<QString>(args.at(1).value).toLongLong();
    const auto max = std::get<QString>(args.at(2).value).toLongLong();

    ScriptResult result;
    result.cond = first.value() > min && first.value() < max;

    return result;
}

Expression ScriptParserPrivate::checkOperator(const Expression& expr)
{
    if(!m_isQuery) {
        return expr;
    }

    switch(m_current.type) {
        case(TokenType::TokColon):
            return relationalOperator(expr, Expr::Contains);
        case(TokenType::TokEquals):
            return relationalOperator(expr, Expr::Equals);
        case(TokenType::TokNot):
            return notKeyword(expr);
        case(TokenType::TokAnd):
            return logicalOperator(expr, Expr::And);
        case(TokenType::TokOr):
            return logicalOperator(expr, Expr::Or);
        case(TokenType::TokXOr):
            return logicalOperator(expr, Expr::XOr);
        case(TokenType::TokMissing):
            return metadataKeyword(expr, Expr::Missing);
        case(TokenType::TokPresent):
            return metadataKeyword(expr, Expr::Present);
        case(TokenType::TokLeftAngle):
            return relationalOperator(expr, Expr::Less, Expr::LessEqual);
        case(TokenType::TokRightAngle):
            return relationalOperator(expr, Expr::Greater, Expr::GreaterEqual);
        case(TokenType::TokBefore):
            return timeKeyword(expr, Expr::Before);
        case(TokenType::TokAfter):
            return timeKeyword(expr, Expr::After);
        case(TokenType::TokSince):
            return timeKeyword(expr, Expr::Since);
        case(TokenType::TokDuring):
            return duringKeyword(expr);
        default:
            break;
    }

    return expr;
}

void ScriptParserPrivate::reset()
{
    m_currentResult.clear();
    m_filteredCount = 0;
    m_limit         = 0;
    m_sortScript.clear();
    m_sortOrder = Qt::AscendingOrder;
    m_variables.clear();
}

ScriptParser::ScriptParser()
    : p{std::make_unique<ScriptParserPrivate>(this)}
{ }

void ScriptParser::addProvider(const ScriptVariableProvider& provider)
{
    p->m_registry->addProvider(provider);
}

void ScriptParser::addProvider(const ScriptFunctionProvider& provider)
{
    p->m_registry->addProvider(provider);
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

QString ScriptParser::evaluate(const QString& input, const ScriptContext& context)
{
    return evaluate(input, Track{}, context);
}

QString ScriptParser::evaluate(const ParsedScript& input, const ScriptContext& context)
{
    return evaluate(input, Track{}, context);
}

QString ScriptParser::evaluate(const QString& input, const Track& track)
{
    if(input.isEmpty()) {
        return {};
    }

    const auto script = parse(input);
    return evaluate(script, track);
}

QString ScriptParser::evaluate(const ParsedScript& input, const Track& track)
{
    if(!input.isValid()) {
        return {};
    }

    p->m_isQuery = false;

    return p->evaluate(input, track);
}

QString ScriptParser::evaluate(const QString& input, const Track& track, const ScriptContext& context)
{
    if(input.isEmpty()) {
        return {};
    }

    const auto script = parse(input);
    return evaluate(script, track, context);
}

QString ScriptParser::evaluate(const ParsedScript& input, const Track& track, const ScriptContext& context)
{
    return p->m_registry->withContext(context, [&] { return evaluate(input, track); });
}

QString ScriptParser::evaluate(const QString& input, const TrackList& tracks)
{
    if(input.isEmpty()) {
        return {};
    }

    const auto script = parse(input);
    return evaluate(script, tracks);
}

QString ScriptParser::evaluate(const ParsedScript& input, const TrackList& tracks)
{
    if(!input.isValid()) {
        return {};
    }

    p->m_isQuery = false;

    return p->evaluate(input, tracks);
}

QString ScriptParser::evaluate(const QString& input, const TrackList& tracks, const ScriptContext& context)
{
    if(input.isEmpty()) {
        return {};
    }

    const auto script = parse(input);
    return evaluate(script, tracks, context);
}

QString ScriptParser::evaluate(const ParsedScript& input, const TrackList& tracks, const ScriptContext& context)
{
    return p->m_registry->withContext(context, [&] { return evaluate(input, tracks); });
}

QString ScriptParser::evaluate(const QString& input, const Playlist& playlist)
{
    if(input.isEmpty()) {
        return {};
    }

    const auto script = parse(input);
    return evaluate(script, playlist);
}

QString ScriptParser::evaluate(const ParsedScript& input, const Playlist& playlist)
{
    if(!input.isValid()) {
        return {};
    }

    p->m_isQuery = false;

    return p->evaluate(input, playlist);
}

QString ScriptParser::evaluate(const QString& input, const Playlist& playlist, const ScriptContext& context)
{
    if(input.isEmpty()) {
        return {};
    }

    const auto script = parse(input);
    return evaluate(script, playlist, context);
}

QString ScriptParser::evaluate(const ParsedScript& input, const Playlist& playlist, const ScriptContext& context)
{
    return p->m_registry->withContext(context, [&] { return evaluate(input, playlist); });
}

TrackList ScriptParser::filter(const QString& input, const TrackList& tracks)
{
    if(input.isEmpty()) {
        return {};
    }

    const auto script = parseQuery(input);
    return filter(script, tracks);
}

TrackList ScriptParser::filter(const ParsedScript& input, const TrackList& tracks)
{
    if(!input.isValid()) {
        return {};
    }

    p->m_isQuery = true;

    return p->evaluateQuery(input, tracks);
}

PlaylistTrackList ScriptParser::filter(const QString& input, const PlaylistTrackList& tracks)
{
    if(input.isEmpty()) {
        return {};
    }

    const auto script = parseQuery(input);
    return filter(script, tracks);
}

PlaylistTrackList ScriptParser::filter(const ParsedScript& input, const PlaylistTrackList& tracks)
{
    if(!input.isValid()) {
        return {};
    }

    p->m_isQuery = true;

    return p->evaluateQuery(input, tracks);
}

int ScriptParser::cacheLimit() const
{
    return p->m_scriptCache.limit();
}

void ScriptParser::setCacheLimit(int limit)
{
    p->m_scriptCache.setLimit(limit);
    p->m_queryCache.setLimit(limit);
    p->m_boundScriptCache.setLimit(limit);
    p->m_boundQueryCache.setLimit(limit);
}

void ScriptParser::clearCache()
{
    p->m_scriptCache.clear();
    p->m_queryCache.clear();
    p->m_boundScriptCache.clear();
    p->m_boundQueryCache.clear();
}
} // namespace Fooyin
