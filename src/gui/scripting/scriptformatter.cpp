/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace {
struct FormatTag
{
    QString name;
    QString option;
};

QString parseHtmlAttribute(const QString& attrs, const QString& name)
{
    const QRegularExpression attrRegex{
        u"(?:^|\\s)%1\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)'|([^\\s\"'>]+))"_s.arg(QRegularExpression::escape(name))};
    const QRegularExpressionMatch match = attrRegex.match(attrs);
    if(!match.hasMatch()) {
        return {};
    }

    for(int i{1}; i <= 3; ++i) {
        if(!match.captured(i).isEmpty()) {
            return match.captured(i);
        }
    }

    return {};
}

FormatTag parseFormatTag(const QString& content)
{
    if(content.isEmpty()) {
        return {};
    }

    const QString trimmed = content.trimmed();
    const int firstSpace  = static_cast<int>(trimmed.indexOf(u' '));
    const int firstEquals = static_cast<int>(trimmed.indexOf(u'='));

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
} // namespace

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
    void processFormat(const FormatTag& tag);
    void flushCurrentBlock();
    void resetFormat();

    [[nodiscard]] QString readTagContent();
    [[nodiscard]] QString peekClosingTagName();

    ScriptScanner m_scanner;
    ScriptFormatterRegistry m_registry;
    QFont m_font;
    QColor m_colour;

    ScriptScanner::Token m_current;
    ScriptScanner::Token m_previous;

    RichTextBlock m_currentBlock;

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

namespace {
// The formatter shares the query scanner, which also emits TokLeftAngle/TokRightAngle
// for the keyword words "LESS"/"GREATER". A real bracket delimiter has the bracket
// character as its value, so check the value rather than the type alone.
bool isLiteralAngle(const ScriptScanner::Token& token, QChar bracket)
{
    return token.value.size() == 1 && token.value.front() == bracket;
}

bool isLeftAngle(const ScriptScanner::Token& token)
{
    return token.type == ScriptScanner::TokLeftAngle && isLiteralAngle(token, u'<');
}

bool isRightAngle(const ScriptScanner::Token& token)
{
    return token.type == ScriptScanner::TokRightAngle && isLiteralAngle(token, u'>');
}
} // namespace

void ScriptFormatterPrivate::expression()
{
    advance();
    if(isLeftAngle(m_previous)) {
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
    const QString rawContent   = readTagContent();
    const bool hasClosingAngle = isRightAngle(m_current);
    const FormatTag tag        = parseFormatTag(rawContent);

    // A genuine formatting tag opens with '<' immediately followed by a recognised
    // formatter name and closes with '>'. Anything else (e.g. "<3", "a < b >",
    // "<notatag>") is literal text the user wants shown verbatim, so emit the original
    // "<...>" sequence instead of dropping it. Option validity is left to
    // processFormat() so a known tag with a bad option still renders its inner text.
    const bool isFormatTag = hasClosingAngle && !rawContent.isEmpty() && !rawContent.front().isSpace()
                          && ScriptFormatterRegistry::isKnown(tag.name);

    if(!isFormatTag) {
        m_currentBlock.text += u'<';
        m_currentBlock.text += rawContent;
        if(hasClosingAngle) {
            m_currentBlock.text += u'>';
            advance();
        }
        return;
    }

    consume(ScriptScanner::TokRightAngle, u"Expected '>' after expression"_s);

    processFormat(tag);
}

QString ScriptFormatterPrivate::readTagContent()
{
    QString content;

    // Stop at a nested '<' as well as '>'/EOS: a real formatting tag never contains
    // a '<' in its content, so encountering one means the current '<' is literal text
    // and must not consume the following tag's opening delimiter.
    while(!isRightAngle(m_current) && !isLeftAngle(m_current) && m_current.type != ScriptScanner::TokEos) {
        content.append(m_current.value);
        advance();
    }

    return content;
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
    const RichFormatting previousFormatting{m_currentBlock.format};
    RichFormatting nextFormatting{m_currentBlock.format};

    const bool formatApplied = ScriptFormatterRegistry::format(nextFormatting, tag.name, tag.option);

    if(formatApplied) {
        flushCurrentBlock();
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

    if(formatApplied) {
        flushCurrentBlock();
        m_currentBlock.format = previousFormatting;
    }
}

void ScriptFormatterPrivate::flushCurrentBlock()
{
    if(!m_currentBlock.text.isEmpty()) {
        m_formatResult.blocks.emplace_back(m_currentBlock);
        m_currentBlock.text.clear();
    }
}

void ScriptFormatterPrivate::resetFormat()
{
    m_currentBlock             = {};
    m_currentBlock.format.font = m_font;
    if(m_colour.isValid()) {
        m_currentBlock.format.colour.setColour(m_colour);
    }
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

    p->flushCurrentBlock();

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
