/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include <gui/scripting/richtextutils.h>

#include <QFontMetrics>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
RichTextMetrics measureRichTextLine(const RichText& richText, const QFont& baseFont)
{
    RichTextMetrics metrics;
    const int defaultHeight = QFontMetrics{baseFont}.height();

    for(const auto& block : richText.blocks) {
        const QFont font = resolvedRichTextFont(block.format, baseFont);
        const QFontMetrics fm{font};

        metrics.width += fm.horizontalAdvance(block.text);
        metrics.height = std::max(metrics.height, fm.height());
    }

    metrics.height = std::max(metrics.height, defaultHeight);
    return metrics;
}
} // namespace

QColor resolvedRichTextColour(const RichFormatting& formatting, const QColor& baseColour, const QColor& linkColour)
{
    QColor colour;
    if(formatting.colour.isExplicit()) {
        colour = formatting.colour.colour;
    }
    else if(!formatting.link.isEmpty() && linkColour.isValid()) {
        colour = linkColour;
    }
    else {
        colour = baseColour;
    }

    if(formatting.colour.alpha >= 0) {
        colour.setAlpha(formatting.colour.alpha);
    }
    return colour;
}

QFont resolvedRichTextFont(const RichFormatting& formatting, const QFont& baseFont)
{
    return formatting.font == QFont{} ? baseFont : formatting.font.resolve(baseFont);
}

RichText trimRichText(RichText richText)
{
    while(!richText.blocks.empty()) {
        auto& block = richText.blocks.front();

        int firstNonSpace{0};
        while(firstNonSpace < block.text.size() && block.text.at(firstNonSpace).isSpace()) {
            ++firstNonSpace;
        }

        if(firstNonSpace > 0) {
            block.text.remove(0, firstNonSpace);
        }

        if(!block.text.isEmpty()) {
            break;
        }

        richText.blocks.erase(richText.blocks.begin());
    }

    while(!richText.blocks.empty()) {
        auto& block = richText.blocks.back();

        int lastNonSpace = block.text.size() - 1;
        while(lastNonSpace >= 0 && block.text.at(lastNonSpace).isSpace()) {
            --lastNonSpace;
        }

        block.text.truncate(lastNonSpace + 1);
        if(!block.text.isEmpty()) {
            break;
        }

        richText.blocks.pop_back();
    }

    return richText;
}

QString richTextToHtml(const RichText& richText, const QColor& linkColour)
{
    QString html;
    html.reserve(richText.joinedText().size() * 2);

    for(const auto& [text, format] : richText.blocks) {
        QStringList styles;

        const bool isLink = !format.link.isEmpty();

        if(!format.font.family().isEmpty()) {
            styles.emplace_back(u"font-family:'%1'"_s.arg(format.font.family().toHtmlEscaped()));
        }
        if(format.font.pointSizeF() > 0) {
            styles.emplace_back(u"font-size:%1pt"_s.arg(format.font.pointSizeF()));
        }
        if(format.font.bold()) {
            styles.emplace_back(u"font-weight:bold"_s);
        }
        if(format.font.italic()) {
            styles.emplace_back(u"font-style:italic"_s);
        }
        if(format.font.underline()) {
            styles.emplace_back(u"text-decoration:underline"_s);
        }
        if(format.font.strikeOut()) {
            styles.emplace_back(u"text-decoration:line-through"_s);
        }
        if(isLink && linkColour.isValid()) {
            QColor colour{linkColour};
            if(format.colour.alpha >= 0) {
                colour.setAlpha(format.colour.alpha);
            }
            styles.emplace_back(u"color:%1"_s.arg(colour.name(QColor::HexArgb)));
        }
        else if(format.colour.isExplicit()) {
            QColor colour{format.colour.colour};
            if(format.colour.alpha >= 0) {
                colour.setAlpha(format.colour.alpha);
            }
            styles.emplace_back(u"color:%1"_s.arg(colour.name(QColor::HexArgb)));
        }

        QString htmlText = text.toHtmlEscaped();
        htmlText.replace(u'\n', u"<br/>"_s);

        const QString style = u"white-space:pre-wrap;%1"_s.arg(styles.join(u";"_s));

        if(!isLink) {
            html += u"<span style=\"%1\">%2</span>"_s.arg(style, htmlText);
        }
        else {
            html += u"<a href=\"%1\"><span style=\"%2\">%3</span></a>"_s.arg(format.link.toHtmlEscaped(), style, text);
        }
    }

    return html;
}

