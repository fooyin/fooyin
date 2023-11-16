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

#include "scripthighlighter.h"

namespace Fooyin {
ScriptHighlighter::ScriptHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter{parent}
{
    m_varFormat.setForeground(Qt::cyan);
    m_keywordFormat.setForeground(Qt::blue);
    m_errorFormat.setBackground(Qt::red);
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
        case(ScriptScanner::TokVar):
            variable();
            break;
        case(ScriptScanner::TokFunc):
            function();
            break;
        case(ScriptScanner::TokQuote):
            quote();
            break;
        case(ScriptScanner::TokLeftSquare):
            conditional();
            break;
        case(ScriptScanner::TokEscape):
        case(ScriptScanner::TokLeftAngle):
        case(ScriptScanner::TokRightAngle):
        case(ScriptScanner::TokComma):
        case(ScriptScanner::TokLeftParen):
        case(ScriptScanner::TokRightParen):
        case(ScriptScanner::TokRightSquare):
        case(ScriptScanner::TokLiteral):
        case(ScriptScanner::TokEos):
        case(ScriptScanner::TokError):
            break;
    }
}

void ScriptHighlighter::quote() { }

void ScriptHighlighter::variable()
{
    setTokenFormat(m_varFormat);

    advance();

    if(m_previous.type == ScriptScanner::TokLeftAngle) {
        setTokenFormat(m_varFormat);
        advance();
        advance();
        setTokenFormat(m_varFormat);
    }

    advance();

    setTokenFormat(m_varFormat);
}

void ScriptHighlighter::function()
{
    setTokenFormat(m_varFormat);
    advance();
    setTokenFormat(m_keywordFormat);
    advance();
    if(!currentToken(ScriptScanner::TokRightParen)) {
        functionArgs();
        while(match(ScriptScanner::TokComma)) {
            functionArgs();
        }
    }
    advance();
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
    setTokenFormat(m_varFormat);

    while(!currentToken(ScriptScanner::TokRightSquare) && !currentToken(ScriptScanner::TokEos)) {
        expression();
    }

    advance();

    setTokenFormat(m_varFormat);
}

void ScriptHighlighter::setTokenFormat(const QTextCharFormat& format)
{
    setFormat(m_previous.position, static_cast<int>(m_previous.value.length()), format);
}

void ScriptHighlighter::advance()
{
    m_previous = m_current;
    m_current  = m_scanner.scanNext();
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
