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

namespace {
bool isLiteral(const QChar ch, bool includeKeywords = true)
{
    switch(ch.unicode()) {
        case(u'%'):
        case(u'<'):
        case(u'>'):
        case(u'$'):
        case(u','):
        case(u'"'):
        case(u'('):
        case(u')'):
        case(u'['):
        case(u']'):
        case(u'/'):
        case(u':'):
        case(u'='):
        case(u'!'):
        case(u'\\'):
        case(u'\0'):
            return false;
        case(u'A'):
        case(u'O'):
        case(u'S'):
            return !includeKeywords;
        default:
            return true;
    }
}

} // namespace

namespace Fooyin {
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

    const QChar c = advance();
    switch(c.unicode()) {
        case(u'%'):
            return makeToken(TokVar);
        case(u'<'):
            return makeToken(TokLeftAngle);
        case(u'>'):
            return makeToken(TokRightAngle);
        case(u'$'):
            return makeToken(TokFunc);
        case(u','):
            return makeToken(TokComma);
        case(u'"'):
            return makeToken(TokQuote);
        case(u'('):
            return makeToken(TokLeftParen);
        case(u')'):
            return makeToken(TokRightParen);
        case(u'['):
            return makeToken(TokLeftSquare);
        case(u']'):
            return makeToken(TokRightSquare);
        case(u'/'):
            return makeToken(TokSlash);
        case(u':'):
            return makeToken(TokColon);
        case(u'='):
            return makeToken(TokEquals);
        case(u'!'):
            return makeToken(TokExclamation);
        case(u'\\'):
            return makeToken(TokEscape);
        case(u'\0'):
            return makeToken(TokEos);
        case(u'A'): {
            if(matchKeyword(QStringLiteral("ND "))) {
                return makeToken(TokAnd);
            }
            if(matchKeyword(QStringLiteral("LL"))) {
                return makeToken(TokAll);
            }
            return literal();
        }
        case(u'O'): {
            if(matchKeyword(QStringLiteral("R "))) {
                return makeToken(TokOr);
            }
            return literal();
        }
        case(u'S'):
            if(matchKeyword(QStringLiteral("ORT "))) {
                if(matchKeyword(QStringLiteral("BY ")) || matchKeyword(QStringLiteral("ASCENDING BY "))) {
                    return makeToken(TokSortAscending);
                }
                if(matchKeyword(QStringLiteral("DESCENDING BY "))) {
                    return makeToken(TokSortDescending);
                }
            }
            return literal();
        default:
            break;
    }

    if(isLiteral(c)) {
        return literal();
    }

    return makeToken(TokError);
}

ScriptScanner::Token ScriptScanner::makeToken(TokenType type) const
{
    Token token;
    token.type     = type;
    token.value    = QStringView{m_start, m_current - m_start}.toString();
    token.position = static_cast<int>(m_start - m_input.cbegin());
    return token;
}

ScriptScanner::Token ScriptScanner::literal()
{
    while(currentIsLiteral() && !isAtEnd()) {
        advance();
    }

    return makeToken(TokLiteral);
}

bool ScriptScanner::currentIsLiteral()
{
    const QChar c = *m_current;
    if(!isLiteral(c, false)) {
        return false;
    }

    switch(c.unicode()) {
        case(u'A'):
            return !checkKeyword(QStringLiteral("AND ")) && !checkKeyword(QStringLiteral("ALL"));
        case(u'O'):
            return !checkKeyword(QStringLiteral("OR "));
        case(u'S'):
            if(!checkKeyword(QStringLiteral("SORT "))) {
                return true;
            }
            return !checkKeyword(QStringLiteral("SORT BY ")) && !checkKeyword(QStringLiteral("SORT ASCENDING BY "))
                && !checkKeyword(QStringLiteral("SORT DESCENDING BY "));
        default:
            break;
    }

    return true;
}

bool ScriptScanner::isAtEnd() const
{
    return *m_current == u'\0';
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

bool ScriptScanner::checkKeyword(const QString& keyword)
{
    return matchKeyword(keyword, false);
}

bool ScriptScanner::matchKeyword(const QString& keyword, bool advance)
{
    const QChar* it = m_current;
    for(const QChar& ch : keyword) {
        if(*it == u'\0' || *it != ch) {
            return false;
        }
        ++it;
    }

    if(advance) {
        std::advance(m_current, keyword.length());
    }
    return true;
}
} // namespace Fooyin