bool richTextHasLineBreaks(const RichText& richText)
{
    for(const auto& block : richText.blocks) {
        for(const QChar character : block.text) {
            if(character == '\n'_L1 || character == QChar::LineSeparator || character == QChar::ParagraphSeparator) {
                return true;
            }
        }
    }

    return false;
}

std::vector<RichText> splitRichTextLines(const RichText& richText)
{
    if(richText.empty()) {
        return {};
    }

    std::vector<RichText> lines(1);

    for(const auto& block : richText.blocks) {
        const QStringView text{block.text};
        if(text.isEmpty()) {
            continue;
        }

        qsizetype lineStart{0};
        for(qsizetype index{0}; index < text.size(); ++index) {
            const QChar character = text.at(index);
            if(character != '\n'_L1 && character != QChar::LineSeparator && character != QChar::ParagraphSeparator) {
                continue;
            }

            const QStringView segment = text.sliced(lineStart, index - lineStart);
            if(!segment.isEmpty()) {
                auto lineBlock = block;
                lineBlock.text = segment.toString();
                lines.back().blocks.push_back(std::move(lineBlock));
            }

            lines.emplace_back();
            lineStart = index + 1;
        }

        const QStringView trailingSegment = text.sliced(lineStart);
        if(!trailingSegment.isEmpty()) {
            auto lineBlock = block;
            lineBlock.text = trailingSegment.toString();
            lines.back().blocks.push_back(std::move(lineBlock));
        }
    }

    return lines;
}

int richTextExtraLineHeight(const RichText& richText, const QFont& baseFont)
{
    return measureRichText(richText, baseFont).extraLineHeight;
}

RichTextMetrics measureRichText(const RichText& richText, const QFont& baseFont)
{
    RichTextMetrics metrics;
    bool isFirstLine{true};

    for(const auto& line : splitRichTextLines(richText)) {
        const auto lineMetrics = measureRichTextLine(line, baseFont);

        metrics.width = std::max(metrics.width, lineMetrics.width);
        metrics.height += lineMetrics.height;
        if(isFirstLine) {
            metrics.firstLineWidth  = lineMetrics.width;
            metrics.firstLineHeight = lineMetrics.height;
            isFirstLine             = false;
        }
        else {
            metrics.extraLineHeight += lineMetrics.height;
        }
    }

    return metrics;
}

RichTextBlockMetrics measureRichTextBlock(const RichText& richText, const QFont& baseFont, int minimumLineHeight)
{
    return measureRichTextBlock(splitRichTextLines(richText), baseFont, minimumLineHeight);
}

RichTextBlockMetrics measureRichTextBlock(const std::vector<RichText>& lines, const QFont& baseFont,
                                          int minimumLineHeight)
{
    RichTextBlockMetrics blockMetrics;
    blockMetrics.lines.reserve(lines.size());

    for(const auto& line : lines) {
        const auto metrics   = measureRichText(line, baseFont);
        const int lineHeight = std::max(metrics.height, minimumLineHeight);

        blockMetrics.width = std::max(blockMetrics.width, metrics.width);
        blockMetrics.height += lineHeight;
        blockMetrics.lines.push_back({.text = line, .metrics = metrics, .height = lineHeight});
    }

    return blockMetrics;
}
} // namespace Fooyin
