/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <gui/scripting/scriptformatter.h>

#include <core/scripting/scriptparser.h>
#include <core/scripting/scriptscanner.h>
#include <gui/scripting/scriptformatterregistry.h>

#include <QApplication>
#include <QPalette>

#include <stack>

namespace Fooyin {
class ScriptFormatterPrivate
{
public:
    void advance();
    void consume(ScriptScanner::TokenType type);
    void consume(ScriptScanner::TokenType type, const QString& message);

    void errorAtCurrent(const QString& message);
    void error(const QString& message);
    void errorAt(const ScriptScanner::Token& token, const QString& message);

    void expression();
    void formatBlock();
    void processFormat(const QString& func, const QString& option);
    void addBlock();
    void closeBlock();
    void resetFormat();

    ScriptScanner m_scanner;
    ScriptFormatterRegistry m_registry;
    QFont m_font;

    ScriptScanner::Token m_current;
    ScriptScanner::Token m_previous;

    RichTextBlock m_currentBlock;
    std::stack<RichTextBlock> m_blockStack;
    std::vector<RichTextBlock> m_blockGroup;

    ErrorList m_errors;
    RichText m_formatResult;
};

void ScriptFormatterPrivate::advance()
{
    m_previous = m_current;

    m_current = m_scanner.next();
    if(m_current.type == ScriptScanner::TokError) {
        errorAtCurrent(m_current.value);
    }
}

void ScriptFormatterPrivate::consume(ScriptScanner::TokenType type)
{
    if(m_current.type == type) {
        advance();
    }
}

void ScriptFormatterPrivate::consume(ScriptScanner::TokenType type, const QString& message)
{
    if(m_current.type == type) {
        advance();
        return;
    }
    errorAtCurrent(message);
}

void ScriptFormatterPrivate::errorAtCurrent(const QString& message)
{
    errorAt(m_current, message);
}

void ScriptFormatterPrivate::error(const QString& message)
{
    errorAt(m_previous, message);
}

void ScriptFormatterPrivate::errorAt(const ScriptScanner::Token& token, const QString& message)
{
    QString errorMsg = QStringLiteral("[%1] Error").arg(token.position);

    if(token.type == ScriptScanner::TokEos) {
        errorMsg += QStringLiteral(" at end of string");
    }
    else {
        errorMsg += QStringLiteral(": '") + token.value + QStringLiteral("'");
    }

    errorMsg += QStringLiteral(" (%1)").arg(message);

    ScriptError currentError;
    currentError.value    = token.value;
    currentError.position = token.position;
    currentError.message  = errorMsg;

    m_errors.emplace_back(currentError);
}

void ScriptFormatterPrivate::expression()
{
    advance();
    switch(m_previous.type) {
        case(ScriptScanner::TokLeftAngle):
            formatBlock();
            break;
        case(ScriptScanner::TokVar):
        case(ScriptScanner::TokFunc):
        case(ScriptScanner::TokQuote):
        case(ScriptScanner::TokLeftSquare):
        case(ScriptScanner::TokRightAngle):
        case(ScriptScanner::TokComma):
        case(ScriptScanner::TokLeftParen):
        case(ScriptScanner::TokRightParen):
        case(ScriptScanner::TokRightSquare):
        case(ScriptScanner::TokSlash):
        case(ScriptScanner::TokColon):
        case(ScriptScanner::TokEquals):
        case(ScriptScanner::TokNot):
        case(ScriptScanner::TokLiteral):
        case(ScriptScanner::TokAnd):
        case(ScriptScanner::TokOr):
        case(ScriptScanner::TokXOr):
        case(ScriptScanner::TokMissing):
        case(ScriptScanner::TokPresent):
        case(ScriptScanner::TokAll):
        case(ScriptScanner::TokSort):
        case(ScriptScanner::TokBy):
        case(ScriptScanner::TokBefore):
        case(ScriptScanner::TokAfter):
        case(ScriptScanner::TokSince):
        case(ScriptScanner::TokDuring):
        case(ScriptScanner::TokLast):
        case(ScriptScanner::TokSecond):
        case(ScriptScanner::TokMinute):
        case(ScriptScanner::TokHour):
        case(ScriptScanner::TokDay):
        case(ScriptScanner::TokWeek):
        case(ScriptScanner::TokAscending):
        case(ScriptScanner::TokDescending):
            m_currentBlock.text += m_previous.value;
            break;
        case(ScriptScanner::TokEscape):
            advance();
            m_currentBlock.text += m_previous.value;
            break;
        case(ScriptScanner::TokEos):
        case(ScriptScanner::TokError):
            break;
    }
}

void ScriptFormatterPrivate::formatBlock()
{
    QString func;
    QString option;

    bool formatOption{false};
    while(m_current.type != ScriptScanner::TokRightAngle && m_current.type != ScriptScanner::TokEos) {
        advance();
        if(m_previous.type == ScriptScanner::TokEquals) {
            formatOption = true;
            advance();
        }

        if(formatOption) {
            option.append(m_previous.value);
        }
        else {
            func.append(m_previous.value);
        }
    }

    processFormat(func, option);
    closeBlock();
}

void ScriptFormatterPrivate::processFormat(const QString& func, const QString& option)
{
    if(m_registry.isFormatFunc(func)) {
        addBlock();
        m_registry.format(m_currentBlock.format, func, option);
    }
    else {
        error(QStringLiteral("Format option not found"));
    }

    consume(ScriptScanner::TokRightAngle, QStringLiteral("Expected '>' after expression"));

    while(m_current.type != ScriptScanner::TokEos
          && (m_current.type != ScriptScanner::TokLeftAngle
              || (m_scanner.peekNext().type != ScriptScanner::TokSlash && m_scanner.peekNext(2).value != func))) {
        expression();
    }

    consume(ScriptScanner::TokLeftAngle);
    consume(ScriptScanner::TokSlash);

    QString closeOption;

    while(m_current.type != ScriptScanner::TokRightAngle && m_current.type != ScriptScanner::TokEos) {
        advance();
        closeOption.append(m_previous.value);
    }

    consume(ScriptScanner::TokRightAngle);
}

void ScriptFormatterPrivate::addBlock()
{
    if(!m_currentBlock.text.isEmpty()) {
        if(m_blockGroup.empty()) {
            m_formatResult.emplace_back(m_currentBlock);
        }
        else {
            m_blockStack.emplace(m_currentBlock);
        }
        m_currentBlock.text.clear();
    }
}

void ScriptFormatterPrivate::closeBlock()
{
    if(!m_currentBlock.text.isEmpty()) {
        if(m_blockStack.empty()) {
            m_formatResult.emplace_back(m_currentBlock);
        }
        else {
            m_blockGroup.emplace_back(m_currentBlock);
        }
    }

    if(!m_blockStack.empty()) {
        m_currentBlock = m_blockStack.top();
        m_blockStack.pop();
    }
    else {
        std::ranges::reverse(m_blockGroup);
        m_formatResult.insert(m_formatResult.end(), m_blockGroup.cbegin(), m_blockGroup.cend());
        m_blockGroup.clear();
        resetFormat();
    }
}

void ScriptFormatterPrivate::resetFormat()
{
    m_currentBlock               = {};
    m_currentBlock.format.font   = m_font;
    m_currentBlock.format.colour = QApplication::palette().text().color();
}

ScriptFormatter::ScriptFormatter()
    : p{std::make_unique<ScriptFormatterPrivate>()}
{ }

ScriptFormatter::~ScriptFormatter() = default;

RichText ScriptFormatter::evaluate(const QString& input)
{
    if(input.isEmpty()) {
        return {};
    }

    p->resetFormat();
    p->m_formatResult.clear();
    p->m_scanner.setup(input);

    p->advance();
    while(p->m_current.type != ScriptScanner::TokEos) {
        p->expression();
    }

    p->consume(ScriptScanner::TokEos, QStringLiteral("Expected end of expression"));

    if(!p->m_currentBlock.text.isEmpty()) {
        p->m_formatResult.emplace_back(p->m_currentBlock);
    }

    return p->m_formatResult;
}

void ScriptFormatter::setBaseFont(const QFont& font)
{
    p->m_font = font;
}
} // namespace Fooyin
