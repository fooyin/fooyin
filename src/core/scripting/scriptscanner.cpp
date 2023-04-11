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
bool isLiteral(QChar ch)
{
    return (ch != TokComma && ch != TokQuote && ch != TokLeftParen && ch != TokRightParen && ch != TokLeftSquare
            && ch != TokFunc && ch != TokVar && ch != TokLeftAngle && ch != TokRightAngle && ch != TokEscape);
}

void Scanner::setup(const QString& input)
{
    m_input   = input;
    m_start   = m_input.cbegin();
    m_current = m_start;
}

Token Scanner::scanNext()
{
    m_start = m_current;

    if(isAtEnd()) {
        return makeToken(TokEos);
    }

    const QChar c = advance();

    switch(c.cell()) {
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

Token Scanner::makeToken(TokenType type) const
{
    Token token;
    token.type     = type;
    token.value    = {m_start, m_current - m_start};
    token.position = static_cast<int>(m_start - m_input.cbegin());
    return token;
}

Token Scanner::literal()
{
    while(isLiteral(peek()) && !isAtEnd()) {
        advance();
    }

    return makeToken(TokLiteral);
}

bool Scanner::isAtEnd() const
{
    return *m_current == '\0';
}

QChar Scanner::advance()
{
    std::advance(m_current, 1);
    return *std::prev(m_current);
}

QChar Scanner::peek() const
{
    return *m_current;
}
} // namespace Fy::Core::Scripting
