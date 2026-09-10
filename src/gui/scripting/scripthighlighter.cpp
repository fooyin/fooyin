/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include "scripthighlighter.h"

#include "utils/utils.h"

namespace Fooyin {
ScriptHighlighter::ScriptHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter{parent}
{
    setColours(defaultColours());
}

ScriptHighlightColours ScriptHighlighter::defaultColours()
{
    const bool isDarkMode = Utils::isDarkMode();

    return {
        .variable       = isDarkMode ? QColor{0x61afef} : QColor{0x0969da},
        .function       = isDarkMode ? QColor{0xe5c07b} : QColor{0x8250df},
        .conditional    = isDarkMode ? QColor{0xc678dd} : QColor{0x9a6700},
        .operatorColour = isDarkMode ? QColor{0xabb2bf} : QColor{0x57606a},
        .quotedText     = isDarkMode ? QColor{0x98c379} : QColor{0x1a7f37},
    };
}

void ScriptHighlighter::setColours(const ScriptHighlightColours& colours)
{
    m_varFormat.setForeground(colours.variable);
    m_functionFormat.setForeground(colours.function);
    m_conditionalFormat.setForeground(colours.conditional);
    m_operatorFormat.setForeground(colours.operatorColour);
    m_commentFormat.setForeground(colours.quotedText);
    rehighlight();
}

void ScriptHighlighter::highlightBlock(const QString& text)
{
    if(text.isEmpty()) {
        return;
    }
    m_scanner.setup(text);

    advance();
    while(m_current.type != ScriptScanner::TokEos) {
        expression();
    }
}

void ScriptHighlighter::expression()
{
    advance();
    switch(m_previous.type) {
        case ScriptScanner::TokVar:
            variable();
            break;
        case ScriptScanner::TokFunc:
            function();
            break;
        case ScriptScanner::TokQuote:
            quote();
            break;
        case ScriptScanner::TokLeftSquare:
            conditional();
            break;
        case ScriptScanner::TokEscape:
        case ScriptScanner::TokLeftAngle:
        case ScriptScanner::TokRightAngle:
        case ScriptScanner::TokComma:
        case ScriptScanner::TokLeftParen:
        case ScriptScanner::TokRightParen:
        case ScriptScanner::TokRightSquare:
        case ScriptScanner::TokLiteral:
        case ScriptScanner::TokSlash:
        case ScriptScanner::TokColon:
        case ScriptScanner::TokEquals:
        case ScriptScanner::TokEos:
        case ScriptScanner::TokError:
        case ScriptScanner::TokNot:
        case ScriptScanner::TokAscending:
        case ScriptScanner::TokDescending:
        case ScriptScanner::TokAnd:
        case ScriptScanner::TokOr:
        case ScriptScanner::TokXOr:
        case ScriptScanner::TokMissing:
        case ScriptScanner::TokPresent:
        case ScriptScanner::TokAll:
        case ScriptScanner::TokSort:
        case ScriptScanner::TokBy:
        case ScriptScanner::TokBefore:
        case ScriptScanner::TokAfter:
        case ScriptScanner::TokSince:
        case ScriptScanner::TokDuring:
        case ScriptScanner::TokLast:
        case ScriptScanner::TokSecond:
        case ScriptScanner::TokMinute:
        case ScriptScanner::TokHour:
        case ScriptScanner::TokDay:
        case ScriptScanner::TokWeek:
        case ScriptScanner::TokMonth:
        case ScriptScanner::TokYear:
        case ScriptScanner::TokLimit:
        case ScriptScanner::TokPlus:
        case ScriptScanner::TokMinus:
            break;
    }
}

void ScriptHighlighter::quote()
{
    setTokenFormat(m_commentFormat);
}

void ScriptHighlighter::variable()
{
    setTokenFormat(m_varFormat);

    advance();

    if(m_previous.type == ScriptScanner::TokLeftAngle || m_previous.type == ScriptScanner::TokVar) {
        setTokenFormat(m_operatorFormat);
        advance();
        setTokenFormat(m_varFormat);
        advance();
        setTokenFormat(m_operatorFormat);
    }
    else {
        setTokenFormat(m_varFormat);
    }

    advance();

    setTokenFormat(m_varFormat);
}

void ScriptHighlighter::function()
{
    setTokenFormat(m_functionFormat);
    advance();
    setTokenFormat(m_functionFormat);
    advance();
    setTokenFormat(m_functionFormat);

    if(!currentToken(ScriptScanner::TokRightParen)) {
        functionArgs();
        while(match(ScriptScanner::TokComma)) {
            functionArgs();
        }
    }

    advance();
    setTokenFormat(m_functionFormat);
}

void ScriptHighlighter::functionArgs()
{
    while(!currentToken(ScriptScanner::TokComma) && !currentToken(ScriptScanner::TokRightParen)
          && !currentToken(ScriptScanner::TokEos)) {
        expression();
    }
}

void ScriptHighlighter::conditional()
{
    setTokenFormat(m_conditionalFormat);

    while(!currentToken(ScriptScanner::TokRightSquare) && !currentToken(ScriptScanner::TokEos)) {
        expression();
    }

    advance();

    setTokenFormat(m_conditionalFormat);
}

void ScriptHighlighter::setTokenFormat(const QTextCharFormat& format)
{
    setFormat(m_previous.position, static_cast<int>(m_previous.value.length()), format);
}

void ScriptHighlighter::advance()
{
    m_previous = m_current;
    m_current  = m_scanner.next();
}

bool ScriptHighlighter::currentToken(ScriptScanner::TokenType type) const
{
    return m_current.type == type;
}

bool ScriptHighlighter::match(ScriptScanner::TokenType type)
{
    if(!currentToken(type)) {
        return false;
    }
    advance();
    return true;
}
} // namespace Fooyin

#include "moc_scripthighlighter.cpp"
