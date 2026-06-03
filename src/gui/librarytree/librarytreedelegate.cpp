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

#include "librarytreedelegate.h"

#include "librarytreeitem.h"

#include <gui/scripting/richtext.h>
#include <gui/scripting/richtextutils.h>

#include <QApplication>
#include <QPainter>

constexpr auto RightContentPadding = 5;

namespace Fooyin {
namespace {
struct DrawTextResult
{
    QRect bound;
    int totalWidth{0};
    int maxHeight{0};
};

struct PaintLine
{
    DrawTextResult measure;
    RichText text;
    int height{0};
};

struct PaintBlock
{
    std::vector<PaintLine> lines;
    int width{0};
    int height{0};
};

RichText fallbackRichText(const QStyleOptionViewItem& option, const QModelIndex& index)
{
    auto richText = index.data(LibraryTreeItem::RichTitle).value<RichText>();
    if(!richText.empty()) {
        return richText;
    }

    RichText fallback;
    const QString text = index.data(Qt::DisplayRole).toString();
    if(text.isEmpty()) {
        return fallback;
    }

    RichTextBlock block;
    block.text        = text;
    block.format.font = option.font;
    fallback.blocks.push_back(std::move(block));
    return fallback;
}

RichText rightRichText(const QModelIndex& index)
{
    return index.data(LibraryTreeItem::RightRichTitle).value<RichText>();
}

DrawTextResult drawTextBlocks(QPainter* painter, const QStyleOptionViewItem& option, QRect rect,
                              const std::vector<RichTextBlock>& blocks)
{
    DrawTextResult result;

    const auto selectedColour = option.palette.color(QPalette::HighlightedText);
    const auto defaultColour  = option.palette.color(QPalette::Text);
    const auto linkColour     = option.palette.color(QPalette::Link);

    for(const auto& block : blocks) {
        if(block.text.isEmpty() || rect.width() <= 0) {
            continue;
        }

        const QFont font = resolvedRichTextFont(block.format, option.font);

        painter->setFont(font);

        QColor colour = resolvedRichTextColour(block.format, defaultColour, linkColour);
        if(option.state & QStyle::State_Selected) {
            colour = selectedColour;
        }
        painter->setPen(colour);

        const QFontMetrics metrics{font};
        const QString text = metrics.elidedText(block.text, Qt::ElideRight, rect.width());
        if(text.isEmpty()) {
            continue;
        }

        result.bound = painter->boundingRect(rect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, text);
        rect.setWidth(std::max(0, rect.width() - result.bound.width()));
        rect.moveLeft(rect.x() + result.bound.width());
        result.totalWidth += result.bound.width();
        result.maxHeight = std::max(result.maxHeight, result.bound.height());
    }

    return result;
}

void paintTextBlocks(QPainter* painter, const QStyleOptionViewItem& option, QRect rect, const DrawTextResult& measured,
                     const std::vector<RichTextBlock>& blocks)
{
    const QStyle* style = option.widget ? option.widget->style() : QApplication::style();

    const auto colourRole     = option.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::NoRole;
    const auto defaultColour  = option.palette.color(QPalette::Text);
    const auto selectedColour = option.palette.color(QPalette::HighlightedText);
    const auto linkColour     = option.palette.color(QPalette::Link);

    int x = rect.x();
    if(measured.totalWidth < rect.width() && option.displayAlignment & Qt::AlignHCenter) {
        x += (rect.width() - measured.totalWidth) / 2;
    }
    else if(measured.totalWidth < rect.width() && option.displayAlignment & Qt::AlignRight) {
        x += rect.width() - measured.totalWidth;
    }

    for(const auto& block : blocks) {
        if(block.text.isEmpty() || x > rect.right()) {
            continue;
        }

        const QFont font = resolvedRichTextFont(block.format, option.font);
        painter->setFont(font);

        QColor colour = resolvedRichTextColour(block.format, defaultColour, linkColour);
        if(option.state & QStyle::State_Selected) {
            colour = selectedColour;
        }
        painter->setPen(colour);

        const QFontMetrics metrics{font};
        const QString text = metrics.elidedText(block.text, Qt::ElideRight, std::max(0, rect.right() - x + 1));
        if(text.isEmpty()) {
            continue;
        }

        const QRect blockRect{x, rect.y(), std::max(0, rect.right() - x + 1), rect.height()};
        style->drawItemText(painter, blockRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, option.palette,
                            true, text, colourRole);
        x += metrics.horizontalAdvance(text);
    }
}

PaintBlock measureDrawTextBlock(QPainter* painter, const QStyleOptionViewItem& option, const QRect& rect,
                                const std::vector<RichText>& lines)
{
    PaintBlock block;
    block.lines.reserve(lines.size());

    for(const auto& line : lines) {
        const DrawTextResult measure = drawTextBlocks(painter, option, rect, line.blocks);
        const int lineHeight         = std::max(measure.maxHeight, option.fontMetrics.height());

        block.width = std::max(block.width, measure.totalWidth);
        block.height += lineHeight;
        block.lines.push_back({.measure = measure, .text = line, .height = lineHeight});
    }

    return block;
}

void paintMeasuredBlock(QPainter* painter, const QStyleOptionViewItem& option, QRect rect, const PaintBlock& block)
{
    int y{rect.y()};

    for(const auto& line : block.lines) {
        if(line.measure.totalWidth > 0) {
            QRect lineRect{rect.x(), y, rect.width(), line.height};
            if(option.direction == Qt::RightToLeft) {
                lineRect.setLeft(rect.right() - line.measure.totalWidth + 1);
                lineRect.setWidth(line.measure.totalWidth);
            }
            paintTextBlocks(painter, option, lineRect, line.measure, line.text.blocks);
        }

        y += line.height;
        if(y > rect.bottom()) {
            break;
        }
    }
}

QSize richTextSize(const QStyleOptionViewItem& option, const QModelIndex& index, int textGap)
{
    const auto richText  = fallbackRichText(option, index);
    const auto rightText = rightRichText(index);
    if(richText.empty()) {
        const QFontMetrics metrics{option.font};
        return metrics.size(Qt::TextSingleLine, index.data(Qt::DisplayRole).toString());
    }

    const auto lines      = splitRichTextLines(richText);
    const auto rightLines = splitRichTextLines(rightText);
    const auto leftBlock  = measureRichTextBlock(lines, option.font, option.fontMetrics.height());
    const auto rightBlock = measureRichTextBlock(rightLines, option.font, option.fontMetrics.height());

    const int width = leftBlock.width + (rightBlock.width > 0 ? textGap + RightContentPadding + rightBlock.width : 0);
    const QSize result{width, std::max(leftBlock.height, rightBlock.height)};
    return result;
}
} // namespace

void LibraryTreeDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    opt.decorationSize = option.decorationSize;

