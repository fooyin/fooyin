/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "filterdelegate.h"

#include "filteritem.h"

#include <gui/scripting/richtext.h>
#include <gui/widgets/expandedtreeview.h>

#include <QApplication>
#include <QPainter>

#include <limits>

constexpr auto IconCaptionMargin = 20;

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
    int maxHeight{0};
};

int alignedLineX(const QRect& rect, int lineWidth, Qt::Alignment alignment)
{
    if(lineWidth < rect.width()) {
        if(alignment & Qt::AlignHCenter) {
            return rect.x() + ((rect.width() - lineWidth) / 2);
        }
        if(alignment & Qt::AlignRight) {
            return rect.right() - lineWidth + 1;
        }
    }

    return rect.x();
}

PreparedTextLine prepareTextBlocks(const QStyleOptionViewItem& option, int maxWidth,
                                   const std::vector<Fooyin::RichTextBlock>& blocks)
{
    PreparedTextLine result;

    const auto selectedColour = option.palette.color(QPalette::HighlightedText);
    const auto defaultColour  = option.palette.color(QPalette::Text);
    int remainingWidth        = maxWidth;

    for(const auto& block : blocks) {
        if(block.text.isEmpty() || remainingWidth <= 0) {
            continue;
        }

        QFont font = block.format.font;
        if(font == QFont{}) {
            font = option.font;
        }

        QColor colour = block.format.colour;
        if(!colour.isValid()) {
            colour = defaultColour;
        }
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
        prepared.height = metrics.height();

        remainingWidth -= prepared.width;
        result.totalWidth += prepared.width;
        result.maxHeight = std::max(result.maxHeight, prepared.height);
        result.blocks.push_back(std::move(prepared));

        if(text != block.text) {
            break;
        }
    }

    return result;
}

void drawPreparedTextLine(QPainter* painter, const QStyleOptionViewItem& option, QRect rect, Qt::Alignment alignment,
                          const PreparedTextLine& line)
{
    if(line.blocks.empty() || rect.width() <= 0 || rect.height() <= 0) {
        return;
    }

    const QStyle* style   = option.widget ? option.widget->style() : QApplication::style();
    const auto colourRole = option.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::NoRole;

    int x = alignedLineX(rect, line.totalWidth, alignment);

    for(const auto& block : line.blocks) {
        painter->setFont(block.font);
        painter->setPen(block.colour);

        const QRect blockRect{x, rect.y(), std::max(0, rect.right() - x + 1), rect.height()};
        style->drawItemText(painter, blockRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, option.palette,
                            true, block.text, colourRole);

        x += block.width;
    }
}
} // namespace

namespace Fooyin::Filters {
void FilterDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);
    setupFilterOption(&opt, index);

    // initStyleOption will override decorationSize when converting the QPixmap to a QIcon, so restore it here
    opt.decorationSize  = option.decorationSize;
    const QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();

    const auto* view        = qobject_cast<const ExpandedTreeView*>(opt.widget);
    const bool richTreeMode = view && view->viewMode() == ExpandedTreeView::ViewMode::Tree;
    const auto richText     = richTreeMode ? fallbackRichText(opt, index) : RichText{};

    IconCaptionLineList richLines;
    if(!richTreeMode) {
        richLines = iconRichLines(index, opt);
    }

    if(!richText.empty()) {
        painter->save();

        if(opt.backgroundBrush.style() != Qt::NoBrush) {
            painter->fillRect(option.rect, opt.backgroundBrush);
            opt.backgroundBrush = Qt::NoBrush;
        }

        opt.text.clear();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

        QRect textRect       = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);
        const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &opt, opt.widget) * 2;
        textRect.adjust(textMargin, 0, -textMargin, 0);
        drawTextBlocks(painter, opt, textRect, richText.blocks);

        painter->restore();
        return;
    }

    if(!richLines.empty()) {
        painter->save();

        const bool selected = opt.state & QStyle::State_Selected;
        if(selected) {
            painter->fillRect(option.rect, opt.palette.brush(QPalette::Highlight));
            opt.state &= ~QStyle::State_Selected;
            opt.backgroundBrush = Qt::NoBrush;
        }
        else if(opt.backgroundBrush.style() != Qt::NoBrush) {
            painter->fillRect(option.rect, opt.backgroundBrush);
            opt.backgroundBrush = Qt::NoBrush;
        }

        opt.text.clear();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

        const QRect textRect = iconTextRect(opt);
        drawRichTextLines(painter, opt, textRect, richLines);

        painter->restore();
        return;
    }

    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
}

