/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include "scriptruntime.h"

namespace Fooyin {
namespace {
ScriptScanner::WhitespaceMode whitespaceMode(ScriptWhitespaceMode mode)
{
    switch(mode) {
        case ScriptWhitespaceMode::Preserve:
            return ScriptScanner::WhitespaceMode::Preserve;
        case ScriptWhitespaceMode::IgnoreLayout:
            break;
    }
    return ScriptScanner::WhitespaceMode::IgnoreLayout;
}

bool isQueryExpression(Expr::Type type)
{
    switch(type) {
        case Expr::Type::Not:
        case Expr::Type::Group:
        case Expr::Type::And:
        case Expr::Type::Or:
        case Expr::Type::XOr:
        case Expr::Type::Equals:
        case Expr::Type::Contains:
        case Expr::Type::Greater:
        case Expr::Type::GreaterEqual:
        case Expr::Type::Less:
        case Expr::Type::LessEqual:
        case Expr::Type::SortAscending:
        case Expr::Type::SortDescending:
        case Expr::Type::All:
        case Expr::Type::Missing:
        case Expr::Type::Present:
        case Expr::Type::Before:
        case Expr::Type::After:
        case Expr::Type::Since:
        case Expr::Type::During:
        case Expr::Type::Date:
        case Expr::Type::Limit:
            return true;
        case Expr::Type::Null:
        case Expr::Type::Literal:
        case Expr::Type::Variable:
        case Expr::Type::VariableList:
        case Expr::Type::VariableRaw:
        case Expr::Type::Function:
        case Expr::Type::FunctionArg:
        case Expr::Type::Conditional:
        case Expr::Type::QuotedLiteral:
            break;
    }

    return false;
}
} // namespace

ScriptParser::ScriptParser()
    : p{std::make_unique<ScriptRuntime>(&ScriptRegistry::instance())}
{ }

ScriptParser::ScriptParser(ScriptRegistry* registry)
    : p{std::make_unique<ScriptRuntime>(registry)}
{ }

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

bool ScriptParser::canEvaluateAsQuery(const ParsedScript& input)
{
    if(!input.isValid()) {
        return false;
    }

    return std::ranges::all_of(input.expressions, [](const Expression& expr) {
        return expr.type == Expr::Literal || expr.type == Expr::QuotedLiteral || isQueryExpression(expr.type);
    });
}

bool ScriptParser::containsQueryExpression(const ParsedScript& input)
{
    return input.isValid()
        && std::ranges::any_of(input.expressions, [](const Expression& expr) { return isQueryExpression(expr.type); });
}

ScriptDependencies ScriptParser::dependencies(const ParsedScript& input)
{
    return p->dependencies(input);
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
    return evaluate(input, context, {});
}

QString ScriptParser::evaluate(const QString& input, const ScriptContext& context,
                               const ScriptEvaluationOptions& options)
{
    if(context.track) {
        return evaluate(input, *context.track, context, options);
    }
    if(context.tracks) {
        return evaluate(input, *context.tracks, context, options);
    }
    if(context.playlist) {
        return evaluate(input, *context.playlist, context, options);
    }
    return evaluate(input, Track{}, context, options);
}

QString ScriptParser::evaluate(const ParsedScript& input, const ScriptContext& context)
{
    return evaluate(input, context, {});
}

QString ScriptParser::evaluate(const ParsedScript& input, const ScriptContext& context,
                               const ScriptEvaluationOptions& options)
{
    if(context.track) {
        return evaluate(input, *context.track, context, options);
    }
    if(context.tracks) {
        return evaluate(input, *context.tracks, context, options);
    }
    if(context.playlist) {
        return evaluate(input, *context.playlist, context, options);
    }
    return evaluate(input, Track{}, context, options);
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

    p->setQueryMode(false);

    return p->evaluate(input, track);
}

QString ScriptParser::evaluate(const QString& input, const Track& track, const ScriptContext& context)
{
    return evaluate(input, track, context, {});
}

QString ScriptParser::evaluate(const QString& input, const Track& track, const ScriptContext& context,
                               const ScriptEvaluationOptions& options)
{
    if(input.isEmpty()) {
        return {};
    }

    const auto script = p->parse(input, whitespaceMode(options.whitespaceMode));
    return evaluate(script, track, context, options);
}

QString ScriptParser::evaluate(const ParsedScript& input, const Track& track, const ScriptContext& context)
{
    return evaluate(input, track, context, {});
}

QString ScriptParser::evaluate(const ParsedScript& input, const Track& track, const ScriptContext& context,
                               const ScriptEvaluationOptions& options)
{
    p->setQueryMode(false);
    return p->withContext(context, [&] { return p->evaluate(input, track, options); });
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

    p->setQueryMode(false);

    return p->evaluate(input, tracks);
}

QString ScriptParser::evaluate(const QString& input, const TrackList& tracks, const ScriptContext& context)
{
    return evaluate(input, tracks, context, {});
}

QString ScriptParser::evaluate(const QString& input, const TrackList& tracks, const ScriptContext& context,
                               const ScriptEvaluationOptions& options)
{
    if(input.isEmpty()) {
        return {};
    }

    const auto script = p->parse(input, whitespaceMode(options.whitespaceMode));
    return evaluate(script, tracks, context, options);
}

QString ScriptParser::evaluate(const ParsedScript& input, const TrackList& tracks, const ScriptContext& context)
{
    return evaluate(input, tracks, context, {});
}

QString ScriptParser::evaluate(const ParsedScript& input, const TrackList& tracks, const ScriptContext& context,
                               const ScriptEvaluationOptions& options)
{
    p->setQueryMode(false);
    return p->withContext(context, [&] { return p->evaluate(input, tracks, options); });
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

    p->setQueryMode(false);

    return p->evaluate(input, playlist);
}

QString ScriptParser::evaluate(const QString& input, const Playlist& playlist, const ScriptContext& context)
{
    return evaluate(input, playlist, context, {});
}

QString ScriptParser::evaluate(const QString& input, const Playlist& playlist, const ScriptContext& context,
                               const ScriptEvaluationOptions& options)
{
    if(input.isEmpty()) {
        return {};
    }

    const auto script = p->parse(input, whitespaceMode(options.whitespaceMode));
    return evaluate(script, playlist, context, options);
}

QString ScriptParser::evaluate(const ParsedScript& input, const Playlist& playlist, const ScriptContext& context)
{
    return evaluate(input, playlist, context, {});
}

QString ScriptParser::evaluate(const ParsedScript& input, const Playlist& playlist, const ScriptContext& context,
                               const ScriptEvaluationOptions& options)
{
    p->setQueryMode(false);
    return p->withContext(context, [&] { return p->evaluate(input, playlist, options); });
}

void ScriptParser::withContext(const ScriptContext& context, const std::function<void()>& operation)
{
    p->withContext(context, operation);
}

int ScriptParser::cacheLimit() const
{
    return p->cacheLimit();
}

void ScriptParser::setCacheLimit(int limit)
{
    p->setCacheLimit(limit);
}

void ScriptParser::clearCache()
{
    p->clearCache();
}
} // namespace Fooyin
