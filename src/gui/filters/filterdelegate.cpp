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

#include <gui/guiutils.h>
#include <gui/scripting/richtext.h>
#include <gui/scripting/richtextutils.h>
#include <gui/widgets/expandedtreeview.h>
#include <gui/widgets/widgetbackground.h>

#include <QApplication>
#include <QPainter>

#include <cmath>

constexpr auto IconCaptionMargin     = 20;
constexpr auto RightCaptionTextWidth = 180;

namespace Fooyin::Filters {
namespace {
struct PreparedTextBlock
{
    QString text;
    QFont font;
    QColor colour;
    int width{0};
};

struct PreparedTextLine
{
    std::vector<PreparedTextBlock> blocks;
    int totalWidth{0};
    TextBaselineMetrics baseline;
    int maxHeight{0};
};

using RichTextLineList = std::vector<RichText>;

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

QColor selectionTextColour(const QStyleOptionViewItem& option)
{
    const auto* view = qobject_cast<const ExpandedTreeView*>(option.widget);
    if(view && view->viewMode() == ExpandedTreeView::ViewMode::Icon
       && view->captionDisplay() == ExpandedTreeView::CaptionDisplay::Overlay) {
        return QApplication::palette().color(option.palette.currentColorGroup(), QPalette::HighlightedText);
    }
    return option.palette.color(Gui::itemViewSelectionTextRole(option));
}

PreparedTextLine prepareTextBlocks(const QStyleOptionViewItem& option, int maxWidth,
                                   const std::vector<RichTextBlock>& blocks)
{
    PreparedTextLine result;
    result.baseline = textBaselineMetrics(option.font);

    const auto selectedColour = selectionTextColour(option);
    const auto defaultColour  = option.palette.color(QPalette::Text);
    const auto linkColour     = option.palette.color(QPalette::Link);
    int remainingWidth        = maxWidth;

    for(const auto& block : blocks) {
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

        remainingWidth -= prepared.width;
        result.totalWidth += prepared.width;
        result.baseline.expand(metrics);
        result.blocks.push_back(std::move(prepared));

        if(text != block.text) {
            break;
        }
    }

    result.maxHeight = result.baseline.height();

    return result;
}

void drawPreparedTextLine(QPainter* painter, QRect rect, Qt::Alignment alignment, const PreparedTextLine& line)
{
    if(line.blocks.empty() || rect.width() <= 0 || rect.height() <= 0) {
        return;
    }

    int x = alignedLineX(rect, line.totalWidth, alignment);

    for(const auto& block : line.blocks) {
        painter->setFont(block.font);
        painter->setPen(block.colour);
        painter->drawText(QPoint{x, rect.y() + line.baseline.ascent}, block.text);

        x += block.width;
    }
}

QSize richLineSize(const QStyleOptionViewItem& option, int maxWidth, const RichText& richText)
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
} // namespace

void FilterDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);
    setupFilterOption(&opt, index);

    // initStyleOption will override decorationSize when converting the QPixmap to a QIcon, so restore it here
    opt.decorationSize  = option.decorationSize;
    const QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();

    const auto* view          = qobject_cast<const ExpandedTreeView*>(opt.widget);
    const bool iconMode       = view && view->viewMode() == ExpandedTreeView::ViewMode::Icon;
    const bool hasArtwork     = iconMode && !opt.icon.isNull();
    const bool overlayCaption = iconMode && view->captionDisplay() == ExpandedTreeView::CaptionDisplay::Overlay;
    const QIcon artworkIcon   = hasArtwork ? opt.icon : QIcon{};

    if(view && view->viewMode() != ExpandedTreeView::ViewMode::Flow) {
        WidgetBackground::clearDelegateBackground(opt);
    }

    Qt::Edges squareEdges;
    if(iconMode) {
        if(view->captionDisplay() == ExpandedTreeView::CaptionDisplay::Bottom) {
            squareEdges = Qt::BottomEdge;
        }
        else if(view->captionDisplay() == ExpandedTreeView::CaptionDisplay::Right) {
            squareEdges = opt.direction == Qt::RightToLeft ? Qt::LeftEdge : Qt::RightEdge;
        }
    }

    const int backgroundRadius = iconMode ? view->iconItemCornerRadius() : 0;

    const auto drawBackground = [&](QStyleOptionViewItem backgroundOpt) {
        painter->save();

        if(backgroundRadius > 0) {
            Gui::clipRoundedRect(*painter, backgroundOpt.rect, backgroundRadius, squareEdges);
        }
        if(!(backgroundOpt.state & QStyle::State_Selected) && backgroundOpt.backgroundBrush.style() != Qt::NoBrush) {
            painter->fillRect(backgroundOpt.rect, backgroundOpt.backgroundBrush);
            backgroundOpt.backgroundBrush = Qt::NoBrush;
        }
        Gui::drawRoundedStyleControl(painter, QStyle::CE_ItemViewItem, backgroundOpt, backgroundOpt.widget,
                                     backgroundRadius, squareEdges);

        painter->restore();
    };

    const QRect decorationRect = hasArtwork ? artworkRect(opt) : QRect{};
    QPixmap scaledArtwork;
    if(hasArtwork) {
        const double dpr = opt.widget ? opt.widget->devicePixelRatioF() : 1.0;
        scaledArtwork    = artworkIcon.pixmap(decorationRect.size(), dpr, Gui::itemViewIconMode(opt));
    }

    const auto drawArtwork = [&]() {
        if(hasArtwork) {
            painter->save();
            const Qt::Alignment alignment
                = iconMode && view->captionDisplay() == ExpandedTreeView::CaptionDisplay::Bottom
                    ? Qt::AlignHCenter | Qt::AlignBottom
                    : Qt::AlignCenter;
            Gui::drawRoundedPixmap(*painter, decorationRect, alignment, scaledArtwork, m_artworkCornerRadius);
            painter->restore();
        }
    };

    const bool richTreeMode = view && view->viewMode() == ExpandedTreeView::ViewMode::Tree;
    const auto richText     = richTreeMode ? fallbackRichText(opt, index) : RichText{};

    IconCaptionLineList richLines;
    if(!richTreeMode) {
        richLines = iconRichLines(index, opt);
    }

    if(!richText.empty()) {
        painter->save();

        QStyleOptionViewItem backgroundOpt{opt};
        if(opt.state & QStyle::State_Selected) {
            backgroundOpt.state &= ~QStyle::State_Selected;
            backgroundOpt.backgroundBrush = Qt::NoBrush;
        }

        backgroundOpt.text.clear();
        if(hasArtwork) {
            backgroundOpt.icon = {};
        }

        drawBackground(backgroundOpt);
        drawArtwork();

        const QRect textRect = Gui::itemViewTextRect(opt);
        drawTextBlocks(painter, opt, textRect, richText.blocks);

        painter->restore();
        return;
    }

    if(!richLines.empty()) {
        painter->save();

        QStyleOptionViewItem backgroundOpt{opt};
        backgroundOpt.text.clear();
        if(hasArtwork) {
            backgroundOpt.icon = {};
        }
        drawBackground(backgroundOpt);

        if(overlayCaption) {
            drawOverlayItem(painter, opt, scaledArtwork, richLines);
        }
        else {
            drawArtwork();
            drawRichTextLines(painter, opt, iconTextRect(opt), richLines, {});
        }

        painter->restore();
        return;
    }

    if(hasArtwork) {
        opt.icon = {};
    }
    if(iconMode) {
        drawBackground(opt);
    }
    else {
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
    }
    drawArtwork();
}