QSize FilterDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);
    setupFilterOption(&opt, index);

    opt.decorationSize = option.decorationSize;

    const auto* view    = qobject_cast<const ExpandedTreeView*>(opt.widget);
    const QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    QSize size          = view && view->viewMode() == ExpandedTreeView::ViewMode::Icon
                            ? iconItemSize(opt, index)
                            : style->sizeFromContents(QStyle::CT_ItemViewItem, &opt, richTextSize(opt, index), opt.widget);

    const QSize sizeHint = index.data(Qt::SizeHintRole).toSize();
    if((!view || view->viewMode() != ExpandedTreeView::ViewMode::Icon) && sizeHint.height() > 0) {
        size.setHeight(sizeHint.height());
    }

    return size;
}

RichText FilterDelegate::recolourRichText(RichText richText, const QColor& colour)
{
    if(!colour.isValid()) {
        return richText;
    }

    for(auto& block : richText.blocks) {
        block.format.colour = colour;
    }

    return richText;
}

QRect FilterDelegate::iconTextRect(const QStyleOptionViewItem& option)
{
    const auto* view = qobject_cast<const ExpandedTreeView*>(option.widget);
    if(!view) {
        return option.rect;
    }

    const QStyle* style = option.widget ? option.widget->style() : QApplication::style();
    QRect rect          = option.rect;
    const int margin    = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &option, option.widget);

    switch(view->captionDisplay()) {
        case ExpandedTreeView::CaptionDisplay::Right: {
            rect = style->subElementRect(QStyle::SE_ItemViewItemText, &option, option.widget);
            rect.adjust(margin, 0, -margin, 0);
            break;
        }
        case ExpandedTreeView::CaptionDisplay::Bottom:
            rect.setTop(rect.top() + option.decorationSize.height() + (IconCaptionMargin / 2));
            rect.adjust(margin, 0, -margin, 0);
            break;
        case ExpandedTreeView::CaptionDisplay::None:
            break;
    }

    return rect;
}

int FilterDelegate::iconTextWidth(const QStyleOptionViewItem& option)
{
    const auto* view = qobject_cast<const ExpandedTreeView*>(option.widget);
    if(!view) {
        return -1;
    }

    switch(view->captionDisplay()) {
        case ExpandedTreeView::CaptionDisplay::Right:
            return std::max(0, iconTextRect(option).width());
        case ExpandedTreeView::CaptionDisplay::Bottom:
            return std::max(0, option.decorationSize.width() + IconCaptionMargin);
        case ExpandedTreeView::CaptionDisplay::None:
            break;
    }

    return -1;
}

IconCaptionLineList FilterDelegate::iconRichLines(const QModelIndex& index, const QStyleOptionViewItem& option)
{
    const auto* view = qobject_cast<const ExpandedTreeView*>(option.widget);
    if(!view || view->viewMode() != ExpandedTreeView::ViewMode::Icon) {
        return {};
    }

    const auto captionLines = index.data(FilterItem::IconCaptionLines).value<IconCaptionLineList>();
    IconCaptionLineList lines;
    lines.reserve(captionLines.size());

    const QColor selectedColour = option.palette.color(QPalette::HighlightedText);
    const bool isSelected       = option.state & QStyle::State_Selected;

    for(const auto& captionLine : captionLines) {
        RichText richLine = captionLine.text;
        if(richLine.empty()) {
            continue;
        }

        if(isSelected) {
            richLine = recolourRichText(std::move(richLine), selectedColour);
        }

        IconCaptionLine line;
        line.text      = std::move(richLine);
        line.alignment = captionLine.alignment;
        lines.push_back(std::move(line));
    }

    return lines;
}

QSize FilterDelegate::richLineSize(const QStyleOptionViewItem& option, int maxWidth, const RichText& richText) const
{
    const PreparedTextLine line = prepareTextBlocks(option, maxWidth, richText.blocks);
    return {line.totalWidth, line.maxHeight};
}

RichText FilterDelegate::fallbackRichText(const QStyleOptionViewItem& option, const QModelIndex& index)
{
    const auto* view = qobject_cast<const ExpandedTreeView*>(option.widget);
    if(view && view->viewMode() == ExpandedTreeView::ViewMode::Tree) {
        const auto richText = index.data(FilterItem::RichColumn).value<RichText>();
        if(!richText.empty()) {
            return richText;
        }
    }

    RichText fallback;

    const QString text = option.text;
    if(text.isEmpty()) {
        return fallback;
    }

    RichTextBlock block;
    block.text          = text;
    block.format.font   = option.font;
    block.format.colour = option.palette.color(QPalette::Text);

    fallback.blocks.push_back(std::move(block));
    return fallback;
}

