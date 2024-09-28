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
bool isLiteral(QChar ch)
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
        case(u'A'):
        case(u'B'):
        case(u'D'):
        case(u'E'):
        case(u'G'):
        case(u'H'):
        case(u'I'):
        case(u'L'):
        case(u'M'):
        case(u'N'):
        case(u'O'):
        case(u'P'):
        case(u'S'):
            return false;
        default:
            return true;
    }
}

bool isStartOfKeyword(QChar ch)
{
    switch(ch.unicode()) {
        case(u'A'):
        case(u'B'):
        case(u'D'):
        case(u'E'):
        case(u'G'):
        case(u'H'):
        case(u'I'):
        case(u'L'):
        case(u'M'):
        case(u'N'):
        case(u'O'):
        case(u'P'):
        case(u'S'):
            return true;
        default:
            return false;
    }
}

bool isWhitespace(QChar ch)
{
    switch(ch.unicode()) {
        case(u' '):
        case(u'\r'):
        case(u'\t'):
            return true;
        default:
            return false;
    }
}

bool isKeyword(QChar ch)
{
    return ch.isUpper() && (isLiteral(ch) || isStartOfKeyword(ch)) && !isWhitespace(ch);
}
} // namespace

namespace Fooyin {
ScriptScanner::ScriptScanner()
    : m_lastToken{nullptr}
    , m_skipWhitespace{false}
{ }

void ScriptScanner::setup(const QString& input)
{
    m_input   = input;
    m_start   = m_input.cbegin();
    m_current = m_start;

    m_tokens.clear();
    m_currentTokenIndex = 0;
    m_lastToken         = nullptr;

    while(!isAtEnd()) {
        const Token token = scanNext();
        if(m_lastToken && m_lastToken->type == token.type) {
            m_lastToken->value += token.value;
        }
        else {
            m_lastToken = &m_tokens.emplace_back(token);
        }
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

void ScriptScanner::setSkipWhitespace(bool enabled)
{
    m_skipWhitespace = enabled;
}

ScriptScanner::Token ScriptScanner::scanNext()
{
    m_start = m_current;

    QChar c = advance();

    if(m_skipWhitespace) {
        while(isWhitespace(c)) {
            m_start = m_current;
            c       = advance();
        }
    }

    if(isStartOfKeyword(c)) {
        return keyword();
    }

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
            return makeToken(TokNot);
        case(u'\\'):
            return makeToken(TokEscape);
        case(u'\0'):
            return makeToken(TokEos);
        default:
            break;
    }

    return literal();
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
    while(isLiteral(peek()) && !isAtEnd()) {
        advance();
    }

    return makeToken(TokLiteral);
}

ScriptScanner::Token ScriptScanner::keyword()
{
    while(isKeyword(peek()) && !isAtEnd()) {
        advance();
    }

    switch(m_start->unicode()) {
        case(u'A'): {
            if(m_current - m_start > 1) {
                switch(m_start[1].unicode()) {
                    case(u'N'):
                        return checkKeyword(2, u"D", TokAnd);
                    case(u'L'):
                        return checkKeyword(2, u"L", TokAll);
                    case(u'F'):
                        return checkKeyword(2, u"TER", TokAfter);
                    case(u'S'):
                        return checkKeyword(2, u"CENDING", TokAscending);
                    default:
                        break;
                }
            }
            break;
        }
        case(u'B'):
            if(m_current - m_start > 1) {
                switch(m_start[1].unicode()) {
                    case(u'E'):
                        return checkKeyword(2, u"FORE", TokBefore);
                    default:
                        break;
                }
            }
            return checkKeyword(1, u"Y", TokBy);
        case(u'D'):
            if(m_current - m_start > 1) {
                switch(m_start[1].unicode()) {
                    case(u'E'):
                        return checkKeyword(2, u"SCENDING", TokDescending);
                    case(u'U'):
                        return checkKeyword(2, u"RING", TokDuring);
                    default:
                        break;
                }
            }
            break;
        case(u'E'):
            return checkKeyword(1, u"QUAL", TokEquals);
        case(u'G'):
            return checkKeyword(1, u"REATER", TokRightAngle);
        case(u'H'):
            return checkKeyword(1, u"AS", TokColon);
        case(u'I'):
            return checkKeyword(1, u"S", TokEquals);
        case(u'L'):
            if(m_current - m_start > 1) {
                switch(m_start[1].unicode()) {
                    case(u'A'):
                        return checkKeyword(2, u"ST", TokLast);
                    case(u'E'):
                        return checkKeyword(2, u"SS", TokLeftAngle);
                    default:
                        break;
                }
            }
            break;
        case(u'M'):
            return checkKeyword(1, u"ISSING", TokMissing);
        case(u'N'):
            return checkKeyword(1, u"OT", TokNot);
        case(u'O'):
            return checkKeyword(1, u"R", TokOr);
        case(u'P'):
            return checkKeyword(1, u"RESENT", TokPresent);
        case(u'S'):
            if(m_current - m_start > 1) {
                switch(m_start[1].unicode()) {
                    case(u'I'):
                        return checkKeyword(2, u"NCE", TokSince);
                    case(u'O'):
                        return checkKeyword(2, u"RT", TokSort);
                    default:
                        break;
                }
            }
            break;
        default:
            break;
    }

    return literal();
}

ScriptScanner::Token ScriptScanner::checkKeyword(int start, QAnyStringView rest, TokenType type)
{
    if(m_current - m_start == start + rest.length() && std::memcmp(m_start + start, rest.data(), rest.length()) == 0) {
        return makeToken(type);
    }
    return literal();
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
} // namespace Fooyin