QSize FilterDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt{option};
    const auto* view = qobject_cast<const ExpandedTreeView*>(option.widget);

    // In icon mode ExpandedTreeView asks sizeHint() for every row during layout. Use a
    // layout-only option setup here so we don't fetch Qt::DecorationRole
    // and eagerly start cover loads for every row.
    if(view && view->viewMode() == ExpandedTreeView::ViewMode::Icon) {
        initLayoutOnlyOption(&opt, index);
    }
    else {
        initStyleOption(&opt, index);
        setupFilterOption(&opt, index);
    }

    opt.decorationSize = option.decorationSize;

    const bool iconMode  = view && view->viewMode() == ExpandedTreeView::ViewMode::Icon;
    const QSize textSize = iconMode ? QSize{} : richTextSize(opt, index);
    QSize size           = iconMode ? iconItemSize(opt, index) : Gui::itemViewTextSizeHint(opt, textSize);

    const QSize sizeHint = index.data(Qt::SizeHintRole).toSize();
    if(sizeHint.height() > 0) {
        size.setHeight(sizeHint.height());
    }

    return size;
}

void FilterDelegate::setArtworkCornerRadius(int radius)
{
    m_artworkCornerRadius = std::clamp(radius, 0, 100);
}

void FilterDelegate::setArtworkMargins(const QMargins& margins)
{
    m_artworkMargins = margins;
}

