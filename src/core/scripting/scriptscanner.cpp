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

#include <core/scripting/scriptscanner.h>

namespace Fooyin {
bool isLiteral(const QChar ch)
{
    switch(ch.unicode()) {
        case(ScriptScanner::TokComma):
        case(ScriptScanner::TokQuote):
        case(ScriptScanner::TokLeftParen):
        case(ScriptScanner::TokRightParen):
        case(ScriptScanner::TokLeftSquare):
        case(ScriptScanner::TokRightSquare):
        case(ScriptScanner::TokFunc):
        case(ScriptScanner::TokVar):
        case(ScriptScanner::TokLeftAngle):
        case(ScriptScanner::TokRightAngle):
        case(ScriptScanner::TokEscape):
            return false;
        default:
            return true;
    }
}

void ScriptScanner::setup(const QString& input)
{
    m_input   = input;
    m_start   = m_input.cbegin();
    m_current = m_start;
}

ScriptScanner::Token ScriptScanner::scanNext()
{
    m_start = m_current;

    if(isAtEnd()) {
        return makeToken(TokEos);
    }

    const QChar c = advance();

    switch(c.unicode()) {
        case('('):
            return makeToken(TokLeftParen);
        case(')'):
            return makeToken(TokRightParen);
        case('['):
            return makeToken(TokLeftSquare);
        case(']'):
            return makeToken(TokRightSquare);
        case('$'):
            return makeToken(TokFunc);
        case('%'):
            return makeToken(TokVar);
        case('<'):
            return makeToken(TokLeftAngle);
        case('>'):
            return makeToken(TokRightAngle);
        case(','):
            return makeToken(TokComma);
        case('"'):
            return makeToken(TokQuote);
        case('\\'):
            return makeToken(TokEscape);
        default:
            return literal();
    }
}

ScriptScanner::Token ScriptScanner::makeToken(TokenType type) const
{
    Token token;
    token.type     = type;
    token.value    = {m_start, m_current - m_start};
    token.position = static_cast<int>(m_start - m_input.cbegin());
    return token;
}

ScriptScanner::Token ScriptScanner::literal()
{
    while(isLiteral(peek()) && !isAtEnd()) {
        advance();
    }

    return makeToken(TokLiteral);
}

bool ScriptScanner::isAtEnd() const
{
    return *m_current == QChar::fromLatin1('\0');
}

QChar ScriptScanner::advance()
{
    std::advance(m_current, 1);
    return *std::prev(m_current);
}

QChar ScriptScanner::peek() const
{
    return *m_current;
}
} // namespace Fooyin
