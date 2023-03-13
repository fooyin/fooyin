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

#include "scriptscanner.h"

namespace Fy::Core::Scripting {
bool isAtEnd(const State& s)
{
    return *s.current == '\0';
}

QChar advance(State& s)
{
    std::advance(s.current, 1);
    return *std::prev(s.current);
}

QChar peek(const State& s)
{
    return *s.current;
}

QChar peekNext(const State& s)
{
    if(isAtEnd(s)) {
        return '\0';
    }
    return *std::next(s.current);
}

void consume(State& s)
{
    s.start = s.current;
}

Expression parseLiteral(State& s)
{
    while(peek(s) != '%' && peek(s) != '$' && !isAtEnd(s)) {
        advance(s);
    }
    Expression expr{Literal};
    expr.value = LiteralValue{QString{s.start, s.current - s.start}};
    return expr;
}

Expression parseVariable(State& s)
{
    consume(s);
    while(peek(s) != '%' && !isAtEnd(s)) {
        advance(s);
    }
    Expression expr{Variable};
    expr.value = VarValue{QString{s.start, s.current - s.start}};

    if(!isAtEnd(s)) {
        advance(s);
    }
    return expr;
}

void Scanner::setup(const QString& input)
{
    m_state.start   = input.constData();
    m_state.current = m_state.start;
}

Expression Scanner::scanNext()
{
    return scanNext(m_state);
}

Expression Scanner::scanNext(State& s)
{
    s.start = s.current;

    if(isAtEnd(s)) {
        return {};
    }

    const QChar c = advance(s);

    switch(c.cell()) {
        case '$': {
            return parseFunction(s);
        }
        case '%': {
            return parseVariable(s);
        }
        default: {
            return parseLiteral(s);
        }
    };
    return {};
}

Expression Scanner::parseFunction(State& s)
{
    consume(s);
    while(peek(s) != '(' && !isAtEnd(s)) {
        advance(s);
    }
    Expression expr{Function};
    FuncValue val{QString{s.start, s.current - s.start}, {}};

    advance(s);
    consume(s);

    int nestedCount{0};

    while(peek(s) != ')' || nestedCount > 0) {
        if(isAtEnd(s)) {
            break;
        }
        if(nestedCount == 0 && peek(s) == ',') {
            val.args.emplace_back(parseFunctionArg(s));
            advance(s);
            consume(s);
            continue;
        }
        if(peek(s) == '$') {
            ++nestedCount;
        }
        else if(peek(s) == ')') {
            --nestedCount;
        }
        advance(s);
    }

    val.args.emplace_back(parseFunctionArg(s));
    expr.value = val;

    if(!isAtEnd(s)) {
        advance(s);
    }
    return expr;
}

Expression Scanner::parseFunctionArg(State& s)
{
    QString const arg{s.start, s.current - s.start};
    State argState;
    argState.current = arg.constData();
    return scanNext(argState);
}
} // namespace Fy::Core::Scripting