RichText FilterDelegate::recolourRichText(RichText richText, const QColor& colour)
{
    if(!colour.isValid()) {
        return richText;
    }

    for(auto& block : richText.blocks) {
        block.format.colour.setColour(colour);
    }

    return richText;
}

QRect FilterDelegate::artworkRect(const QStyleOptionViewItem& option) const
{
    const auto* view = qobject_cast<const ExpandedTreeView*>(option.widget);

    QStyleOptionViewItem artworkOpt{option};
    artworkOpt.rect = option.rect.marginsRemoved(m_artworkMargins);

    if(view && view->captionDisplay() == ExpandedTreeView::CaptionDisplay::Right) {
        return QStyle::alignedRect(option.direction, Qt::AlignLeft | Qt::AlignVCenter, option.decorationSize,
                                   artworkOpt.rect);
    }

    return QStyle::alignedRect(option.direction, Qt::AlignTop | Qt::AlignHCenter, option.decorationSize,
                               artworkOpt.rect);
}

QRect FilterDelegate::iconTextRect(const QStyleOptionViewItem& option) const
{
    const auto* view = qobject_cast<const ExpandedTreeView*>(option.widget);
    if(!view) {
        return option.rect;
    }

    const QStyle* style = option.widget ? option.widget->style() : QApplication::style();
    QRect rect          = option.rect;
    const int margin    = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &option, option.widget) + 1;

    switch(view->captionDisplay()) {
        case ExpandedTreeView::CaptionDisplay::Right: {
            rect.adjust(margin, 0, -margin, 0);
            const QRect artwork = artworkRect(option);
            if(option.direction == Qt::RightToLeft) {
                rect.setRight(artwork.left() - m_artworkMargins.left() - margin - 1);
            }
            else {
                rect.setLeft(artwork.right() + m_artworkMargins.right() + margin + 1);
            }
            break;
        }
        case ExpandedTreeView::CaptionDisplay::Bottom: {
            const int verticalMargin
                = std::max(2, style->pixelMetric(QStyle::PM_FocusFrameVMargin, &option, option.widget));
            rect.setTop(rect.top() + m_artworkMargins.top() + option.decorationSize.height() + m_artworkMargins.bottom()
                        + verticalMargin);
            rect.adjust(0, 0, 0, -verticalMargin);
            const int horizontalMargin = std::max(4, margin);
            rect.adjust(horizontalMargin, 0, -horizontalMargin, 0);
            break;
        }
        case ExpandedTreeView::CaptionDisplay::Overlay:
            rect = artworkRect(option).adjusted(std::max(4, margin), 0, -std::max(4, margin), 0);
            break;
        case ExpandedTreeView::CaptionDisplay::None:
            break;
    }

    return rect;
}

