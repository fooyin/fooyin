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

#include <gui/scripting/scriptformatterregistry.h>

#include <QCoreApplication>
#include <QRegularExpression>

#include <optional>

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
    void expression();
    bool formatBlock();
    void processFormat(const FormatTag& tag, qsizetype tagPosition, qsizetype tagLength);
    void flushCurrentBlock();
    void resetFormat();

    void addError(qsizetype position, QStringView value);

    [[nodiscard]] std::optional<FormatTag> peekFormatTag() const;
    [[nodiscard]] bool isClosingTag(const QString& name) const;
    [[nodiscard]] qsizetype tagEnd() const;
    [[nodiscard]] qsizetype tagContentEnd() const;

    QFont m_font;
    QColor m_colour;

    QStringView m_input;
    qsizetype m_position{0};

    RichTextBlock m_currentBlock;

    ErrorList m_errors;
    RichText m_formatResult;
};

void ScriptFormatterPrivate::expression()
{
    const QChar current = m_input.at(m_position);

    if(current == u'<' && formatBlock()) {
        return;
    }

    if(current == u'\\') {
        ++m_position;
        if(m_position < m_input.size()) {
            m_currentBlock.text += m_input.at(m_position++);
        }
        return;
    }

    const qsizetype start = m_position++;
    while(m_position < m_input.size() && m_input.at(m_position) != u'<' && m_input.at(m_position) != u'\\') {
        ++m_position;
    }
    m_currentBlock.text += m_input.sliced(start, m_position - start);
}

bool ScriptFormatterPrivate::formatBlock()
{
    const auto tag = peekFormatTag();
    if(!tag) {
        return false;
    }

    const qsizetype tagPos{m_position};
    const qsizetype end = tagEnd();
    m_position          = end + 1;
    processFormat(*tag, tagPos, end - tagPos + 1);
    return true;
}

std::optional<FormatTag> ScriptFormatterPrivate::peekFormatTag() const
{
    const qsizetype end = tagEnd();
    if(end < 0 || end == m_position + 1) {
        return {};
    }

    const QStringView content = m_input.sliced(m_position + 1, end - m_position - 1);
    if(content.front().isSpace()) {
        return {};
    }

    FormatTag tag = parseFormatTag(content.toString());
    if(!ScriptFormatterRegistry::isKnown(tag.name)) {
        return {};
    }

    return tag;
}

bool ScriptFormatterPrivate::isClosingTag(const QString& name) const
{
    if(m_position + 2 >= m_input.size() || m_input.at(m_position) != u'<' || m_input.at(m_position + 1) != u'/') {
        return false;
    }

    const qsizetype end = tagEnd();
    if(end < 0) {
        return false;
    }

    return m_input.sliced(m_position + 2, end - m_position - 2).trimmed().compare(name, Qt::CaseInsensitive) == 0;
}

qsizetype ScriptFormatterPrivate::tagEnd() const
{
    const qsizetype end = tagContentEnd();
    return end < m_input.size() && m_input.at(end) == u'>' ? end : -1;
}

qsizetype ScriptFormatterPrivate::tagContentEnd() const
{
    for(qsizetype pos{m_position + 1}; pos < m_input.size(); ++pos) {
        const QChar current = m_input.at(pos);
        if(current == u'>' || current == u'<') {
            return pos;
        }
    }

    return m_input.size();
}

void ScriptFormatterPrivate::processFormat(const FormatTag& tag, qsizetype tagPosition, qsizetype tagLength)
{
    const RichFormatting previousFormatting{m_currentBlock.format};
    RichFormatting nextFormatting{m_currentBlock.format};

    const bool formatApplied = ScriptFormatterRegistry::format(nextFormatting, tag.name, tag.option);

    if(formatApplied) {
        flushCurrentBlock();
        m_currentBlock.format = std::move(nextFormatting);
    }
    else {
        addError(tagPosition, m_input.sliced(tagPosition, tagLength));
    }

    while(m_position < m_input.size() && !isClosingTag(tag.name)) {
        expression();
    }

    if(m_position < m_input.size()) {
        m_position = tagEnd() + 1;
    }

    if(formatApplied) {
        flushCurrentBlock();
        m_currentBlock.format = previousFormatting;
    }
}

void ScriptFormatterPrivate::addError(qsizetype position, QStringView value)
{
    QString errorMessage = QCoreApplication::translate("Fooyin::ScriptFormatter",
                                                       "[%1] Error in formatting tag '%2': invalid formatting option.")
                               .arg(position)
                               .arg(value);

    m_errors.emplace_back(ScriptError{
        .position = static_cast<int>(position),
        .value    = value.toString(),
        .message  = std::move(errorMessage),
    });
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
    p->m_errors.clear();
    p->m_formatResult.clear();

    if(input.isEmpty()) {
        return {};
    }

    p->resetFormat();

    if(!input.contains(u'<') && !input.contains(u'\\')) {
        p->m_currentBlock.text = input;
        p->m_formatResult.blocks.emplace_back(p->m_currentBlock);
        return p->m_formatResult;
    }

    p->m_input    = input;
    p->m_position = 0;
    while(p->m_position < p->m_input.size()) {
        p->expression();
    }

    p->flushCurrentBlock();
    p->m_input = {};

    return p->m_formatResult;
}

const ErrorList& ScriptFormatter::errors() const
{
    return p->m_errors;
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
