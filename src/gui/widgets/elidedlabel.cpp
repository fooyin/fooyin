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

#include <gui/widgets/elidedlabel.h>

#include <gui/scripting/richtextutils.h>

#include <QPainter>
#include <QResizeEvent>

#include <utility>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
QString singleLineText(QString text)
{
    text.replace('\n'_L1, ' '_L1);
    text.replace(QChar::LineSeparator, ' '_L1);
    text.replace(QChar::ParagraphSeparator, ' '_L1);
    return text;
}

RichText singleLineRichText(RichText richText)
{
    for(auto& block : richText.blocks) {
        block.text = singleLineText(std::move(block.text));
    }
    return richText;
}

QString elidedSingleLineText(const QString& text, const QFontMetrics& fm, Qt::TextElideMode elideMode, int width)
{
    QString elidedText = fm.elidedText(text, elideMode, width);
    if(elidedText == u"…"_s && !text.isEmpty()) {
        elidedText = text.left(1);
    }
    return elidedText;
}

struct PreparedTextBlock
{
    QString text;
    QFont font;
    QColor colour;
    int width{0};
    int height{0};
};

struct PreparedTextLine
{
    std::vector<PreparedTextBlock> blocks;
    int totalWidth{0};
    int height{0};
};

PreparedTextLine prepareRichTextLine(const std::vector<RichTextBlock>& blocks, const QFont& baseFont,
                                     const QColor& baseColour, const QColor& linkColour, Qt::TextElideMode elideMode,
                                     int maxWidth)
{
    PreparedTextLine line;
    line.height = QFontMetrics{baseFont}.height();

    int remainingWidth{maxWidth};

    for(const auto& block : blocks) {
        if(block.text.isEmpty() || remainingWidth <= 0) {
            continue;
        }

        const QFont font = resolvedRichTextFont(block.format, baseFont);
        const QFontMetrics fm{font};
        const QString text = elidedSingleLineText(block.text, fm, elideMode, remainingWidth);

        if(text.isEmpty()) {
            continue;
        }

        PreparedTextBlock prepared;
        prepared.text   = text;
        prepared.font   = font;
        prepared.colour = resolvedRichTextColour(block.format, baseColour, linkColour);
        prepared.width  = fm.horizontalAdvance(text);
        prepared.height = fm.height();

        line.totalWidth += prepared.width;
        line.height = std::max(line.height, prepared.height);
        remainingWidth -= prepared.width;

        line.blocks.push_back(std::move(prepared));

        if(text != block.text) {
            break;
        }
    }

    return line;
}

QSize richTextNaturalSize(const RichText& richText, const QFont& baseFont)
{
    if(richText.empty()) {
        const QFontMetrics fm{baseFont};
        return fm.size(Qt::TextSingleLine, {});
    }

    const auto metrics = measureRichText(richText, baseFont);
    return {metrics.width,
            std::max(metrics.firstLineHeight + metrics.extraLineHeight, QFontMetrics{baseFont}.height())};
}
} // namespace

ElidedLabel::ElidedLabel(QWidget* parent)
    : ElidedLabel{{}, Qt::ElideRight, parent}
{ }

ElidedLabel::ElidedLabel(Qt::TextElideMode elideMode, QWidget* parent)
    : ElidedLabel{{}, elideMode, parent}
{ }

ElidedLabel::ElidedLabel(QString text, Qt::TextElideMode elideMode, QWidget* parent)
    : QLabel{parent}
    , m_elideMode{elideMode}
    , m_text{std::move(text)}
    , m_isElided{false}
    , m_multilineEnabled{true}
{ }

Qt::TextElideMode ElidedLabel::elideMode() const
{
    return m_elideMode;
}

void ElidedLabel::setElideMode(Qt::TextElideMode elideMode)
{
    m_elideMode = elideMode;
    elideText(width());
}

QString ElidedLabel::text() const
{
    return m_text;
}

void ElidedLabel::setText(const QString& text)
{
    m_text     = text;
    m_richText = {};

    elideText(width());
}

const RichText& ElidedLabel::richText() const
{
    return m_richText;
}

