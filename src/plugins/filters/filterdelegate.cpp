/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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
#include <gui/scripting/richtextutils.h>
#include <gui/widgets/expandedtreeview.h>

#include <QApplication>
#include <QPainter>

constexpr auto IconCaptionMargin     = 20;
constexpr auto RightCaptionTextWidth = 180;

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

using RichTextLineList = std::vector<Fooyin::RichText>;

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
    const auto linkColour     = option.palette.color(QPalette::Link);
    int remainingWidth        = maxWidth;

    for(const auto& block : blocks) {
        if(block.text.isEmpty() || remainingWidth <= 0) {
            continue;
        }

        const QFont font = Fooyin::resolvedRichTextFont(block.format, option.font);
        QColor colour    = Fooyin::resolvedRichTextColour(block.format, defaultColour, linkColour);
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

    if(result.maxHeight <= 0) {
        result.maxHeight = option.fontMetrics.height();
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
struct IconItemLayoutMetrics
{
    int horizontalMargin{12};
    int verticalPadding{0};
};

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
    const auto* view = qobject_cast<const ExpandedTreeView*>(option.widget);

    // In icon mode ExpandedTreeView asks sizeHint() for every row during layout. Use a
    // layout-only option setup here so we don't fetch Qt::DecorationRole
    // and eagerly start cover loads for everyrow.
    if(view && view->viewMode() == ExpandedTreeView::ViewMode::Icon) {
        initLayoutOnlyOption(&opt, index);
    }
    else {
        initStyleOption(&opt, index);
        setupFilterOption(&opt, index);
    }

    opt.decorationSize = option.decorationSize;

    const QStyle* style  = opt.widget ? opt.widget->style() : QApplication::style();
    const bool iconMode  = view && view->viewMode() == ExpandedTreeView::ViewMode::Icon;
    const QSize textSize = iconMode ? QSize{} : richTextSize(opt, index);
    QSize size           = iconMode ? iconItemLayoutSize(opt)
                                    : style->sizeFromContents(QStyle::CT_ItemViewItem, &opt, textSize, opt.widget);

    if(!iconMode) {
        const int verticalPadding = std::max(2, style->pixelMetric(QStyle::PM_FocusFrameHMargin, &opt, opt.widget));
        size.setHeight(std::max(size.height(), textSize.height() + (2 * verticalPadding)));
    }

    const QSize sizeHint = index.data(Qt::SizeHintRole).toSize();
    if(sizeHint.height() > 0) {
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
        case ExpandedTreeView::CaptionDisplay::Right: {
            const int width = iconTextRect(option).width();
            return width > 0 ? width : RightCaptionTextWidth;
        }
        case ExpandedTreeView::CaptionDisplay::Bottom: {
            const int width = iconTextRect(option).width();
            return width > 0 ? width : -1;
        }
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
    QSize size{0, 0};
    const auto lines = splitRichTextLines(richText);

    for(const auto& line : lines) {
        const PreparedTextLine preparedLine = prepareTextBlocks(option, maxWidth, line.blocks);
        size.setWidth(std::max(size.width(), preparedLine.totalWidth));
        size.rheight() += preparedLine.maxHeight;
    }

    return size;
}

QSize richTextNaturalSize(const QStyleOptionViewItem& option, const RichText& richText)
{
    const auto metrics = measureRichText(richText, option.font);
    return {metrics.width, metrics.firstLineHeight + metrics.extraLineHeight};
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
    block.text        = text;
    block.format.font = option.font;

    fallback.blocks.push_back(std::move(block));
    return fallback;
}

QSize FilterDelegate::richTextSize(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const auto richLines = iconRichLines(index, option);
    if(!richLines.empty()) {
        QSize size{0, 0};
        const auto* view    = qobject_cast<const ExpandedTreeView*>(option.widget);
        const int textWidth = iconTextWidth(option);
        const bool constrainWidth
            = view && view->captionDisplay() == ExpandedTreeView::CaptionDisplay::Right && textWidth > 0;

        for(const auto& line : richLines) {
            const QSize lineSize
                = constrainWidth ? richLineSize(option, textWidth, line.text) : richTextNaturalSize(option, line.text);

            size.setWidth(std::max(size.width(), lineSize.width()));
            size.setHeight(size.height() + lineSize.height());
        }

        if(constrainWidth) {
            size.setWidth(textWidth);
        }

        return size;
    }

    const auto richText = fallbackRichText(option, index);
    if(richText.empty()) {
        const QFontMetrics metrics{option.font};
        return metrics.size(Qt::TextSingleLine, option.text);
    }

    return richTextNaturalSize(option, richText);
}

QSize FilterDelegate::iconItemSize(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const auto* view = qobject_cast<const ExpandedTreeView*>(option.widget);
    if(!view || view->viewMode() != ExpandedTreeView::ViewMode::Icon) {
        return {};
    }

    const QStyle* style  = option.widget ? option.widget->style() : QApplication::style();
    const QSize textSize = richTextSize(option, index);

    const int verticalMargin = std::max(2, style->pixelMetric(QStyle::PM_FocusFrameVMargin, &option, option.widget));
    const IconItemLayoutMetrics metrics{.verticalPadding = 2 * verticalMargin};

    return iconItemSizeForText(option, metrics, textSize);
}

QSize FilterDelegate::iconItemLayoutSize(const QStyleOptionViewItem& option)
{
    const auto* view = qobject_cast<const ExpandedTreeView*>(option.widget);
    if(!view || view->viewMode() != ExpandedTreeView::ViewMode::Icon) {
        return {};
    }

    const QStyle* style = option.widget ? option.widget->style() : QApplication::style();

    const int verticalMargin = std::max(2, style->pixelMetric(QStyle::PM_FocusFrameVMargin, &option, option.widget));
    const IconItemLayoutMetrics metrics{.verticalPadding = 2 * verticalMargin};

    const int lineCount  = option.text.isEmpty() ? 0 : option.text.count(QChar::LineSeparator) + 1;
    const int textHeight = lineCount * option.fontMetrics.height();
    int textWidth        = 0;
    if(view->captionDisplay() == ExpandedTreeView::CaptionDisplay::Right) {
        textWidth = RightCaptionTextWidth;
    }
    else if(lineCount > 0) {
        textWidth = option.decorationSize.width() + metrics.horizontalMargin;
    }

    return iconItemSizeForText(option, metrics, {textWidth, textHeight});
}

QSize FilterDelegate::iconItemSizeForText(const QStyleOptionViewItem& option, const IconItemLayoutMetrics& metrics,
                                          const QSize& textSize)
{
    const auto* view = qobject_cast<const ExpandedTreeView*>(option.widget);
    if(!view || view->viewMode() != ExpandedTreeView::ViewMode::Icon) {
        return {};
    }

    const QStyle* style = option.widget ? option.widget->style() : QApplication::style();
    switch(view->captionDisplay()) {
        case ExpandedTreeView::CaptionDisplay::Bottom: {
            return {std::max(option.decorationSize.width(), textSize.width() + metrics.horizontalMargin),
                    option.decorationSize.height() + (IconCaptionMargin / 2) + textSize.height()
                        + metrics.verticalPadding};
        }
        case ExpandedTreeView::CaptionDisplay::Right: {
            const QSize styleSize = style->sizeFromContents(QStyle::CT_ItemViewItem, &option, textSize, option.widget);
            const int itemWidth   = option.rect.width() > 0 ? option.rect.width()
                                                            : option.decorationSize.width() + IconCaptionMargin
                                                                  + textSize.width() + metrics.horizontalMargin;
            const int itemHeight
                = std::max({styleSize.height(), option.decorationSize.height() + metrics.verticalPadding,
                            textSize.height() + metrics.verticalPadding});
            return {itemWidth, itemHeight};
        }
        case ExpandedTreeView::CaptionDisplay::None:
            return {option.decorationSize.width(), option.decorationSize.height() + metrics.verticalPadding};
    }

    return {};
}

void FilterDelegate::initLayoutOnlyOption(QStyleOptionViewItem* option, const QModelIndex& index) const
{
    if(!option) {
        return;
    }

    option->index = index;
    option->features &= ~(QStyleOptionViewItem::HasDisplay | QStyleOptionViewItem::HasDecoration
                          | QStyleOptionViewItem::HasCheckIndicator);
    option->text.clear();
    option->icon = {};

    const QVariant fontRole = index.data(Qt::FontRole);
    if(fontRole.isValid() && !fontRole.isNull()) {
        option->font        = qvariant_cast<QFont>(fontRole).resolve(option->font);
        option->fontMetrics = QFontMetrics(option->font);
    }

    const QVariant foregroundRole = index.data(Qt::ForegroundRole);
    if(foregroundRole.canConvert<QBrush>()) {
        option->palette.setBrush(QPalette::Text, qvariant_cast<QBrush>(foregroundRole));
    }

    const QVariant alignmentRole = index.data(Qt::TextAlignmentRole);
    if(alignmentRole.isValid() && !alignmentRole.isNull()) {
        option->displayAlignment = static_cast<Qt::Alignment>(alignmentRole.toInt());
    }

    const QVariant checkStateRole = index.data(Qt::CheckStateRole);
    if(checkStateRole.isValid() && !checkStateRole.isNull()) {
        option->features |= QStyleOptionViewItem::HasCheckIndicator;
        option->checkState = static_cast<Qt::CheckState>(checkStateRole.toInt());
    }

    const QVariant displayRole = index.data(Qt::DisplayRole);
    if(displayRole.isValid() && !displayRole.isNull()) {
        option->text = displayText(displayRole, option->locale);
        if(!option->text.isEmpty()) {
            option->features |= QStyleOptionViewItem::HasDisplay;
        }
    }

    const QVariant backgroundRole = index.data(Qt::BackgroundRole);
    option->backgroundBrush       = qvariant_cast<QBrush>(backgroundRole);

    option->features |= QStyleOptionViewItem::HasDecoration;
    setupFilterOption(option, index);
    option->styleObject = nullptr;
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
    std::vector<std::pair<PreparedTextLine, Qt::Alignment>> preparedLines;

    for(const auto& line : lines) {
        const auto logicalLines = splitRichTextLines(line.text);
        if(logicalLines.empty()) {
            PreparedTextLine prepared;
            prepared.maxHeight = option.fontMetrics.height();
            totalHeight += prepared.maxHeight;
            preparedLines.emplace_back(std::move(prepared), line.alignment);
            continue;
        }

        for(const auto& logicalLine : logicalLines) {
            auto prepared = prepareTextBlocks(option, maxWidth, logicalLine.blocks);
            totalHeight += prepared.maxHeight;
            preparedLines.emplace_back(std::move(prepared), line.alignment);
        }
    }

    int y = rect.y();
    if(view && view->captionDisplay() == ExpandedTreeView::CaptionDisplay::Right && totalHeight < rect.height()) {
        y += (rect.height() - totalHeight) / 2;
    }

    for(const auto& [prepared, alignment] : preparedLines) {
        if(y > rect.bottom()) {
            break;
        }

        const QRect lineRect{rect.x(), y, rect.width(), prepared.maxHeight};
        drawPreparedTextLine(painter, option, lineRect, alignment, prepared);
        y += prepared.maxHeight;
    }
}

void FilterDelegate::drawTextBlocks(QPainter* painter, const QStyleOptionViewItem& option, QRect rect,
                                    const std::vector<RichTextBlock>& blocks)
{
    RichText richText;
    richText.blocks = blocks;

    const auto logicalLines = splitRichTextLines(richText);
    if(logicalLines.empty()) {
        return;
    }

    std::vector<PreparedTextLine> preparedLines;
    preparedLines.reserve(logicalLines.size());

    int totalHeight{0};
    for(const auto& logicalLine : logicalLines) {
        auto preparedLine = prepareTextBlocks(option, rect.width(), logicalLine.blocks);
        totalHeight += preparedLine.maxHeight;
        preparedLines.push_back(std::move(preparedLine));
    }

    int y = rect.y();
    if(totalHeight < rect.height()) {
        y += (rect.height() - totalHeight) / 2;
    }

    for(const auto& preparedLine : preparedLines) {
        if(y > rect.bottom()) {
            break;
        }

        drawPreparedTextLine(painter, option, {rect.x(), y, rect.width(), preparedLine.maxHeight},
                             option.displayAlignment, preparedLine);
        y += preparedLine.maxHeight;
    }
}
} // namespace Fooyin::Filters

#include "moc_filterdelegate.cpp"
