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
#include <QRegularExpression>

#include <stack>

using namespace Qt::StringLiterals;

namespace Fooyin {
class ScriptFormatterPrivate
{
public:
    struct FormatTag
    {
        QString name;
        QString option;
    };

    void advance();
    void consume(ScriptScanner::TokenType type);
    void consume(ScriptScanner::TokenType type, const QString& message);

    void errorAtCurrent(const QString& message);
    void error(const QString& message);
    void errorAt(const ScriptScanner::Token& token, const QString& message);

    void expression();
    void formatBlock();
    void processFormat(const FormatTag& tag);
    void addBlock();
    void closeBlock();
    void resetFormat();
    [[nodiscard]] QString readTagContent();
    [[nodiscard]] FormatTag parseFormatTag(const QString& content) const;
    [[nodiscard]] QString parseHtmlAttribute(const QString& attrs, const QString& name) const;
    [[nodiscard]] QString peekClosingTagName();

    ScriptScanner m_scanner;
    ScriptFormatterRegistry m_registry;
    QFont m_font;
    QColor m_colour;

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
        errorAtCurrent(m_current.value.toString());
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
    QString errorMsg = u"[%1] Error"_s.arg(token.position);

    if(token.type == ScriptScanner::TokEos) {
        errorMsg += u" at end of string"_s;
    }
    else {
        errorMsg += u": '"_s + token.value.toString() + u"'"_s;
    }

    errorMsg += u" (%1)"_s.arg(message);

    ScriptError currentError;
    currentError.value    = token.value.toString();
    currentError.position = token.position;
    currentError.message  = errorMsg;

    m_errors.emplace_back(currentError);
}

void ScriptFormatterPrivate::expression()
{
    advance();
    if(m_previous.type == ScriptScanner::TokLeftAngle) {
        formatBlock();
    }
    else if(m_previous.type == ScriptScanner::TokEscape) {
        advance();
        m_currentBlock.text += m_previous.value;
    }
    else if(m_previous.type != ScriptScanner::TokEos && m_previous.type != ScriptScanner::TokError) {
        m_currentBlock.text += m_previous.value;
    }
}

void ScriptFormatterPrivate::formatBlock()
{
    const FormatTag tag = parseFormatTag(readTagContent());
    consume(ScriptScanner::TokRightAngle, u"Expected '>' after expression"_s);

    if(tag.name.isEmpty()) {
        error(u"Format option not found"_s);
        return;
    }

    processFormat(tag);
    closeBlock();
}

QString ScriptFormatterPrivate::readTagContent()
{
    QString content;

    while(m_current.type != ScriptScanner::TokRightAngle && m_current.type != ScriptScanner::TokEos) {
        content.append(m_current.value);
        advance();
    }

    return content.trimmed();
}

ScriptFormatterPrivate::FormatTag ScriptFormatterPrivate::parseFormatTag(const QString& content) const
{
    if(content.isEmpty()) {
        return {};
    }

    const QString trimmed = content.trimmed();
    const int firstSpace  = trimmed.indexOf(u' ');
    const int firstEquals = trimmed.indexOf(u'=');

    if(firstEquals >= 0 && (firstSpace < 0 || firstEquals < firstSpace)) {
        return {
            .name   = trimmed.left(firstEquals).trimmed().toLower(),
            .option = trimmed.mid(firstEquals + 1).trimmed(),
        };
    }

    const QString name = (firstSpace < 0 ? trimmed : trimmed.left(firstSpace)).trimmed().toLower();
    if(name.isEmpty()) {
        return {};
    }

    FormatTag tag{.name = name, .option = {}};
    if(firstSpace >= 0 && name == "a"_L1) {
        tag.option = parseHtmlAttribute(trimmed.mid(firstSpace + 1), u"href"_s);
    }

    return tag;
}

QString ScriptFormatterPrivate::parseHtmlAttribute(const QString& attrs, const QString& name) const
{
    const QRegularExpression attrRegex{
        u"(?:^|\\s)%1\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)'|([^\\s\"'>]+))"_s.arg(QRegularExpression::escape(name))};
    const QRegularExpressionMatch match = attrRegex.match(attrs);
    if(!match.hasMatch()) {
        return {};
    }

    for(int i = 1; i <= 3; ++i) {
        if(!match.captured(i).isEmpty()) {
            return match.captured(i);
        }
    }

    return {};
}

QString ScriptFormatterPrivate::peekClosingTagName()
{
    if(m_current.type != ScriptScanner::TokLeftAngle || m_scanner.peekNext().type != ScriptScanner::TokSlash) {
        return {};
    }

    QString content;
    int delta = 2;
    while(true) {
        const auto token = m_scanner.peekNext(delta++);
        if(token.type == ScriptScanner::TokRightAngle || token.type == ScriptScanner::TokEos) {
            break;
        }
        content.append(token.value);
    }

    return content.trimmed().toLower();
}

void ScriptFormatterPrivate::processFormat(const FormatTag& tag)
{
    RichFormatting nextFormatting{m_currentBlock.format};

    if(m_registry.format(nextFormatting, tag.name, tag.option)) {
        addBlock();
        m_currentBlock.format = std::move(nextFormatting);
    }
    else {
        error(u"Format option not found"_s);
    }

    while(m_current.type != ScriptScanner::TokEos) {
        if(m_current.type == ScriptScanner::TokLeftAngle && peekClosingTagName() == tag.name) {
            break;
        }
        expression();
    }

    consume(ScriptScanner::TokLeftAngle);
    consume(ScriptScanner::TokSlash);

    QString closeOption;
    closeOption.reserve(tag.name.size());

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
            m_formatResult.blocks.emplace_back(m_currentBlock);
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
            m_formatResult.blocks.emplace_back(m_currentBlock);
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
        m_formatResult.blocks.insert(m_formatResult.blocks.end(), m_blockGroup.cbegin(), m_blockGroup.cend());
        m_blockGroup.clear();
        resetFormat();
    }
}

void ScriptFormatterPrivate::resetFormat()
{
    m_currentBlock               = {};
    m_currentBlock.format.font   = m_font;
    m_currentBlock.format.colour = m_colour.isValid() ? m_colour : QApplication::palette().text().color();
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

    if(!input.contains(u'<') && !input.contains(u'\\')) {
        p->m_currentBlock.text = input;
        p->m_formatResult.blocks.emplace_back(p->m_currentBlock);
        return p->m_formatResult;
    }

    p->m_scanner.setup(input);

    p->advance();
    while(p->m_current.type != ScriptScanner::TokEos) {
        p->expression();
    }

    p->consume(ScriptScanner::TokEos, u"Expected end of expression"_s);

    if(!p->m_currentBlock.text.isEmpty()) {
        p->m_formatResult.blocks.emplace_back(p->m_currentBlock);
    }

    return p->m_formatResult;
}
void ScriptFormatter::setBaseFont(const QFont& font)
{
    p->m_font = font;
}

void ScriptFormatter::setBaseColour(const QColor& colour)
{
    p->m_colour = colour;
}
} // namespace Fooyin
