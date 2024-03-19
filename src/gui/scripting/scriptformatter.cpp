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
struct ScriptFormatter::Private
{
    ScriptScanner scanner;
    ScriptFormatterRegistry registry;

    ScriptScanner::Token current;
    ScriptScanner::Token previous;

    ErrorList errors;

    FormattedTextBlock currentFormat;
    std::stack<FormattedTextBlock> formatStack;
    std::vector<FormattedTextBlock> nestedFormats;

    FormattedText formatResult;

    void advance()
    {
        previous = current;

        current = scanner.next();
        if(current.type == ScriptScanner::TokError) {
            errorAtCurrent(current.value.toString());
        }
    }

    void consume(ScriptScanner::TokenType type, const QString& message)
    {
        if(current.type == type) {
            advance();
            return;
        }
        errorAtCurrent(message);
    }

    void errorAtCurrent(const QString& message)
    {
        errorAt(current, message);
    }

    void error(const QString& message)
    {
        errorAt(previous, message);
    }

    void errorAt(const ScriptScanner::Token& token, const QString& message)
    {
        QString errorMsg = QStringLiteral("[%1] Error").arg(token.position);

        if(token.type == ScriptScanner::TokEos) {
            errorMsg += QStringLiteral(" at end of string");
        }
        else {
            errorMsg += QStringLiteral(": '") + token.value.toString() + QStringLiteral("'");
        }

        errorMsg += QString::fromLatin1(" (%1)").arg(message);

        ScriptError currentError;
        currentError.value    = token.value.toString();
        currentError.position = token.position;
        currentError.message  = errorMsg;

        errors.emplace_back(currentError);
    }

    void expression()
    {
        advance();
        switch(previous.type) {
            case(ScriptScanner::TokLeftAngle):
                formatBlock();
                break;
            case(ScriptScanner::TokVar):
            case(ScriptScanner::TokFunc):
            case(ScriptScanner::TokQuote):
            case(ScriptScanner::TokLeftSquare):
            case(ScriptScanner::TokEscape):
            case(ScriptScanner::TokRightAngle):
            case(ScriptScanner::TokComma):
            case(ScriptScanner::TokLeftParen):
            case(ScriptScanner::TokRightParen):
            case(ScriptScanner::TokRightSquare):
            case(ScriptScanner::TokSlash):
            case(ScriptScanner::TokColon):
            case(ScriptScanner::TokEquals):
            case(ScriptScanner::TokLiteral):
                currentFormat.text += previous.value.toString();
            case(ScriptScanner::TokEos):
            case(ScriptScanner::TokError):
                break;
        }
    }

    void formatBlock()
    {
        QString func;
        QString option;

        bool formatOption{false};
        while(current.type != ScriptScanner::TokRightAngle && current.type != ScriptScanner::TokEos) {
            advance();
            if(previous.type == ScriptScanner::TokEquals) {
                formatOption = true;
                advance();
            }

            if(formatOption) {
                option.append(previous.value.toString());
            }
            else {
                func.append(previous.value.toString());
            }
        }

        processFormat(func, option);
        closeFormat(func);
        closeBlock();
    }

    void processFormat(const QString& func, const QString& option)
    {
        if(registry.isFormatFunc(func)) {
            addBlock();
            registry.format(currentFormat, func, option);
        }
        else {
            error(QStringLiteral("Format option not found"));
        }

        consume(ScriptScanner::TokRightAngle, QStringLiteral("Expected '>' after expression"));

        while(current.type != ScriptScanner::TokEos
              && (current.type != ScriptScanner::TokLeftAngle
                  || (scanner.peekNext().type != ScriptScanner::TokSlash && scanner.peekNext(2).value != func))) {
            expression();
        }

        consume(ScriptScanner::TokLeftAngle, QStringLiteral("Expected '<' after expression"));
        consume(ScriptScanner::TokSlash, QStringLiteral("Expected '/' after expression"));
    }

    void closeFormat(const QString& func)
    {
        QString closeOption;

        while(current.type != ScriptScanner::TokRightAngle && current.type != ScriptScanner::TokEos) {
            advance();
            closeOption.append(previous.value.toString());
        }

        if(closeOption != func) {
            error(QStringLiteral("Formatting option not closed"));
        }

        consume(ScriptScanner::TokRightAngle, QStringLiteral("Expected '>' after expression"));
    }

    void addBlock()
    {
        if(!currentFormat.text.isEmpty()) {
            if(nestedFormats.empty()) {
                formatResult.emplace_back(currentFormat);
            }
            else {
                formatStack.emplace(currentFormat);
            }
            currentFormat.text.clear();
        }
    }

    void closeBlock()
    {
        if(!currentFormat.text.isEmpty()) {
            if(formatStack.empty()) {
                formatResult.emplace_back(currentFormat);
            }
            else {
                nestedFormats.emplace_back(currentFormat);
            }
        }

        if(!formatStack.empty()) {
            currentFormat = formatStack.top();
            formatStack.pop();
        }
        else {
            std::ranges::reverse(nestedFormats);
            formatResult.insert(formatResult.end(), nestedFormats.cbegin(), nestedFormats.cend());
            nestedFormats.clear();
            resetFormat();
        }
    }

    void resetFormat()
    {
        currentFormat               = {};
        currentFormat.format.colour = qApp->palette().text().color();
    }
};

ScriptFormatter::ScriptFormatter()
    : p{std::make_unique<Private>()}
{ }

ScriptFormatter::~ScriptFormatter() = default;

FormattedText ScriptFormatter::evaluate(const QString& input)
{
    if(input.isEmpty()) {
        return {};
    }

    p->resetFormat();
    p->formatResult.clear();
    p->scanner.setup(input);

    p->advance();
    while(p->current.type != ScriptScanner::TokEos) {
        p->expression();
    }

    p->consume(ScriptScanner::TokEos, QStringLiteral("Expected end of expression"));

    if(!p->currentFormat.text.isEmpty()) {
        p->formatResult.emplace_back(p->currentFormat);
    }

    return p->formatResult;
}
} // namespace Fooyin
