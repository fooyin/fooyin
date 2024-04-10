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
        case(ScriptScanner::TokSlash):
        case(ScriptScanner::TokColon):
        case(ScriptScanner::TokEquals):
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

    m_tokens.clear();
    m_currentTokenIndex = 0;

    while(!isAtEnd()) {
        m_tokens.emplace_back(scanNext());
    }
}

ScriptScanner::Token ScriptScanner::next()
{
    if(m_currentTokenIndex < 0 || std::cmp_greater_equal(m_currentTokenIndex, m_tokens.size())) {
        return makeToken(TokEos);
    }

    return m_tokens.at(m_currentTokenIndex++);
}

ScriptScanner::Token ScriptScanner::peekNext(int delta)
{
    const int next = m_currentTokenIndex + delta - 1;

    if(next < 0 || std::cmp_greater_equal(next, m_tokens.size())) {
        return makeToken(TokEos);
    }

    return m_tokens.at(next);
}

ScriptScanner::Token ScriptScanner::scanNext()
{
    m_start = m_current;

    if(isAtEnd()) {
        return makeToken(TokEos);
    }

    const QChar c = advance();

    if(isLiteral(c)) {
        return literal();
    }

    return makeToken(static_cast<TokenType>(c.unicode()));
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
