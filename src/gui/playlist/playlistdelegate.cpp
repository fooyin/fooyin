/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include "playlistdelegate.h"

#include "playlistitem.h"

#include <QApplication>
#include <QPainter>

namespace Fooyin {
struct DrawTextResult
{
    QRect bound;
    int totalWidth{0};
};

template <typename Range>
DrawTextResult drawTextBlocks(QPainter* painter, const QStyleOptionViewItem& option, QRect rect, const Range& blocks,
                              Qt::Alignment alignment)
{
    DrawTextResult result;

    const auto colour = option.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::NoRole;

    for(const auto& block : blocks) {
        painter->setFont(block.format.font);
        painter->setPen(block.format.colour);
        result.bound = painter->boundingRect(rect, alignment | Qt::TextWrapAnywhere, block.text);
        option.widget->style()->drawItemText(
            painter, rect, alignment, option.palette, true,
            painter->fontMetrics().elidedText(block.text, Qt::ElideRight, rect.width()), colour);

        if(alignment & Qt::AlignRight) {
            rect.moveRight((rect.x() + rect.width()) - result.bound.width());
        }
        else {
            rect.setWidth(rect.width() - result.bound.width());
            rect.moveLeft(rect.x() + result.bound.width());
        }
        result.totalWidth += result.bound.width();
    }

    return result;
}

void paintHeader(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    QStyleOptionViewItem opt{option};
    opt.text.clear();
    opt.icon = {};

    QStyle* style = opt.widget ? opt.widget->style() : qApp->style();

    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    QPen linePen = painter->pen();
    linePen.setWidth(1);
    QColor lineColour = opt.palette.color(QPalette::Text);
    lineColour.setAlpha(40);
    linePen.setColor(lineColour);

    const auto title    = index.data(PlaylistItem::Role::Title).value<RichText>();
    const auto subtitle = index.data(PlaylistItem::Role::Subtitle).value<RichText>();
    const auto side     = index.data(PlaylistItem::Role::Right).value<RichText>();
    const auto info     = index.data(PlaylistItem::Role::Info).value<RichText>();
    const auto cover    = index.data(Qt::DecorationRole).value<QPixmap>();

    const QRect& rect    = opt.rect;
    const int halfWidth  = rect.width() / 2;
    constexpr int offset = 5;

    constexpr int coverMargin     = 10;
    const int coverSize           = rect.height() - (2 * coverMargin);
    constexpr int coverFrameWidth = 2;
    const int coverFrameOffset    = coverFrameWidth / 2;

    const QRect coverRect{rect.left() + coverMargin, rect.top() + coverMargin, coverSize, coverSize};
    QRect coverFrameRect = coverRect.adjusted(-coverFrameOffset, -coverFrameOffset, coverFrameWidth, coverFrameWidth);

    if(cover.isNull()) {
        coverFrameRect.setWidth(0);
    }

    const auto titleOffset = static_cast<int>(rect.height() * 0.08);
    const auto infoOffset  = static_cast<int>(rect.height() * 0.12);

    const QRect rightRect{rect.left() + halfWidth, rect.top(), halfWidth - offset, rect.height()};
    const auto [rightBound, totalRightWidth]
        = drawTextBlocks(painter, opt, rightRect, side | std::views::reverse, Qt::AlignVCenter | Qt::AlignRight);

    const int leftWidth = rect.width() - coverFrameRect.width() - totalRightWidth;

    QRect subtitleRect{coverFrameRect.right() + (2 * offset), rect.top(), leftWidth, rect.height()};
    if(totalRightWidth > 0) {
        subtitleRect.setWidth(subtitleRect.width() - (5 * offset));
    }
    const auto [subtitleBound, _]
        = drawTextBlocks(painter, opt, subtitleRect, subtitle, Qt::AlignVCenter | Qt::AlignLeft);

    const QRect titleRect{coverFrameRect.right() + 2 * offset, rect.top() + titleOffset, leftWidth, rect.height()};
    drawTextBlocks(painter, opt, titleRect, title, Qt::AlignTop);

    const QRect infoRect{coverFrameRect.right() + 2 * offset, rect.top() - infoOffset, leftWidth, rect.height()};
    drawTextBlocks(painter, opt, infoRect, info, Qt::AlignBottom);

    const QLineF rightLine(subtitleBound.right() + offset, subtitleBound.center().y() + 1, rightBound.left() - offset,
                           rightBound.center().y() + 1);
    const QLineF headerLine(coverFrameRect.right() + 2 * offset, coverFrameRect.bottom() + coverFrameWidth,
                            rect.right() - offset, coverFrameRect.bottom() + coverFrameWidth);

    painter->setPen(linePen);
    if(!subtitle.empty() && !side.empty() && rect.width() > 160) {
        painter->drawLine(rightLine);
    }

    painter->drawLine(headerLine);

    if(!cover.isNull()) {
        QPen coverPen     = painter->pen();
        QColor coverColor = opt.palette.color(QPalette::Shadow);
        coverColor.setAlpha(65);
        coverPen.setColor(coverColor);
        coverPen.setWidth(coverFrameWidth);

        painter->setRenderHint(QPainter::Antialiasing);
        painter->drawRect(coverFrameRect);
        painter->drawPixmap(coverRect, cover);
    }
}

void paintSimpleHeader(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    QStyleOptionViewItem opt{option};
    opt.text.clear();
    opt.icon = {};

    QStyle* style = opt.widget ? opt.widget->style() : qApp->style();

    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    QPen linePen = painter->pen();
    linePen.setWidth(1);
    QColor lineColour = opt.palette.color(QPalette::Text);
    lineColour.setAlpha(40);
    linePen.setColor(lineColour);

    const auto title    = index.data(PlaylistItem::Role::Title).value<RichText>();
    const auto subtitle = index.data(PlaylistItem::Role::Right).value<RichText>();

    const QRect& rect    = opt.rect;
    const int height     = rect.height();
    const int halfWidth  = rect.width() / 2;
    constexpr int offset = 5;

    const QRect rightRect{rect.left() + halfWidth, rect.top(), halfWidth - offset, height};
    auto [rightBound, totalRightWidth]
        = drawTextBlocks(painter, opt, rightRect, subtitle | std::views::reverse, Qt::AlignVCenter | Qt::AlignRight);

    QRect leftRect{rect.left() + offset, rect.top(), rect.width() - totalRightWidth, height};
    if(totalRightWidth > 0) {
        leftRect.setWidth(leftRect.width() - (4 * offset));
    }
    auto [leftBound, _] = drawTextBlocks(painter, opt, leftRect, title, Qt::AlignVCenter | Qt::AlignLeft);

    if(!title.empty()) {
        if(subtitle.empty()) {
            rightBound = {rect.right() - offset, rect.top(), offset, height};
        }

        const QLineF rightLine(leftBound.right() + offset, leftBound.center().y() + 1, rightBound.left() - offset,
                               rightBound.center().y() + 1);

        painter->setPen(lineColour);
        painter->drawLine(rightLine);
    }
}

void paintSubheader(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    QStyleOptionViewItem opt{option};

    QStyle* style = opt.widget ? opt.widget->style() : qApp->style();

    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    QPen linePen = painter->pen();
    linePen.setWidth(1);
    QColor lineColour = opt.palette.color(QPalette::Text);
    lineColour.setAlpha(40);
    linePen.setColor(lineColour);

    const auto title    = index.data(PlaylistItem::Role::Title).value<RichText>();
    const auto subtitle = index.data(PlaylistItem::Role::Subtitle).value<RichText>();

    const QRect& rect    = opt.rect;
    const int height     = rect.height();
    const int halfWidth  = rect.width() / 2;
    constexpr int offset = 5;

    const QRect rightRect{rect.left() + halfWidth, rect.top(), halfWidth - offset, height};
    auto [rightBound, totalRightWidth]
        = drawTextBlocks(painter, opt, rightRect, subtitle | std::views::reverse, Qt::AlignVCenter | Qt::AlignRight);

    QRect leftRect{rect.left() + offset, rect.top(), rect.width() - totalRightWidth, height};
    if(totalRightWidth > 0) {
        leftRect.setWidth(leftRect.width() - (4 * offset));
    }
    auto [leftBound, _] = drawTextBlocks(painter, opt, leftRect, title, Qt::AlignVCenter | Qt::AlignLeft);

    if(title.empty()) {
        leftBound = {rect.left(), rect.top(), offset, height};
    }

    if(subtitle.empty()) {
        rightBound = {rect.right() - offset, rect.top(), offset, height};
    }

    painter->setPen(linePen);
    const QLineF titleLine(leftBound.right() + offset, leftBound.center().y() + 1, rightBound.left() - offset,
                           rightBound.center().y() + 1);
    painter->drawLine(titleLine);
}

void paintTrack(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    QStyleOptionViewItem opt{option};

    QStyle* style = option.widget ? option.widget->style() : qApp->style();

    const auto icon         = index.data(Qt::DecorationRole).value<QPixmap>();
    const bool singleColumn = index.data(PlaylistItem::Role::SingleColumnMode).toBool();

    if(opt.backgroundBrush != Qt::NoBrush) {
        painter->fillRect(option.rect, opt.backgroundBrush);
        opt.backgroundBrush = {};
    }

    if(!singleColumn) {
        // Draw it ourselves so it has the correct alignment
        opt.icon = {};
    }

    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, option.widget);