    const auto decPos = index.data(LibraryTreeItem::Role::DecorationPosition).value<QStyleOptionViewItem::Position>();
    if(!opt.icon.isNull()) {
        opt.decorationPosition = decPos;
    }

    const QStyle* style  = option.widget ? option.widget->style() : QApplication::style();
    const auto richText  = fallbackRichText(opt, index);
    const auto rightText = rightRichText(index);

    painter->save();

    if(opt.backgroundBrush.style() != Qt::NoBrush) {
        painter->fillRect(option.rect, opt.backgroundBrush);
        opt.backgroundBrush = Qt::NoBrush;
    }

    opt.text.clear();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, option.widget);

    if(!richText.empty()) {
        QRect textRect       = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, option.widget);
        const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, option.widget) * 2;
        textRect.adjust(textMargin, 0, -textMargin, 0);

        const auto lines      = splitRichTextLines(richText);
        const auto rightLines = splitRichTextLines(rightText);

        const PaintBlock rightBlock = measureDrawTextBlock(painter, opt, textRect, rightLines);

        QRect leftMeasureRect{textRect};
        if(rightBlock.width > 0) {
            leftMeasureRect.setWidth(
                std::max(0, leftMeasureRect.width() - rightBlock.width - textMargin - RightContentPadding));
        }
        const PaintBlock leftBlock = measureDrawTextBlock(painter, opt, leftMeasureRect, lines);
        const int totalHeight      = std::max(leftBlock.height, rightBlock.height);

        int y = textRect.y();
        if(totalHeight < textRect.height()) {
            y += (textRect.height() - totalHeight) / 2;
        }

        QRect leftRect{textRect.x(), y + ((totalHeight - leftBlock.height) / 2), textRect.width(), leftBlock.height};
        if(rightBlock.width > 0) {
            leftRect.setWidth(std::max(0, leftRect.width() - rightBlock.width - textMargin - RightContentPadding));
        }

        paintMeasuredBlock(painter, opt, leftRect, leftBlock);

        if(rightBlock.width > 0) {
            const QRect rightRect{textRect.right() - rightBlock.width + 1, y + ((totalHeight - rightBlock.height) / 2),
                                  rightBlock.width, rightBlock.height};
            paintMeasuredBlock(painter, opt, rightRect, rightBlock);
        }
    }

    painter->restore();
}

QSize LibraryTreeDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);
    opt.decorationSize = option.decorationSize;

    const auto decPos = index.data(LibraryTreeItem::Role::DecorationPosition).value<QStyleOptionViewItem::Position>();
    if(!opt.icon.isNull()) {
        opt.decorationPosition = decPos;
    }

    opt.text.clear();

    const QStyle* style  = opt.widget ? opt.widget->style() : QApplication::style();
    const int textGap    = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, opt.widget) * 2;
    const QSize textSize = richTextSize(opt, index, textGap);
    const QSize baseTextSize{textSize.width(), opt.fontMetrics.height()};
    const QSize baseSize     = style->sizeFromContents(QStyle::CT_ItemViewItem, &opt, baseTextSize, opt.widget);
    const int verticalMargin = std::max(0, baseSize.height() - baseTextSize.height());

    QSize size = style->sizeFromContents(QStyle::CT_ItemViewItem, &opt, textSize, opt.widget);
    size.setHeight(std::max(size.height(), textSize.height() + verticalMargin));

    const QSize sizeHint = index.data(Qt::SizeHintRole).toSize();
    if(sizeHint.height() > 0) {
        size.setHeight(sizeHint.height());
    }

    return size;
}
} // namespace Fooyin
