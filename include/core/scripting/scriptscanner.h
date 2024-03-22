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
    enum TokenType : char
    {
        TokVar         = '%',
        TokLeftAngle   = '<',
        TokRightAngle  = '>',
        TokFunc        = '$',
        TokComma       = ',',
        TokQuote       = '"',
        TokLeftParen   = '(',
        TokRightParen  = ')',
        TokLeftSquare  = '[',
        TokRightSquare = ']',
        TokSlash       = '/',
        TokColon       = ':',
        TokEquals      = '=',
        TokEscape      = '\\',
        TokEos         = '\0',
        TokError,
        TokLiteral,
    };

    struct Token
    {
        TokenType type{TokError};
        QStringView value;
        int position{0};
    };

    void setup(const QString& input);
    Token next();
    Token peekNext(int delta = 1);

private:
    Token scanNext();
    [[nodiscard]] Token makeToken(TokenType type) const;
    Token literal();

    [[nodiscard]] bool isAtEnd() const;
    QChar advance();
    [[nodiscard]] QChar peek() const;

    QStringView m_input;
    const QChar* m_start;
    const QChar* m_current;

    std::vector<Token> m_tokens;
    int m_currentTokenIndex;
};
} // namespace Fooyin
