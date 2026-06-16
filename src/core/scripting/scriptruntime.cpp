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

#include "scriptruntime.h"

#include <core/constants.h>
#include <core/library/tracksort.h>
#include <core/scripting/scriptenvironmenthelpers.h>
#include <utils/helpers.h>
#include <utils/stringutils.h>
#include <utils/utils.h>

using namespace Qt::StringLiterals;

using TokenType = Fooyin::ScriptScanner::TokenType;

namespace Fooyin {
namespace {
std::atomic_uint64_t& nextParsedScriptId()
{
    static std::atomic_uint64_t nextParsedScriptId{1};
    return nextParsedScriptId;
}

QDateTime evalDate(const Expression& expr)
{
    if(!std::holds_alternative<QString>(expr.value)) {
        return {};
    }

    const QString value = std::get<QString>(expr.value);
    return Utils::dateStringToDate(value);
}

struct DateRange
{
    QDateTime start;
    QDateTime end;
};

DateRange calculateDateRange(const Expression& expr)
{
    if(!std::holds_alternative<QString>(expr.value)) {
        return {};
    }

    const QString dateString = std::get<QString>(expr.value);

    DateRange range;

    const auto formats = Utils::dateFormats();
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

QStringList evalStringList(const ScriptResult& evalExpr, const QStringList& result)
{
    QStringList listResult;
    const QStringList values = evalExpr.value.split(QLatin1String{Constants::UnitSeparator});
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

bool matchSearch(const Track& track, const QString& search, bool phraseMatch)
{
    if(search.isEmpty()) {
        return true;
    }

    if(phraseMatch) {
        return track.hasMatch(search);
    }

    const QStringList terms = search.split(u' ', Qt::SkipEmptyParts);
    return std::ranges::all_of(terms, [&track](const QString& term) { return track.hasMatch(term); });
}

bool isQueryExpression(Expr::Type type)
{
    using Type = Expr::Type;
    switch(type) {
        case Type::Not:
        case Type::Group:
        case Type::And:
        case Type::Or:
        case Type::XOr:
        case Type::Equals:
        case Type::Contains:
        case Type::Greater:
        case Type::GreaterEqual:
        case Type::Less:
        case Type::LessEqual:
        case Type::SortAscending:
        case Type::SortDescending:
        case Type::All:
        case Type::Missing:
        case Type::Present:
        case Type::Before:
        case Type::After:
        case Type::Since:
        case Type::During:
        case Type::Date:
        case Type::Limit:
            return true;
        case Type::Null:
        case Type::Literal:
        case Type::Variable:
        case Type::VariableList:
        case Type::VariableRaw:
        case Type::Function:
        case Type::FunctionArg:
        case Type::Conditional:
        case Type::QuotedLiteral:
            break;
    }

    return false;
}

ParsedScript makeLiteralQuery(const QString& input)
{
    ParsedScript script;
    script.input   = input;
    script.cacheId = nextParsedScriptId().fetch_add(1, std::memory_order_relaxed);
    script.expressions.emplace_back(Expr::Literal, input.trimmed());
    return script;
}

ScriptResult evalLiteral(const BoundExpression& exp)
{
    ScriptResult result;
    result.value = std::get<QString>(exp.value);
    result.cond  = true;
    return result;
}

Expression normaliseQueryField(Expression field)
{
    if(field.type != Expr::Literal) {
        return field;
    }

    auto& value = std::get<QString>(field.value);
    value       = value.trimmed().toUpper();
    field.type  = Expr::Variable;

    return field;
}
} // namespace

ScriptRuntime::ScriptRuntime()
    : m_registry{std::make_unique<ScriptRegistry>()}
    , m_isQuery{false}
    , m_sortOrder{Qt::AscendingOrder}
    , m_limit{0}
    , m_filteredCount{0}
{ }

ScriptRuntime::~ScriptRuntime() = default;

void ScriptRuntime::addProvider(const ScriptVariableProvider& provider)
{
    m_registry->addProvider(provider);
}

void ScriptRuntime::addProvider(const ScriptFunctionProvider& provider)
{
    m_registry->addProvider(provider);
}

void ScriptRuntime::advance()
{
    m_previous = m_current;

    m_current = m_scanner.next();
    if(m_current.type == TokenType::TokError) {
        errorAtCurrent(m_current.value.toString());
    }
}

void ScriptRuntime::consume(TokenType type, const QString& message)
{
    if(m_current.type == type) {
        advance();
        return;
    }
    errorAtCurrent(message);
}

bool ScriptRuntime::currentToken(TokenType type) const
{
    return m_current.type == type;
}

bool ScriptRuntime::match(TokenType type)
{
    if(!currentToken(type)) {
        return false;
    }
    advance();
    return true;
}

void ScriptRuntime::errorAtCurrent(const QString& message)
{
    errorAt(m_current, message);
}

void ScriptRuntime::errorAt(const ScriptScanner::Token& token, const QString& message)
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

void ScriptRuntime::error(const QString& message)
{
    errorAt(m_previous, message);
}

Expression ScriptRuntime::expression()
{
    advance();

    switch(m_previous.type) {
        case TokenType::TokVar:
            return variable();
        case TokenType::TokFunc:
            return function();
        case TokenType::TokQuote:
            return quote();
        case TokenType::TokLeftSquare:
            return conditional();
        case TokenType::TokEscape:
            advance();
            return literal();
        case TokenType::TokLeftParen:
            return m_isQuery ? group() : literal();
        case TokenType::TokNot:
            return m_isQuery ? notKeyword({}) : literal();
        case TokenType::TokAll:
            return m_isQuery ? Expression{Expr::All} : literal();
        case TokenType::TokSort:
            return m_isQuery ? sort() : literal();
        case TokenType::TokLimit:
            return limit();
        case TokenType::TokAnd:
        case TokenType::TokOr:
        case TokenType::TokXOr:
        case TokenType::TokMissing:
        case TokenType::TokPresent:
        case TokenType::TokColon:
        case TokenType::TokEquals:
        case TokenType::TokLeftAngle:
        case TokenType::TokRightAngle:
        case TokenType::TokRightParen:
        case TokenType::TokComma:
        case TokenType::TokBy:
        case TokenType::TokBefore:
        case TokenType::TokAfter:
        case TokenType::TokSince:
        case TokenType::TokDuring:
        case TokenType::TokLast:
        case TokenType::TokSecond:
        case TokenType::TokMinute:
        case TokenType::TokHour:
        case TokenType::TokDay:
        case TokenType::TokWeek:
        case TokenType::TokMonth:
        case TokenType::TokYear:
        case TokenType::TokRightSquare:
        case TokenType::TokAscending:
        case TokenType::TokDescending:
        case TokenType::TokSlash:
        case TokenType::TokLiteral:
        case TokenType::TokPlus:
        case TokenType::TokMinus:
            return literal();
        case TokenType::TokEos:
        case TokenType::TokError:
            break;
    }

    return {Expr::Null};
}

Expression ScriptRuntime::literal() const
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

Expression ScriptRuntime::quote()
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

Expression ScriptRuntime::variable()
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

Expression ScriptRuntime::function()
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

Expression ScriptRuntime::functionArgs()
{
    Expression expr{Expr::FunctionArg};
    ExpressionList funcExpr;

    while(!currentToken(TokenType::TokComma) && !currentToken(TokenType::TokRightParen)
          && !currentToken(TokenType::TokEos)) {
        if(currentToken(TokenType::TokEscape) && m_scanner.peekNext().type == TokenType::TokLeftAngle) {
            const bool hasClosing    = hasClosingAngleBeforeArgEnd(2);
            const Expression argExpr = hasClosing ? consumeAngleLiteral(true) : consumeEscapedAngleLiteral();
            funcExpr.emplace_back(argExpr);
            continue;
        }

        if(currentToken(TokenType::TokLeftAngle) && hasClosingAngleBeforeArgEnd()) {
            funcExpr.emplace_back(consumeAngleLiteral(false));
            continue;
        }

        const Expression argExpr = expression();
        if(argExpr.type != Expr::Null) {
            funcExpr.emplace_back(argExpr);
        }
    }

    expr.value = funcExpr;
    return expr;
}

bool ScriptRuntime::hasClosingAngleBeforeArgEnd(int startDelta)
{
    bool inQuote{false};

    for(int delta{startDelta};; ++delta) {
        const auto token = m_scanner.peekNext(delta);
        switch(token.type) {
            case TokenType::TokQuote:
                inQuote = !inQuote;
                break;
            case TokenType::TokRightAngle:
                if(!inQuote) {
                    return true;
                }
                break;
            case TokenType::TokRightParen:
            case TokenType::TokRightSquare:
            case TokenType::TokEos:
                if(!inQuote) {
                    return false;
                }
                break;
            case TokenType::TokError:
                return false;
            default:
                break;
        }
    }
}

Expression ScriptRuntime::consumeAngleLiteral(bool escaped)
{
    Expression expr{Expr::Literal};
    QString value;

    if(escaped && currentToken(TokenType::TokEscape)) {
        advance();
        value.append(m_previous.value);
    }

    if(currentToken(TokenType::TokLeftAngle)) {
        advance();
        value.append(m_previous.value);
    }

    while(!currentToken(TokenType::TokRightAngle) && !currentToken(TokenType::TokEos)) {
        advance();
        value.append(m_previous.value);
    }

    if(currentToken(TokenType::TokRightAngle)) {
        advance();
        value.append(m_previous.value);
    }

    expr.value = value;
    return expr;
}

Expression ScriptRuntime::consumeEscapedAngleLiteral()
{
    Expression expr{Expr::Literal};
    QString value;

    if(currentToken(TokenType::TokEscape)) {
        advance();
        value.append(m_previous.value);
    }

    if(currentToken(TokenType::TokLeftAngle)) {
        advance();
        value.append(m_previous.value);
    }

    expr.value = value;
    return expr;
}

Expression ScriptRuntime::conditional()
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

Expression ScriptRuntime::group()
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

Expression ScriptRuntime::notKeyword(const Expression& key)
{
    Expression expr{Expr::Not};
    ExpressionList args;

    if(currentToken(TokenType::TokNot)) {
        advance();
        const Expression argExpr = checkOperator(key);
        if(argExpr.type != Expr::Null && argExpr.type != key.type) {
            args.emplace_back(argExpr);
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

Expression ScriptRuntime::logicalOperator(const Expression& key, Expr::Type type)
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

Expression ScriptRuntime::relationalOperator(const Expression& key, Expr::Type type, Expr::Type equalsType)
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

Expression ScriptRuntime::metadataKeyword(const Expression& key, Expr::Type type)
{
    Expression expr{type};
    ExpressionList args;

    advance();

    args.emplace_back(normaliseQueryField(key));

    expr.value = args;
    return expr;
}

Expression ScriptRuntime::timeKeyword(const Expression& key, Expr::Type type)
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

Expression ScriptRuntime::duringKeyword(const Expression& key)
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

        if(currentToken(TokenType::TokYear)) {
            date  = date.addYears(-count);
            valid = true;
        }
        else if(currentToken(TokenType::TokMonth)) {
            date  = date.addMonths(-count);
            valid = true;
        }
        else if(currentToken(TokenType::TokWeek)) {
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

Expression ScriptRuntime::sort()
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

Expression ScriptRuntime::limit()
{
    Expression expr{Expr::Limit};

    if(!currentToken(TokenType::TokEos)) {
        advance();
        expr.value = m_previous.value.toString();
    }

    return expr;
}

ScriptResult ScriptRuntime::evalExpression(const BoundExpression& exp, const auto& tracks)
{
    switch(exp.type) {
        case Expr::Literal:
        case Expr::QuotedLiteral:
            return evalLiteral(exp);
        case Expr::Variable:
            return evalVariable(exp, tracks);
        case Expr::VariableList:
            return evalVariableList(exp, tracks);
        case Expr::VariableRaw:
            return evalVariableRaw(exp, tracks);
        case Expr::Function:
            return evalFunction(exp, tracks);
        case Expr::FunctionArg:
            return evalFunctionArg(exp, tracks);
        case Expr::Conditional:
            return evalConditional(exp, tracks);
        case Expr::Not:
            return evalNot(exp, tracks);
        case Expr::Group:
            return evalGroup(exp, tracks);
        case Expr::And:
            return evalAnd(exp, tracks);
        case Expr::Or:
            return evalOr(exp, tracks);
        case Expr::XOr:
            return evalXOr(exp, tracks);
        case Expr::Missing:
            return evalMissing(exp, tracks);
        case Expr::Present:
            return evalPresent(exp, tracks);
        case Expr::Equals:
            return evalEquals(exp, tracks);
        case Expr::Contains:
            return evalContains(exp, tracks);
        case Expr::Greater:
            return compareValues(exp, tracks, std::greater<>());
        case Expr::GreaterEqual:
            return compareValues(exp, tracks, std::greater_equal<>());
        case Expr::Less:
            return compareValues(exp, tracks, std::less<>());
        case Expr::LessEqual:
            return compareValues(exp, tracks, std::less_equal<>());
        case Expr::Before:
            return compareDates(exp, tracks, std::less<>());
        case Expr::After:
            return compareDates(exp, tracks, std::greater<>());
        case Expr::Since:
            return compareDates(exp, tracks, std::greater_equal<>());
        case Expr::During:
            return compareDateRange(exp, tracks);
        case Expr::Limit:
            return evalLimit(exp);
        case Expr::SortAscending:
        case Expr::SortDescending:
            return evalSort(exp);
        case Expr::All:
            return ScriptResult{.value = {}, .cond = true};
        case Expr::Null:
        default:
            return {};
    }
}

ScriptResult ScriptRuntime::evalVariable(const BoundExpression& exp, const auto& tracks)
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

ScriptResult ScriptRuntime::evalVariableList(const BoundExpression& exp, const auto& tracks)
{
    const auto& var = std::get<QString>(exp.value);
    return m_registry->value(exp.variableKind, var, makeScriptSubject(tracks));
}

ScriptResult ScriptRuntime::evalVariableRaw(const BoundExpression& exp, const auto& tracks)
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

ScriptResult ScriptRuntime::evalFunction(const BoundExpression& exp, const auto& tracks)
{
    const auto& func = std::get<BoundFunctionValue>(exp.value);
    auto toResult    = [](QString value) {
        const bool cond = !value.isEmpty();
        return ScriptResult{.value = std::move(value), .cond = cond};
    };

    switch(func.kind) {
        case FunctionKind::Get: {
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
        case FunctionKind::Put:
        case FunctionKind::Puts: {
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
        case FunctionKind::If: {
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
        case FunctionKind::If2: {
            const auto size = func.args.size();
            if(size < 1 || size > 2) {
                return {};
            }

            ScriptResult first = evalExpression(func.args.at(0), tracks);
            if(first.cond) {
                return first;
            }
            if(size == 2) {
                return evalExpression(func.args.at(1), tracks);
            }
            return {};
        }
        case FunctionKind::If3: {
            const auto size = func.args.size();
            if(size < 2) {
                return {};
            }

            for(size_t i{0}; i + 1 < size; ++i) {
                const ScriptResult current = evalExpression(func.args.at(i), tracks);
                if(current.cond) {
                    return current;
                }
            }

            return evalExpression(func.args.back(), tracks);
        }
        case FunctionKind::IfEqual: {
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
        case FunctionKind::IfGreater: {
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
        case FunctionKind::IfLonger: {
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
        case FunctionKind::Add:
        case FunctionKind::Sub:
        case FunctionKind::Mul:
        case FunctionKind::Div: {
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
        case FunctionKind::Mod: {
            const auto size = static_cast<qsizetype>(func.args.size());
            if(size < 2) {
                return {};
            }

            int total = evalExpression(func.args.front(), tracks).value.toInt();
            for(qsizetype i{1}; i < size; ++i) {
                total %= evalExpression(func.args.at(i), tracks).value.toInt();
            }
            return toResult(QString::number(total));
        }
        case FunctionKind::Num: {
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
        case FunctionKind::Pad:
        case FunctionKind::PadRight: {
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
        case FunctionKind::Generic:
            break;
    }

    ScriptValueList args;
    args.reserve(func.args.size());
    std::ranges::transform(func.args, std::back_inserter(args),
                           [this, &tracks](const BoundExpression& arg) { return evalExpression(arg, tracks); });
    return m_registry->function(func.functionId, func.name, args, makeScriptSubject(tracks));
}

ScriptResult ScriptRuntime::evalFunctionArg(const BoundExpression& exp, const auto& tracks)
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

ScriptResult ScriptRuntime::evalConditional(const BoundExpression& exp, const auto& tracks)
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

ScriptResult ScriptRuntime::evalNot(const BoundExpression& exp, const auto& tracks)
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

ScriptResult ScriptRuntime::evalGroup(const BoundExpression& exp, const auto& tracks)
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

ScriptResult ScriptRuntime::evalAnd(const BoundExpression& exp, const auto& tracks)
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

ScriptResult ScriptRuntime::evalOr(const BoundExpression& exp, const auto& tracks)
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

ScriptResult ScriptRuntime::evalXOr(const BoundExpression& exp, const auto& tracks)
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

ScriptResult ScriptRuntime::evalMissing(const BoundExpression& exp, const auto& tracks)
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

ScriptResult ScriptRuntime::evalPresent(const BoundExpression& exp, const auto& tracks)
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

ScriptResult ScriptRuntime::evalEquals(const BoundExpression& exp, const auto& tracks)
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

ScriptResult ScriptRuntime::evalContains(const BoundExpression& exp, const auto& tracks)
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
    result.cond = Utils::foldForSearch(first.value).contains(Utils::foldForSearch(second.value));

    return result;
}

ScriptResult ScriptRuntime::evalContains(const BoundExpression& exp, const Track& track)
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
        result.cond = Utils::foldForSearch(first.value).contains(Utils::foldForSearch(second.value));
    }

    return result;
}

ScriptResult ScriptRuntime::evalLimit(const BoundExpression& exp)
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

ScriptResult ScriptRuntime::evalSort(const BoundExpression& exp)
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

ParsedScript ScriptRuntime::parse(const QString& input, ScriptScanner::WhitespaceMode whitespaceMode)
{
    if(input.isEmpty()) {
        return {};
    }

    m_isQuery = false;
    m_scanner.setWhitespaceMode(whitespaceMode);
    m_currentScript = {};

    QString cacheKey{input};

    if(whitespaceMode != ScriptScanner::WhitespaceMode::IgnoreLayout) {
        const QString separator = QString::fromLatin1(Constants::UnitSeparator);
        QString key{separator};
        key += QString::number(static_cast<int>(whitespaceMode));
        key += separator;
        key += input;
        cacheKey = key;
    }

    if(m_scriptCache.contains(cacheKey)) {
        return m_scriptCache.get(cacheKey);
    }

    m_currentInput          = input;
    m_currentScript.input   = input;
    m_currentScript.cacheId = nextParsedScriptId().fetch_add(1, std::memory_order_relaxed);
    m_scanner.setup(input);

    advance();
    while(m_current.type != TokenType::TokEos) {
        const Expression expr = expression();
        if(expr.type != Expr::Null) {
            m_currentScript.expressions.emplace_back(expr);
        }
    }

    consume(TokenType::TokEos, QObject::tr("Expected end of script"));
    m_scriptCache.insert(cacheKey, m_currentScript);

    return m_currentScript;
}

ParsedScript ScriptRuntime::parseQuery(const QString& input)
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

const BoundScript& ScriptRuntime::bind(const ParsedScript& input)
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

QString ScriptRuntime::evaluate(const ParsedScript& input, const Track& track)
{
    return evaluateImpl(input, track);
}

QString ScriptRuntime::evaluate(const ParsedScript& input, const TrackList& tracks)
{
    return evaluateImpl(input, tracks);
}

QString ScriptRuntime::evaluate(const ParsedScript& input, const Playlist& playlist)
{
    return evaluateImpl(input, playlist);
}

template <typename Tracks>
QString ScriptRuntime::evaluateImpl(const ParsedScript& input, const Tracks& tracks)
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

template <typename TrackListType, typename UpdateContext>
TrackListType ScriptRuntime::evaluateQuery(const ParsedScript& input, const TrackListType& tracks,
                                           const ScriptSearchOptions& options, const ScriptContext& context,
                                           UpdateContext&& updateContext)
{
    if(!input.isValid()) {
        return {};
    }

    const BoundScript bound = bind(input);
    if(!bound.isValid()) {
        return {};
    }

    reset();
    TrackListType filteredTracks;

    const bool useSearchScript      = !options.script.isEmpty();
    const bool wasQuery             = std::exchange(m_isQuery, false);
    const ParsedScript searchScript = useSearchScript ? parse(options.script) : ParsedScript{};
    m_isQuery                       = wasQuery;

    const auto literalMatches = [&](const auto& item, const QString& search, bool phraseMatch) {
        const Track& track = [&]() -> const Track& {
            if constexpr(std::is_same_v<TrackListType, PlaylistTrackList>) {
                return item.track;
            }
            else {
                return item;
            }
        }();

        if(!useSearchScript) {
            return matchSearch(track, search, phraseMatch);
        }
        if(!searchScript.isValid()) {
            return false;
        }

        if(updateContext) {
            updateContext(item);
        }

        const bool wasQueryEval = m_isQuery;
        m_isQuery               = false;
        const QString text      = m_registry->withContext(context, [&] { return evaluate(searchScript, track); });
        m_isQuery               = wasQueryEval;
        return TrackQueryFilter::matchesSearch(text, search, options.mode, phraseMatch);
    };

    if(bound.expressions.size() == 1) {
        const auto& firstExpr = bound.expressions.front();
        if(firstExpr.type == Expr::Literal || firstExpr.type == Expr::QuotedLiteral) {
            // Simple search query - just match all terms in metadata/filepath
            const auto& search = std::get<QString>(firstExpr.value);
            filteredTracks     = Utils::filter(tracks, [&literalMatches, &search, &firstExpr](const auto& track) {
                return literalMatches(track, search, firstExpr.type == Expr::QuotedLiteral);
            });
        }
        else if(!isQueryExpression(firstExpr.type)) {
            return {};
        }
    }

    if(filteredTracks.empty()) {
        for(const auto& track : tracks) {
            const bool matches = std::ranges::all_of(bound.expressions, [&](const auto& expr) {
                if(expr.type == Expr::Literal || expr.type == Expr::QuotedLiteral) {
                    // Assume simple search query
                    const auto& search = std::get<QString>(expr.value);
                    return literalMatches(track, search, expr.type == Expr::QuotedLiteral);
                }

                if constexpr(std::is_same_v<TrackListType, PlaylistTrackList>) {
                    return evalExpression(expr, track.track).cond;
                }
                else {
                    return evalExpression(expr, track).cond;
                }
            });

            if(matches) {
                filteredTracks.emplace_back(track);
            }
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

    if(m_limit > 0 && std::cmp_greater(filteredTracks.size(), m_limit)) {
        filteredTracks.resize(static_cast<size_t>(m_limit));
    }

    return filteredTracks;
}

TrackList ScriptRuntime::filterQuery(const QString& search, const TrackList& tracks, const ScriptSearchOptions& options,
                                     const ScriptContext& context,
                                     const std::function<void(const Track&)>& updateContext)
{
    if(search.isEmpty()) {
        return {};
    }

    auto script = parseQuery(search);
    if(!ScriptParser::canEvaluateAsQuery(script)) {
        script = makeLiteralQuery(search);
    }

    m_isQuery = true;
    return evaluateQuery(script, tracks, options, context, updateContext);
}

PlaylistTrackList ScriptRuntime::filterQuery(const QString& search, const PlaylistTrackList& tracks,
                                             const ScriptSearchOptions& options, const ScriptContext& context,
                                             const std::function<void(const PlaylistTrack&)>& updateContext)
{
    if(search.isEmpty()) {
        return {};
    }

    auto script = parseQuery(search);
    if(!ScriptParser::canEvaluateAsQuery(script)) {
        script = makeLiteralQuery(search);
    }

    m_isQuery = true;
    return evaluateQuery(script, tracks, options, context, updateContext);
}

ScriptResult ScriptRuntime::compareValues(const BoundExpression& exp, const auto& tracks, const auto& comparator)
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

ScriptResult ScriptRuntime::compareDates(const BoundExpression& exp, const auto& tracks, const auto& comparator)
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

ScriptResult ScriptRuntime::compareDateRange(const BoundExpression& exp, const auto& tracks)
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

Expression ScriptRuntime::checkOperator(const Expression& expr)
{
    if(!m_isQuery) {
        return expr;
    }

    switch(m_current.type) {
        case TokenType::TokColon:
            return relationalOperator(expr, Expr::Contains);
        case TokenType::TokEquals:
            return relationalOperator(expr, Expr::Equals);
        case TokenType::TokNot:
            return notKeyword(expr);
        case TokenType::TokAnd:
            return logicalOperator(expr, Expr::And);
        case TokenType::TokOr:
            return logicalOperator(expr, Expr::Or);
        case TokenType::TokXOr:
            return logicalOperator(expr, Expr::XOr);
        case TokenType::TokMissing:
            return metadataKeyword(expr, Expr::Missing);
        case TokenType::TokPresent:
            return metadataKeyword(expr, Expr::Present);
        case TokenType::TokLeftAngle:
            return relationalOperator(expr, Expr::Less, Expr::LessEqual);
        case TokenType::TokRightAngle:
            return relationalOperator(expr, Expr::Greater, Expr::GreaterEqual);
        case TokenType::TokBefore:
            return timeKeyword(expr, Expr::Before);
        case TokenType::TokAfter:
            return timeKeyword(expr, Expr::After);
        case TokenType::TokSince:
            return timeKeyword(expr, Expr::Since);
        case TokenType::TokDuring:
            return duringKeyword(expr);
        default:
            break;
    }

    return expr;
}

void ScriptRuntime::reset()
{
    m_currentResult.clear();
    m_filteredCount = 0;
    m_limit         = 0;
    m_sortScript.clear();
    m_sortOrder = Qt::AscendingOrder;
    m_variables.clear();
}

int ScriptRuntime::cacheLimit() const
{
    return m_scriptCache.limit();
}

void ScriptRuntime::setCacheLimit(int limit)
{
    m_scriptCache.setLimit(limit);
    m_queryCache.setLimit(limit);
    m_boundScriptCache.setLimit(limit);
    m_boundQueryCache.setLimit(limit);
}

void ScriptRuntime::clearCache()
{
    m_scriptCache.clear();
    m_queryCache.clear();
    m_boundScriptCache.clear();
    m_boundQueryCache.clear();
}

void ScriptRuntime::setQueryMode(bool isQuery)
{
    m_isQuery = isQuery;
}
} // namespace Fooyin
