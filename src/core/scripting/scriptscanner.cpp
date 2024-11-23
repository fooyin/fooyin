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

using namespace Qt::StringLiterals;

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
        case(u'*'):
        case(u'+'):
        case(u'-'):
        case(u'\\'):
        case(u'\0'):
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
        case(u'W'):
        case(u'X'):
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
        if(m_lastToken && m_lastToken->type == TokLiteral && token.type == TokLiteral) {
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
        case(u'*'):
            return makeToken(TokAll);
        case(u'+'):
            return makeToken(TokPlus);
        case(u'-'):
            return makeToken(TokMinus);
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
    token.value    = QStringView{m_start, currentLength()}.toString();
    token.position = static_cast<int>(m_start - m_input.cbegin());
    return token;
}

ScriptScanner::Token ScriptScanner::literal()
{
    while(isLiteral(peek()) && !isStartOfKeyword(peek()) && !isAtEnd()) {
        advance();
    }

    return makeToken(TokLiteral);
}

ScriptScanner::Token ScriptScanner::keyword()
{
    while(isKeyword(peek()) && !isAtEnd()) {
        advance();
    }

    const auto isPlural = [this](int pos) {
        return currentLength() > pos && m_start[pos].unicode() == u'S';
    };

    switch(m_start->unicode()) {
        case(u'A'): {
            if(currentLength() > 1) {
                switch(m_start[1].unicode()) {
                    case(u'F'):
                        return checkKeyword(2, "TER"_L1, TokAfter);
                    case(u'N'):
                        return checkKeyword(2, "D"_L1, TokAnd);
                    case(u'L'):
                        return checkKeyword(2, "L"_L1, TokAll);
                    case(u'S'):
                        return checkKeyword(2, "CENDING"_L1, TokAscending);
                    default:
                        break;
                }
            }
            break;
        }
        case(u'B'):
            if(currentLength() > 1) {
                switch(m_start[1].unicode()) {
                    case(u'E'):
                        return checkKeyword(2, "FORE"_L1, TokBefore);
                    default:
                        break;
                }
            }
            return checkKeyword(1, "Y"_L1, TokBy);
        case(u'D'):
            if(currentLength() > 1) {
                switch(m_start[1].unicode()) {
                    case(u'A'):
                        return checkKeyword(2, isPlural(3) ? "YS"_L1 : "Y"_L1, TokDay);
                    case(u'E'):
                        return checkKeyword(2, "SCENDING"_L1, TokDescending);
                    case(u'U'):
                        return checkKeyword(2, "RING"_L1, TokDuring);
                    default:
                        break;
                }
            }
            break;
        case(u'E'):
            return checkKeyword(1, "QUAL"_L1, TokEquals);
        case(u'G'):
            return checkKeyword(1, "REATER"_L1, TokRightAngle);
        case(u'H'):
            if(currentLength() > 1) {
                switch(m_start[1].unicode()) {
                    case(u'A'):
                        return checkKeyword(2, "S"_L1, TokColon);
                    case(u'O'):
                        return checkKeyword(2, isPlural(4) ? "URS"_L1 : "UR"_L1, TokHour);
                    default:
                        break;
                }
            }
            break;
        case(u'I'):
            return checkKeyword(1, "S"_L1, TokEquals);
        case(u'L'):
            if(currentLength() > 1) {
                switch(m_start[1].unicode()) {
                    case(u'A'):
                        return checkKeyword(2, "ST"_L1, TokLast);
                    case(u'E'):
                        return checkKeyword(2, "SS"_L1, TokLeftAngle);
                    case(u'I'):
                        return checkKeyword(2, "MIT"_L1, TokLimit);
                    default:
                        break;
                }
            }
            break;
        case(u'M'):
            if(currentLength() > 1) {
                switch(m_start[1].unicode()) {
                    case(u'I'):
                        if(currentLength() > 2) {
                            switch(m_start[2].unicode()) {
                                case(u'N'):
                                    return checkKeyword(3, isPlural(6) ? "UTES"_L1 : "UTE"_L1, TokMinute);
                                case(u'S'):
                                    return checkKeyword(3, "SING"_L1, TokMissing);
                                default:
                                    break;
                            }
                        }
                    default:
                        break;
                }
            }
            break;
        case(u'N'):
            return checkKeyword(1, "OT"_L1, TokNot);
        case(u'O'):
            return checkKeyword(1, "R"_L1, TokOr);
        case(u'P'):
            return checkKeyword(1, "RESENT"_L1, TokPresent);
        case(u'S'):
            if(currentLength() > 1) {
                switch(m_start[1].unicode()) {
                    case(u'E'):
                        return checkKeyword(2, isPlural(6) ? "CONDS"_L1 : "COND"_L1, TokSecond);
                    case(u'I'):
                        return checkKeyword(2, "NCE"_L1, TokSince);
                    case(u'O'):
                        return checkKeyword(2, "RT"_L1, TokSort);
                    default:
                        break;
                }
            }
            break;
        case(u'W'):
            return checkKeyword(1, isPlural(4) ? "EEKS"_L1 : "EEK"_L1, TokWeek);
        case(u'X'):
            return checkKeyword(1, "OR"_L1, TokXOr);
        default:
            break;
    }

    return literal();
}

ScriptScanner::Token ScriptScanner::checkKeyword(int start, QAnyStringView rest, TokenType type)
{
    if(currentLength() == start + rest.length() && std::memcmp(m_start + start, rest.data(), rest.length()) == 0) {
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

int ScriptScanner::currentLength() const
{
    return static_cast<int>(m_current - m_start);
}
} // namespace Fooyin
