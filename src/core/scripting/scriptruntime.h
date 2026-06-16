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

#pragma once

#include "scriptbinder.h"
#include "scriptcache.h"
#include "scriptregistry.h"

#include <core/scripting/scriptscanner.h>
#include <core/scripting/trackqueryfilter.h>

#include <QDateTime>

#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>

namespace Fooyin {
class ScriptFunctionProvider;
class ScriptVariableProvider;

class ScriptRuntime
{
public:
    ScriptRuntime();
    ~ScriptRuntime();

    void addProvider(const ScriptVariableProvider& provider);
    void addProvider(const ScriptFunctionProvider& provider);

    ParsedScript parse(const QString& input,
                       ScriptScanner::WhitespaceMode whitespaceMode = ScriptScanner::WhitespaceMode::IgnoreLayout);
    ParsedScript parseQuery(const QString& input);

    QString evaluate(const ParsedScript& input, const Track& track);
    QString evaluate(const ParsedScript& input, const TrackList& tracks);
    QString evaluate(const ParsedScript& input, const Playlist& playlist);

    [[nodiscard]] TrackList filterQuery(const QString& search, const TrackList& tracks,
                                        const ScriptSearchOptions& options, const ScriptContext& context = {},
                                        const std::function<void(const Track&)>& updateContext = {});
    [[nodiscard]] PlaylistTrackList filterQuery(const QString& search, const PlaylistTrackList& tracks,
                                                const ScriptSearchOptions& options, const ScriptContext& context = {},
                                                const std::function<void(const PlaylistTrack&)>& updateContext = {});

    [[nodiscard]] int cacheLimit() const;
    void setCacheLimit(int limit);
    void clearCache();

    template <typename Func>
    auto withContext(const ScriptContext& context, Func&& func)
    {
        return m_registry->withContext(context, std::forward<Func>(func));
    }

    void setQueryMode(bool isQuery);

private:
    void advance();
    void consume(ScriptScanner::TokenType type, const QString& message);
    [[nodiscard]] bool currentToken(ScriptScanner::TokenType type) const;
    bool match(ScriptScanner::TokenType type);

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

    template <typename Tracks>
    QString evaluateImpl(const ParsedScript& input, const Tracks& tracks);

    ScriptResult evalExpression(const BoundExpression& exp, const auto& tracks);
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

    template <typename TrackListType, typename UpdateContext>
    TrackListType evaluateQuery(const ParsedScript& input, const TrackListType& tracks,
                                const ScriptSearchOptions& options, const ScriptContext& context,
                                UpdateContext&& updateContext);

    ScriptResult compareValues(const BoundExpression& exp, const auto& tracks, const auto& comparator);
    ScriptResult compareDates(const BoundExpression& exp, const auto& tracks, const auto& comparator);
    ScriptResult compareDateRange(const BoundExpression& exp, const auto& tracks);
    Expression checkOperator(const Expression& expr);

    bool hasClosingAngleBeforeArgEnd(int startDelta = 1);
    Expression consumeAngleLiteral(bool escaped);
    Expression consumeEscapedAngleLiteral();

    void reset();

    ScriptScanner m_scanner;
    std::unique_ptr<ScriptRegistry> m_registry;

    ScriptScanner::Token m_current;
    ScriptScanner::Token m_previous;

    bool m_isQuery;
    QString m_currentInput;
    ParsedScript m_currentScript;
    ScriptCache m_scriptCache;
    ScriptCache m_queryCache;
    BoundScriptCache m_boundScriptCache;
    BoundScriptCache m_boundQueryCache;
    BoundScript m_currentBoundScript;
    QStringList m_currentResult;

    QString m_sortScript;
    Qt::SortOrder m_sortOrder;
    int m_limit;
    int m_filteredCount;
    std::unordered_map<QString, QString> m_variables;
};
} // namespace Fooyin
