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

namespace {
Fooyin::ScriptScanner::WhitespaceMode whitespaceMode(Fooyin::ScriptWhitespaceMode mode)
{
    switch(mode) {
        case Fooyin::ScriptWhitespaceMode::Preserve:
            return Fooyin::ScriptScanner::WhitespaceMode::Preserve;
        case Fooyin::ScriptWhitespaceMode::IgnoreLayout:
            break;
    }
    return Fooyin::ScriptScanner::WhitespaceMode::IgnoreLayout;
}

bool isQueryExpression(Fooyin::Expr::Type type)
{
    using Type = Fooyin::Expr::Type;
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

bool canEvaluateAsQuery(const Fooyin::ParsedScript& script)
{
    if(!script.isValid()) {
        return false;
    }

    return std::ranges::all_of(script.expressions, [](const Fooyin::Expression& expr) {
        return expr.type == Fooyin::Expr::Literal || expr.type == Fooyin::Expr::QuotedLiteral
            || isQueryExpression(expr.type);
    });
}

bool containsQueryExpression(const Fooyin::ParsedScript& script)
{
    return script.isValid() && std::ranges::any_of(script.expressions, [](const Fooyin::Expression& expr) {
               return isQueryExpression(expr.type);
           });
}
} // namespace

namespace Fooyin {
ScriptParser::ScriptParser()
    : p{std::make_unique<ScriptRuntime>()}
{ }

void ScriptParser::addProvider(const ScriptVariableProvider& provider)
{
    p->addProvider(provider);
}

void ScriptParser::addProvider(const ScriptFunctionProvider& provider)
{
    p->addProvider(provider);
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

bool ScriptParser::canEvaluateAsQuery(const ParsedScript& input)
{
    return ::canEvaluateAsQuery(input);
}

bool ScriptParser::containsQueryExpression(const ParsedScript& input)
{
    return ::containsQueryExpression(input);
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

QString ScriptParser::evaluate(const QString& input, const ScriptContext& context,
                               const ScriptEvaluationOptions& options)
{
    return evaluate(input, Track{}, context, options);
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
    return evaluate(script, track, context);
}

QString ScriptParser::evaluate(const ParsedScript& input, const Track& track, const ScriptContext& context)
{
    return p->withContext(context, [&] { return evaluate(input, track); });
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
    return evaluate(script, tracks, context);
}

QString ScriptParser::evaluate(const ParsedScript& input, const TrackList& tracks, const ScriptContext& context)
{
    return p->withContext(context, [&] { return evaluate(input, tracks); });
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
    return evaluate(script, playlist, context);
}

QString ScriptParser::evaluate(const ParsedScript& input, const Playlist& playlist, const ScriptContext& context)
{
    return p->withContext(context, [&] { return evaluate(input, playlist); });
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
