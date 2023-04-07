/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include <QString>

namespace Fy::Core::Scripting {
enum TokenType : char
{
    TokVar         = '%',
    TokFunc        = '$',
    TokComma       = ',',
    TokQuote       = '"',
    TokLeftParen   = '(',
    TokRightParen  = ')',
    TokLeftSquare  = '[',
    TokRightSquare = ']',
    TokEscape      = '\\',
    TokEos         = '\0',
    TokError,
    TokLiteral,
};

struct Token
{
    TokenType type;
    QStringView value;
    int position;
};

class Scanner
{
public:
    void setup(const QString& input);
    Token scanNext();

private:
    [[nodiscard]] Token makeToken(TokenType type) const;
    Token literal();

    [[nodiscard]] bool isAtEnd() const;
    QChar advance();
    [[nodiscard]] QChar peek() const;

    QStringView m_input;
    const QChar* m_start;
    const QChar* m_current;
};
} // namespace Fy::Core::Scripting