int FilterDelegate::iconTextWidth(const QStyleOptionViewItem& option) const
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
        case ExpandedTreeView::CaptionDisplay::Bottom:
        case ExpandedTreeView::CaptionDisplay::Overlay: {
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

    const QColor selectedColour = selectionTextColour(option);
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

    const int verticalMargin  = std::max(2, style->pixelMetric(QStyle::PM_FocusFrameVMargin, &option, option.widget));
    const int verticalPadding = 2 * verticalMargin;
    const QSize artworkSize
        = option.decorationSize
        + QSize{m_artworkMargins.left() + m_artworkMargins.right(), m_artworkMargins.top() + m_artworkMargins.bottom()};

    switch(view->captionDisplay()) {
        case ExpandedTreeView::CaptionDisplay::Bottom: {
            return {artworkSize.width(), artworkSize.height() + textSize.height() + verticalPadding};
        }
        case ExpandedTreeView::CaptionDisplay::Right: {
            const int itemWidth  = option.rect.width() > 0
                                     ? option.rect.width()
                                     : artworkSize.width() + IconCaptionMargin + RightCaptionTextWidth;
            const int itemHeight = std::max(artworkSize.height(), textSize.height()) + verticalPadding;
            return {itemWidth, itemHeight};
        }
        case ExpandedTreeView::CaptionDisplay::None:
        case ExpandedTreeView::CaptionDisplay::Overlay:
            return artworkSize;
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

void FilterDelegate::drawOverlayItem(QPainter* painter, QStyleOptionViewItem option, const QPixmap& artwork,
                                     const IconCaptionLineList& lines) const
{
    const QRect decorationRect = artworkRect(option);
    const QRectF imageRect     = artwork.isNull()
                                   ? decorationRect
                                   : QStyle::alignedRect(Qt::LeftToRight, Qt::AlignHCenter | Qt::AlignBottom,
                                                         artwork.deviceIndependentSize().toSize(), decorationRect);

    const qreal dpr = option.widget ? option.widget->devicePixelRatioF() : 1.0;
    QPixmap layer{QSize{static_cast<int>(std::ceil(option.rect.width() * dpr)),
                        static_cast<int>(std::ceil(option.rect.height() * dpr))}};
    layer.setDevicePixelRatio(dpr);
    layer.fill(Qt::transparent);
    {
        QPainter layerPainter{&layer};
        layerPainter.setRenderHints(painter->renderHints());
        layerPainter.translate(-option.rect.topLeft());
        layerPainter.drawPixmap(imageRect.topLeft(), artwork);

        for(const auto role : {QPalette::Text, QPalette::Link}) {
            if(option.palette.color(role).lightness() < 128) {
                option.palette.setColor(role, Qt::white);
            }
        }
        const int margin     = std::max(4, Gui::itemViewTextMargin(option.widget, &option));
        const QRect textRect = imageRect.adjusted(margin, 0, -margin, 0).toAlignedRect();
        drawRichTextLines(&layerPainter, option, textRect, lines, imageRect);
    }

    // Mask artwork and captions together
    Gui::drawRoundedPixmap(*painter, imageRect, option.rect.topLeft(), layer, m_artworkCornerRadius);
}

void FilterDelegate::drawRichTextLines(QPainter* painter, const QStyleOptionViewItem& option, QRect rect,
                                       const IconCaptionLineList& lines, const QRectF& artwork) const
{
    if(lines.empty() || rect.width() <= 0 || rect.height() <= 0) {
        return;
    }

    const auto* view          = qobject_cast<const ExpandedTreeView*>(option.widget);
    const bool overlayCaption = view && view->captionDisplay() == ExpandedTreeView::CaptionDisplay::Overlay;
    const qreal radius
        = overlayCaption ? std::min(artwork.width(), artwork.height()) * m_artworkCornerRadius / 200.0 : 0.0;
    const int maxWidth = rect.width();

    struct CaptionLine
    {
        PreparedTextLine prepared;
        Qt::Alignment alignment;
        RichText text;
    };

    int totalHeight{0};
    std::vector<CaptionLine> preparedLines;

    for(const auto& line : lines) {
        auto logicalLines = splitRichTextLines(line.text);
        if(logicalLines.empty()) {
            PreparedTextLine prepared;
            prepared.baseline  = textBaselineMetrics(option.font);
            prepared.maxHeight = prepared.baseline.height();
            totalHeight += prepared.maxHeight;
            preparedLines.push_back({.prepared = std::move(prepared), .alignment = line.alignment, .text = {}});
            continue;
        }

        for(auto& logicalLine : logicalLines) {
            auto prepared = prepareTextBlocks(option, maxWidth, logicalLine.blocks);
            totalHeight += prepared.maxHeight;
            preparedLines.push_back(
                {.prepared = std::move(prepared), .alignment = line.alignment, .text = std::move(logicalLine)});
        }
    }

    painter->save();

    if(overlayCaption) {
        const QStyle* style = option.widget ? option.widget->style() : QApplication::style();

        const int padding = std::max(2, style->pixelMetric(QStyle::PM_FocusFrameVMargin, &option, option.widget));
        const QRectF visibleArtwork = artwork.intersected(QRectF{artworkRect(option)});
        const qreal height = std::min(visibleArtwork.height(), static_cast<qreal>(totalHeight + (2 * padding)));

        const QRectF background{visibleArtwork.left(), visibleArtwork.bottom() - height, visibleArtwork.width(),
                                height};
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setClipRect(visibleArtwork, Qt::IntersectClip);

        const bool selected     = option.state & QStyle::State_Selected;
        QColor backgroundColour = selected ? option.palette.color(QPalette::Highlight) : QColor{Qt::black};
        backgroundColour.setAlpha(selected ? 230 : 180);

        painter->fillRect(background, backgroundColour);
        if(!selected && option.state & QStyle::State_MouseOver) {
            QStyleOptionViewItem hoverOpt{option};
            hoverOpt.rect            = background.toAlignedRect();
            hoverOpt.backgroundBrush = Qt::NoBrush;
            hoverOpt.palette.setBrush(QPalette::Base, Qt::NoBrush);
            hoverOpt.features.setFlag(QStyleOptionViewItem::Alternate, false);
            style->drawPrimitive(QStyle::PE_PanelItemViewItem, &hoverOpt, painter, option.widget);
        }
        rect.setTop(static_cast<int>(std::ceil(background.top() + padding)));
        rect.setBottom(static_cast<int>(std::ceil(background.bottom() - padding)) - 1);
        painter->setClipRect(rect, Qt::IntersectClip);
    }

    int y = rect.y();
    if(view && view->captionDisplay() == ExpandedTreeView::CaptionDisplay::Right && totalHeight < rect.height()) {
        y += (rect.height() - totalHeight) / 2;
    }

    for(const auto& [prepared, alignment, text] : preparedLines) {
        if(y > rect.bottom()) {
            break;
        }

        QRect lineRect{rect.x(), y, rect.width(), prepared.maxHeight};
        if(radius > 0.0) {
            // Elide against the narrowest part of the curve spanned by this line
            // TODO: Add option to adjust height of inline captions? Otherwise, text at the bottom won't be usable
            //       with circular icons configured
            const qreal distance
                = std::clamp(std::max(artwork.top() + radius - lineRect.top(),
                                      lineRect.bottom() + 1 - (artwork.y() + artwork.height() - radius)),
                             0.0, radius);
            const auto inset
                = static_cast<int>(std::ceil(radius - std::sqrt((radius * radius) - (distance * distance))));
            lineRect.adjust(inset, 0, -inset, 0);

            auto fittedLine     = prepareTextBlocks(option, lineRect.width(), text.blocks);
            fittedLine.baseline = prepared.baseline;
            drawPreparedTextLine(painter, lineRect, alignment, fittedLine);
        }
        else {
            drawPreparedTextLine(painter, lineRect, alignment, prepared);
        }
        y += prepared.maxHeight;
    }

    painter->restore();
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

        drawPreparedTextLine(painter, {rect.x(), y, rect.width(), preparedLine.maxHeight}, option.displayAlignment,
                             preparedLine);
        y += preparedLine.maxHeight;
    }
}
} // namespace Fooyin::Filters

#include "moc_filterdelegate.cpp"