    const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, option.widget) + 1;

    if(singleColumn) {
        const int indent     = icon.isNull() ? index.data(PlaylistItem::Role::Indentation).toInt() : 0;
        const auto leftSide  = index.data(PlaylistItem::Role::Left).value<RichText>();
        const auto rightSide = index.data(PlaylistItem::Role::Right).value<RichText>();

        const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &option);

        const QRect rightRect     = textRect.adjusted(textRect.center().x() - textRect.left(), 0, -textMargin, 0);
        auto [_, totalRightWidth] = drawTextBlocks(painter, opt, rightRect, rightSide | std::views::reverse,
                                                   Qt::AlignVCenter | Qt::AlignRight);

        const QRect leftRect = textRect.adjusted(indent + textMargin, 0, -totalRightWidth, 0);
        drawTextBlocks(painter, opt, leftRect, leftSide, Qt::AlignVCenter | Qt::AlignLeft);
    }
    else {
        const auto columnText = index.data(PlaylistItem::Role::Column).value<RichText>();

        const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &option);

        const QRect columnRect = textRect.adjusted(textMargin, 0, -textMargin, 0);
        drawTextBlocks(painter, opt, columnRect, columnText, Qt::AlignVCenter | opt.displayAlignment);

        if(!icon.isNull()) {
            style->drawItemPixmap(painter, opt.rect, static_cast<int>(opt.displayAlignment), icon);
        }
    }
}

void PlaylistDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    painter->save();

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    const auto type = index.data(PlaylistItem::Type).toInt();
    switch(type) {
        case(PlaylistItem::Track): {
            paintTrack(painter, opt, index);
            break;
        }
        case(PlaylistItem::Header): {
            const auto simple = index.data(PlaylistItem::Simple).toBool();
            simple ? paintSimpleHeader(painter, opt, index) : paintHeader(painter, opt, index);
            break;
        }
        case(PlaylistItem::Subheader): {
            paintSubheader(painter, opt, index);
            break;
        }
        default: {
            break;
        }
    }
    painter->restore();
}

QSize PlaylistDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    // Prevents the current playing row being larger than the rest
    opt.icon           = {};
    opt.decorationSize = {};

    const QWidget* widget = opt.widget;
    const QStyle* style   = widget ? widget->style() : qApp->style();

    QSize size = index.data(Qt::SizeHintRole).toSize();
    int rowHeight{0};

    if(size.width() <= 0) {
        rowHeight = size.height();
        size      = style->sizeFromContents(QStyle::CT_ItemViewItem, &opt, size, widget);
    }

    if(rowHeight > 0) {
        size.setHeight(rowHeight);
    }

    return size;
}
} // namespace Fooyin

#include "moc_playlistdelegate.cpp"