QSize FilterDelegate::richTextSize(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const auto richLines = iconRichLines(index, option);
    if(!richLines.empty()) {
        QSize size;
        const int textWidth = iconTextWidth(option);

        for(const auto& line : richLines) {
            const QSize lineSize
                = richLineSize(option, textWidth > 0 ? textWidth : std::numeric_limits<int>::max(), line.text);

            size.setWidth(std::max(size.width(), lineSize.width()));
            size.setHeight(size.height() + lineSize.height());
        }

        if(textWidth > 0) {
            size.setWidth(textWidth);
        }

        return size;
    }

    const auto richText = fallbackRichText(option, index);
    if(richText.empty()) {
        const QFontMetrics metrics{option.font};
        return metrics.size(Qt::TextSingleLine, option.text);
    }

    QSize size;

    for(const auto& block : richText.blocks) {
        QFont font = block.format.font;
        if(font == QFont{}) {
            font = option.font;
        }

        const QFontMetrics metrics{font};
        const QRect bound = metrics.boundingRect(block.text);
        size.setWidth(size.width() + bound.width());
        size.setHeight(std::max(size.height(), bound.height()));
    }

    return size;
}

QSize FilterDelegate::iconItemSize(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const auto* view = qobject_cast<const ExpandedTreeView*>(option.widget);
    if(!view || view->viewMode() != ExpandedTreeView::ViewMode::Icon) {
        return {};
    }

    const QStyle* style  = option.widget ? option.widget->style() : QApplication::style();
    const QSize textSize = richTextSize(option, index);

    const int horizontalMargin{12};
    const int verticalMargin = std::max(2, style->pixelMetric(QStyle::PM_FocusFrameHMargin, &option, option.widget));

    switch(view->captionDisplay()) {
        case ExpandedTreeView::CaptionDisplay::Bottom:
            return {std::max(option.decorationSize.width(), textSize.width() + horizontalMargin),
                    option.decorationSize.height() + (IconCaptionMargin / 2) + textSize.height() + verticalMargin};
        case ExpandedTreeView::CaptionDisplay::Right:
            return {option.rect.width() > 0
                        ? option.rect.width()
                        : option.decorationSize.width() + IconCaptionMargin + textSize.width() + horizontalMargin,
                    std::max(option.decorationSize.height(), textSize.height() + verticalMargin)};
        case ExpandedTreeView::CaptionDisplay::None:
            return option.decorationSize;
    }

    return {};
}

void FilterDelegate::setupFilterOption(QStyleOptionViewItem* option, const QModelIndex& index)
{
    if(!option) {
        return;
    }

    const auto* view = qobject_cast<const ExpandedTreeView*>(option->widget);
    if(!view || view->viewMode() != ExpandedTreeView::ViewMode::Icon) {
        return;
    }

    const QString iconLabel = index.data(FilterItem::IconLabel).toString();
    if(!iconLabel.isNull()) {
        option->text = iconLabel;
    }
}

void FilterDelegate::drawRichTextLines(QPainter* painter, const QStyleOptionViewItem& option, QRect rect,
                                       const IconCaptionLineList& lines)
{
    if(lines.empty() || rect.width() <= 0 || rect.height() <= 0) {
        return;
    }

    const auto* view   = qobject_cast<const ExpandedTreeView*>(option.widget);
    const int maxWidth = rect.width();

    int totalHeight{0};
    std::vector<PreparedTextLine> preparedLines;
    preparedLines.reserve(lines.size());

    for(const auto& line : lines) {
        auto prepared = prepareTextBlocks(option, maxWidth, line.text.blocks);
        totalHeight += prepared.maxHeight;
        preparedLines.push_back(std::move(prepared));
    }

    int y = rect.y();
    if(view && view->captionDisplay() == ExpandedTreeView::CaptionDisplay::Right && totalHeight < rect.height()) {
        y += (rect.height() - totalHeight) / 2;
    }

    for(size_t i{0}; i < lines.size(); ++i) {
        const auto& prepared = preparedLines.at(i);

        const QRect lineRect{rect.x(), y, rect.width(), prepared.maxHeight};
        drawPreparedTextLine(painter, option, lineRect, lines.at(i).alignment, prepared);
        y += prepared.maxHeight;

        if(y >= rect.bottom()) {
            break;
        }
    }
}

void FilterDelegate::drawTextBlocks(QPainter* painter, const QStyleOptionViewItem& option, QRect rect,
                                    const std::vector<RichTextBlock>& blocks)
{
    const QStyle* style = option.widget ? option.widget->style() : QApplication::style();

    const auto colourRole = option.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::NoRole;
    const PreparedTextLine preparedLine = prepareTextBlocks(option, rect.width(), blocks);

    if(preparedLine.blocks.empty()) {
        return;
    }

    int x = alignedLineX(rect, preparedLine.totalWidth, option.displayAlignment);

    for(const auto& block : preparedLine.blocks) {
        painter->setFont(block.font);
        painter->setPen(block.colour);

        const QRect blockRect{x, rect.y(), std::max(0, rect.right() - x + 1), rect.height()};
        style->drawItemText(painter, blockRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, option.palette,
                            true, block.text, colourRole);

        x += block.width;
    }
}
} // namespace Fooyin::Filters

#include "moc_filterdelegate.cpp"
