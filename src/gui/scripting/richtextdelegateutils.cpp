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

#include "richtextdelegateutils.h"

#include <gui/guiutils.h>

#include <QFontMetrics>
#include <QPainter>
#include <QStyleOptionViewItem>

namespace Fooyin::Utils {
namespace {
int alignedLineX(const QRect& rect, int lineWidth, Qt::Alignment alignment)
{
    if(lineWidth < rect.width() && alignment & Qt::AlignRight) {
        return rect.right() - lineWidth + 1;
    }

    return rect.x();
}
} // namespace

PreparedTextLines prepareTextLines(const QStyleOptionViewItem& option, int maxWidth, const RichText& richText)
{
    PreparedTextLines result;
    if(maxWidth <= 0) {
        return result;
    }

    const QColor selectedColour = option.palette.color(Gui::itemViewSelectionTextRole(option));
    const QColor defaultColour  = option.palette.color(QPalette::Text);
    const QColor linkColour     = option.palette.color(QPalette::Link);

    const auto richLines = splitRichTextLines(richText);
    result.reserve(richLines.size());

    for(const auto& richLine : richLines) {
        PreparedTextLine line;
        line.baseline = textBaselineMetrics(option.font);
        int remainingWidth{maxWidth};

        for(const auto& block : richLine.blocks) {
            if(block.text.isEmpty() || remainingWidth <= 0) {
                continue;
            }

            const QFont font = resolvedRichTextFont(block.format, option.font);
            QColor colour    = resolvedRichTextColour(block.format, defaultColour, linkColour);
            if(option.state & QStyle::State_Selected) {
                colour = selectedColour;
            }

            const QFontMetrics metrics{font};
            const QString text = metrics.elidedText(block.text, Qt::ElideRight, remainingWidth);
            if(text.isEmpty()) {
                continue;
            }

            PreparedTextBlock prepared;
            prepared.text   = text;
            prepared.font   = font;
            prepared.colour = colour;
            prepared.width  = metrics.horizontalAdvance(text);

            line.totalWidth += prepared.width;
            line.baseline.expand(metrics);
            remainingWidth -= prepared.width;

            line.blocks.push_back(std::move(prepared));

            if(text != block.text) {
                break;
            }
        }

        line.height = line.baseline.height();
        result.push_back(std::move(line));
    }

    return result;
}

QSize richTextNaturalSize(const QStyleOptionViewItem& option, const RichText& richText)
{
    if(richText.empty()) {
        const QFontMetrics metrics{option.font};
        return metrics.size(Qt::TextSingleLine, {});
    }

    QSize size;
    const auto lines = splitRichTextLines(richText);

    for(const auto& line : lines) {
        int lineWidth{0};
        TextBaselineMetrics baseline = textBaselineMetrics(option.font);

        for(const auto& block : line.blocks) {
            if(block.text.isEmpty()) {
                continue;
            }

            const QFont font = resolvedRichTextFont(block.format, option.font);
            const QFontMetrics metrics{font};

            lineWidth += metrics.horizontalAdvance(block.text);
            baseline.expand(metrics);
        }

        size.setWidth(std::max(size.width(), lineWidth));
        size.setHeight(size.height() + baseline.height());
    }

    return size;
}

void drawPreparedTextLines(QPainter* painter, const QRect& rect, const PreparedTextLines& lines,
                           Qt::Alignment alignment)
{
    if(lines.empty() || rect.width() <= 0 || rect.height() <= 0) {
        return;
    }

    int totalHeight{0};
    for(const auto& line : lines) {
        totalHeight += line.height;
    }

    int y = rect.y() + std::max(0, (rect.height() - totalHeight) / 2);

    for(const auto& line : lines) {
        int x = alignedLineX(rect, line.totalWidth, alignment);

        for(const auto& block : line.blocks) {
            painter->setFont(block.font);
            painter->setPen(block.colour);
            painter->drawText(QPoint{x, y + line.baseline.ascent}, block.text);
            x += block.width;
        }

        y += line.height;
    }
}
} // namespace Fooyin::Utils