bool ElidedLabel::multilineEnabled() const
{
    return m_multilineEnabled;
}

void ElidedLabel::setMultilineEnabled(bool enabled)
{
    if(m_multilineEnabled == enabled) {
        return;
    }

    m_multilineEnabled = enabled;

    if(!m_richText.empty()) {
        updateGeometry();
        update();
    }
}

void ElidedLabel::setRichText(const RichText& text)
{
    m_richText = text;
    m_text     = text.joinedText();
    m_isElided = false;

    QLabel::clear();

    updateGeometry();
    update();
}

void ElidedLabel::clear()
{
    setText({});
}

QSize ElidedLabel::sizeHint() const
{
    if(!m_richText.empty()) {
        const RichText richText = m_multilineEnabled ? m_richText : singleLineRichText(m_richText);
        const QSize richSize    = richTextNaturalSize(richText, font());
        return {richSize.width(), std::max(richSize.height(), QLabel::sizeHint().height())};
    }

    const QFontMetrics fm{fontMetrics()};
    const QSize size{fm.horizontalAdvance(m_text), QLabel::sizeHint().height()};
    return size;
}

QSize ElidedLabel::minimumSizeHint() const
{
    const QFontMetrics fm{fontMetrics()};
    const QSize size{fm.horizontalAdvance(m_text.left(2) + u"…"_s), QLabel::sizeHint().height()};
    return size;
}

void ElidedLabel::paintEvent(QPaintEvent* event)
{
    QLabel::paintEvent(event);

    if(m_richText.empty()) {
        return;
    }

    QPainter painter(this);
    const QRect rect = contentsRect();
    if(rect.width() <= 0 || rect.height() <= 0 || m_text.isEmpty()) {
        return;
    }

    const QPalette::ColorGroup group = isEnabled() ? QPalette::Normal : QPalette::Disabled;
    const QColor baseColour          = palette().color(group, foregroundRole());
    const QColor linkColour          = palette().color(group, QPalette::Link);
    const RichText richText          = m_multilineEnabled ? m_richText : singleLineRichText(m_richText);
    const auto logicalLines          = splitRichTextLines(richText);
    if(logicalLines.empty()) {
        return;
    }

    std::vector<PreparedTextLine> lines;
    lines.reserve(logicalLines.size());

    int totalHeight{0};
    for(const auto& logicalLine : logicalLines) {
        auto line = prepareRichTextLine(logicalLine.blocks, font(), baseColour, linkColour, m_elideMode, rect.width());
        totalHeight += line.height;
        lines.push_back(std::move(line));
    }

    int y = rect.y();
    if(alignment().testFlag(Qt::AlignBottom)) {
        y = rect.bottom() - totalHeight + 1;
    }
    else if(!alignment().testFlag(Qt::AlignTop)) {
        y += std::max(0, (rect.height() - totalHeight) / 2);
    }

    for(const auto& line : lines) {
        int x = rect.x();
        if(alignment().testFlag(Qt::AlignHCenter)) {
            x += std::max(0, (rect.width() - line.totalWidth) / 2);
        }
        else if(alignment().testFlag(Qt::AlignRight)) {
            x = rect.right() - line.totalWidth + 1;
        }

        for(const auto& block : line.blocks) {
            painter.setFont(block.font);
            painter.setPen(block.colour);
            painter.drawText(QRect{x, y, std::max(0, rect.right() - x + 1), line.height},
                             Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, block.text);
            x += block.width;
        }

        y += line.height;
        if(y > rect.bottom()) {
            break;
        }
    }
}

void ElidedLabel::resizeEvent(QResizeEvent* event)
{
    if(m_richText.empty()) {
        elideText(event->size().width());
    }

    QLabel::resizeEvent(event);
}

void ElidedLabel::elideText(int width)
{
    if(!m_richText.empty()) {
        QLabel::clear();
        updateGeometry();
        update();
        return;
    }

    const QFontMetrics fm{fontMetrics()};

    const QString elidedText = elidedSingleLineText(m_text, fm, m_elideMode, width - (margin() * 2) - indent());

    m_isElided = elidedText != m_text;

    QLabel::setText(elidedText);
}
} // namespace Fooyin
