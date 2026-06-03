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

#include "queueviewerdelegate.h"

#include "queuevieweritem.h"

#include <gui/scripting/richtext.h>
#include <gui/scripting/richtextutils.h>

#include <QApplication>
#include <QPainter>

constexpr auto RightContentPadding = 5;

namespace Fooyin {
namespace {
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

using PreparedTextLines = std::vector<PreparedTextLine>;

int alignedLineX(const QRect& rect, int lineWidth, Qt::Alignment alignment)
{
    if(lineWidth < rect.width() && alignment & Qt::AlignRight) {
        return rect.right() - lineWidth + 1;
    }

    return rect.x();
}

PreparedTextLines prepareTextLines(const QStyleOptionViewItem& option, int maxWidth, const RichText& richText)
{
    PreparedTextLines result;
    if(maxWidth <= 0) {
        return result;
    }

    const QColor selectedColor = option.palette.color(QPalette::HighlightedText);
    const QColor defaultColour = option.palette.color(QPalette::Text);
    const QColor linkColour    = option.palette.color(QPalette::Link);
    const int defaultHeight    = QFontMetrics{option.font}.height();

    const auto richLines = splitRichTextLines(richText);
    result.reserve(richLines.size());

    for(const auto& richLine : richLines) {
        PreparedTextLine line;
        int remainingWidth{maxWidth};

        for(const auto& block : richLine.blocks) {
            if(block.text.isEmpty() || remainingWidth <= 0) {
                continue;
            }

            const QFont font = resolvedRichTextFont(block.format, option.font);
            QColor colour    = resolvedRichTextColour(block.format, defaultColour, linkColour);
            if(option.state & QStyle::State_Selected) {
                colour = selectedColor;
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
            prepared.width  = metrics.boundingRect(text).width();
            prepared.height = metrics.height();

            line.totalWidth += prepared.width;
            line.height = std::max(line.height, prepared.height);
            remainingWidth -= prepared.width;

            line.blocks.push_back(std::move(prepared));

            if(text != block.text) {
                break;
            }
        }

        line.height = std::max(line.height, defaultHeight);
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
        int lineHeight{0};

        for(const auto& block : line.blocks) {
            if(block.text.isEmpty()) {
                continue;
            }

            const QFont font = resolvedRichTextFont(block.format, option.font);
            const QFontMetrics metrics{font};

            lineWidth += metrics.boundingRect(block.text).width();
            lineHeight = std::max(lineHeight, metrics.height());
        }

        size.setWidth(std::max(size.width(), lineWidth));
        size.setHeight(size.height() + std::max(lineHeight, QFontMetrics{option.font}.height()));
    }

    return size;
}

void drawPreparedTextLines(QPainter* painter, const QStyleOptionViewItem& option, const QRect& rect,
                           const PreparedTextLines& lines, Qt::Alignment alignment)
{
    if(lines.empty() || rect.width() <= 0 || rect.height() <= 0) {
        return;
    }

    const QStyle* style   = option.widget ? option.widget->style() : QApplication::style();
    const auto colourRole = option.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::NoRole;

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

            const QRect blockRect{x, y, std::max(0, rect.right() - x + 1), line.height};
            style->drawItemText(painter, blockRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                                option.palette, true, block.text, colourRole);
            x += block.width;
        }

        y += line.height;
    }
}
} // namespace

void QueueViewerDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);
    opt.decorationSize         = option.decorationSize;
    opt.showDecorationSelected = true;
    opt.text.clear();

    const QStyle* style  = opt.widget ? opt.widget->style() : QApplication::style();
    const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, opt.widget) * 2;
    QRect textRect       = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);
    textRect.adjust(textMargin, 0, -textMargin, 0);

    const auto leftRichText  = index.data(QueueViewerItem::RichTitle).value<RichText>();
    const auto rightRichText = index.data(QueueViewerItem::RichRightText).value<RichText>();
    const QSize rightSize    = richTextNaturalSize(opt, rightRichText);

    const int rightWidth = std::min(rightSize.width() + RightContentPadding, textRect.width());
    int leftWidth        = textRect.width();
    if(rightWidth > 0) {
        leftWidth = std::max(0, textRect.width() - rightWidth - textMargin);
    }

    QRect leftRect{textRect};
    leftRect.setWidth(leftWidth);

    QRect rightRect{textRect};
    rightRect.setLeft(textRect.right() - rightWidth + 1);

    const auto leftLines  = prepareTextLines(opt, leftRect.width(), leftRichText);
    const auto rightLines = prepareTextLines(opt, rightRect.width(), rightRichText);

    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, option.widget);
    drawPreparedTextLines(painter, opt, leftRect, leftLines, Qt::AlignLeft);
    drawPreparedTextLines(painter, opt, rightRect, rightLines, Qt::AlignRight);
}

QSize QueueViewerDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);

    opt.decorationSize = option.decorationSize;

    const QStyle* style   = opt.widget ? opt.widget->style() : QApplication::style();
    const int textGap     = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, opt.widget) * 2;
    const QSize leftSize  = richTextNaturalSize(opt, index.data(QueueViewerItem::RichTitle).value<RichText>());
    const QSize rightSize = richTextNaturalSize(opt, index.data(QueueViewerItem::RichRightText).value<RichText>());
    const QSize textSize{leftSize.width() + rightSize.width()
                             + (rightSize.width() > 0 ? textGap + RightContentPadding : 0),
                         std::max(leftSize.height(), rightSize.height())};

    QSize size = style->sizeFromContents(QStyle::CT_ItemViewItem, &opt, textSize, opt.widget);

    return size;
}
} // namespace Fooyin
