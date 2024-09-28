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

#pragma once

#include "fycore_export.h"

#include <QString>

namespace Fooyin {
class FYCORE_EXPORT ScriptScanner
{
public:
    enum TokenType : uint8_t
    {
        TokError       = 0,
        TokEos         = 1,
        TokVar         = 2,
        TokLeftAngle   = 3,
        TokRightAngle  = 4,
        TokFunc        = 5,
        TokComma       = 6,
        TokQuote       = 7,
        TokLeftParen   = 8,
        TokRightParen  = 9,
        TokLeftSquare  = 10,
        TokRightSquare = 11,
        TokSlash       = 12,
        TokColon       = 13,
        TokEquals      = 14,
        TokNot         = 15,
        TokEscape      = 16,
        TokLiteral     = 17,
        TokAnd         = 18,
        TokOr          = 19,
        TokSort        = 20,
        TokAscending   = 21,
        TokDescending  = 22,
        TokBy          = 23,
        TokAll         = 24,
        TokBefore      = 25,
        TokAfter       = 26,
        TokSince       = 27,
        TokDuring      = 28,
        TokLast        = 29,
        TokSecond      = 30,
        TokMinute      = 31,
        TokHour        = 32,
        TokDay         = 33,
        TokWeek        = 34,
        TokMissing     = 35,
        TokPresent     = 36,
    };

    struct Token
    {
        TokenType type{TokError};
        QString value;
        int position{-1};
    };

    ScriptScanner();

    void setup(const QString& input);
    Token next();
    Token peekNext(int delta = 1);

    void setSkipWhitespace(bool enabled);

private:
    Token scanNext();
    [[nodiscard]] Token makeToken(TokenType type) const;
    Token literal();
    Token keyword();
    Token checkKeyword(int start, QAnyStringView rest, TokenType type);

    [[nodiscard]] bool isAtEnd() const;
    QChar advance();
    [[nodiscard]] QChar peek() const;

    QStringView m_input;
    const QChar* m_start;
    const QChar* m_current;

    std::vector<Token> m_tokens;
    Token* m_lastToken;
    int m_currentTokenIndex;
    bool m_skipWhitespace;
};
} // namespace Fooyin
