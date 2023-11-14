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

namespace Fy::Gui::Sandbox {
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
    while(m_current.type != Core::Scripting::TokEos) {
        expression();
    }
}

void ScriptHighlighter::expression()
{
    advance();
    switch(m_previous.type) {
        case(Core::Scripting::TokVar):
            variable();
            break;
        case(Core::Scripting::TokFunc):
            function();
            break;
        case(Core::Scripting::TokQuote):
            quote();
            break;
        case(Core::Scripting::TokLeftSquare):
            conditional();
            break;
        case(Core::Scripting::TokEscape):
        case(Core::Scripting::TokLeftAngle):
        case(Core::Scripting::TokRightAngle):
        case(Core::Scripting::TokComma):
        case(Core::Scripting::TokLeftParen):
        case(Core::Scripting::TokRightParen):
        case(Core::Scripting::TokRightSquare):
        case(Core::Scripting::TokLiteral):
        case(Core::Scripting::TokEos):
        case(Core::Scripting::TokError):
            break;
    }
}

void ScriptHighlighter::quote() { }

void ScriptHighlighter::variable()
{
    setTokenFormat(m_varFormat);

    advance();

    if(m_previous.type == Core::Scripting::TokLeftAngle) {
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
    if(!currentToken(Core::Scripting::TokRightParen)) {
        functionArgs();
        while(match(Core::Scripting::TokComma)) {
            functionArgs();
        }
    }
    advance();
}

void ScriptHighlighter::functionArgs()
{
    while(!currentToken(Core::Scripting::TokComma) && !currentToken(Core::Scripting::TokRightParen)
          && !currentToken(Core::Scripting::TokEos)) {
        expression();
    }
}

void ScriptHighlighter::conditional()
{
    setTokenFormat(m_varFormat);

    while(!currentToken(Core::Scripting::TokRightSquare) && !currentToken(Core::Scripting::TokEos)) {
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

bool ScriptHighlighter::currentToken(Core::Scripting::TokenType type) const
{
    return m_current.type == type;
}

bool ScriptHighlighter::match(Core::Scripting::TokenType type)
{
    if(!currentToken(type)) {
        return false;
    }
    advance();
    return true;
}
} // namespace Fy::Gui::Sandbox

#include "moc_scripthighlighter.cpp"
