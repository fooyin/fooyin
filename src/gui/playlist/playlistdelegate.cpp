/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <QPainter>

namespace Fooyin {
struct DrawTextResult
{
    QRect bound;
    int totalWidth{0};
};

template <typename Range>
DrawTextResult drawTextBlocks(QPainter* painter, const QStyleOptionViewItem& option, QRect& rect, const Range& blocks,
                              Qt::AlignmentFlag alignment, bool isPlaying = false)
{
    DrawTextResult result;

    const auto colour
        = option.state & QStyle::State_Selected || isPlaying ? QPalette::HighlightedText : QPalette::NoRole;

    for(const auto& block : blocks) {
        painter->setFont(block.font);
        painter->setPen(block.colour);
        result.bound = painter->boundingRect(rect, alignment | Qt::AlignVCenter | Qt::TextWrapAnywhere, block.text);
        option.widget->style()->drawItemText(
            painter, rect, alignment | Qt::AlignVCenter, option.palette, true,
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
    const int x         = option.rect.x();
    const int y         = option.rect.y();
    const int width     = option.rect.width();
    const int height    = option.rect.height();
    const int right     = x + width;
    const int semiWidth = width / 2;
    const int offset    = 10;

    QPen linePen = painter->pen();
    linePen.setWidth(1);
    QColor lineColour = option.palette.color(QPalette::Text);
    lineColour.setAlpha(40);
    linePen.setColor(lineColour);

    const auto title     = index.data(PlaylistItem::Role::Title).value<TextBlockList>();
    const auto subtitle  = index.data(PlaylistItem::Role::Subtitle).value<TextBlockList>();
    const auto side      = index.data(PlaylistItem::Role::Right).value<TextBlockList>();
    const auto info      = index.data(PlaylistItem::Role::Info).value<TextBlockList>();
    const auto showCover = index.data(PlaylistItem::Role::ShowCover).toBool();
    const auto cover     = index.data(PlaylistItem::Role::Cover).value<QPixmap>();

    option.widget->style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter);

    const int coverSize        = 58;
    const int coverMargin      = 10;
    const int coverFrameWidth  = 2;
    const int coverFrameOffset = coverFrameWidth / 2;

    const int lineMargin = 8;

    const QRect coverRect{x + coverMargin, y + coverMargin, coverSize, height - 2 * coverMargin};
    const auto coverFrameRect = showCover
                                  ? QRect{coverRect.x() - coverFrameOffset, coverRect.y() - coverFrameOffset,
                                          coverRect.width() + coverFrameWidth, coverRect.height() + coverFrameWidth}
                                  : QRect{};

    const auto drawCover = [&painter, &option, &coverFrameRect, &coverRect, &cover]() {
        QPen coverPen     = painter->pen();
        QColor coverColor = option.palette.color(QPalette::Shadow);
        coverColor.setAlpha(65);
        coverPen.setColor(coverColor);
        coverPen.setWidth(coverFrameWidth);

        painter->setRenderHint(QPainter::Antialiasing);
        painter->drawRect(coverFrameRect);
        painter->drawPixmap(coverRect, cover);
    };

    QRect rightRect{(right - semiWidth), y, semiWidth - offset, height};
    const auto [rightBound, totalRightWidth]
        = drawTextBlocks(painter, option, rightRect, side | std::views::reverse, Qt::AlignRight);

    const int leftWidth = width - coverFrameRect.right() - totalRightWidth - (offset * 2) - 20;
    QRect subtitleRect{coverFrameRect.right() + offset, y, leftWidth, height};
    const auto [subtitleBound, _] = drawTextBlocks(painter, option, subtitleRect, subtitle, Qt::AlignLeft);

    QRect titleRect{coverFrameRect.right() + offset, y - 20, width - 2 * offset - coverFrameRect.right(), height};
    drawTextBlocks(painter, option, titleRect, title, Qt::AlignLeft);

    QRect infoRect{coverFrameRect.right() + offset, y + 20, width - 2 * offset - coverFrameRect.right(), height};
    drawTextBlocks(painter, option, infoRect, info, Qt::AlignLeft);

    painter->setPen(linePen);
    const QLineF rightLine((subtitleBound.x() + subtitleBound.width() + offset),
                           (subtitleBound.y() + (subtitleBound.height() / 2)), (rightBound.x() - offset),
                           (rightBound.y()) + (rightBound.height() / 2));
    const QLineF headerLine(x + coverSize + 2 * coverMargin, (y + height) - lineMargin, (x + width) - offset,
                            (y + height) - lineMargin);
    if(!subtitle.empty() && !side.empty() && width > 160) {
        painter->drawLine(rightLine);
    }
    painter->drawLine(headerLine);

    if(showCover) {
        drawCover();
    }
}

void paintSimpleHeader(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    const int x         = option.rect.x();
    const int y         = option.rect.y();
    const int width     = option.rect.width();
    const int height    = option.rect.height();
    const int right     = x + width;
    const int semiWidth = width / 2;
    const int offset    = 10;

    QPen linePen = painter->pen();
    linePen.setWidth(1);
    QColor lineColour = option.palette.color(QPalette::Text);
    lineColour.setAlpha(40);
    linePen.setColor(lineColour);

    const auto title = index.data(PlaylistItem::Role::Title).value<TextBlockList>();
    const auto side  = index.data(PlaylistItem::Role::Right).value<TextBlockList>();

    option.widget->style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter);

    const int lineSpacing = 10;

    QRect rightRect{(right - semiWidth), y, semiWidth - offset, height};
    const auto [rightBound, totalRightWidth]
        = drawTextBlocks(painter, option, rightRect, side | std::views::reverse, Qt::AlignRight);

    const int leftWidth = width - totalRightWidth - (offset * 2) - 20;
    QRect titleRect{x + offset, y, leftWidth, height};
    auto [titleBound, _] = drawTextBlocks(painter, option, titleRect, title, Qt::AlignLeft);

    painter->setPen(lineColour);
    if(side.empty()) {
        titleBound = {(right - 5), y, 5, height};
    }
    const QLineF rightLine((titleBound.x() + titleBound.width() + lineSpacing),
                           (titleBound.y() + (titleBound.height() / 2)), (rightBound.x() - lineSpacing),
                           (rightBound.y()) + (rightBound.height() / 2));

    if(!title.empty()) {
        painter->drawLine(rightLine);
    }
}

void paintSubheader(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    const int x         = option.rect.x();
    const int y         = option.rect.y();
    const int width     = option.rect.width();
    const int height    = option.rect.height();
    const int right     = x + width;
    const int semiWidth = width / 2;
    const int offset    = 10;

    const auto title  = index.data(PlaylistItem::Role::Title).value<TextBlockList>();
    const auto info   = index.data(PlaylistItem::Role::Subtitle).value<TextBlockList>();
    const auto indent = index.data(PlaylistItem::Role::Indentation).toInt();

    option.widget->style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter);

    if(title.empty()) {
        return;
    }

    QPen linePen = painter->pen();
    linePen.setWidth(1);
    QColor lineColour = option.palette.color(QPalette::Text);
    lineColour.setAlpha(40);
    linePen.setColor(lineColour);

    QRect rightRect{(right - semiWidth), y, semiWidth - offset, height};
    auto [rightBound, totalRightWidth]
        = drawTextBlocks(painter, option, rightRect, info | std::views::reverse, Qt::AlignRight);

    const int leftWidth = width - totalRightWidth - (offset * 2) - 20;
    QRect titleRect{(x + offset + indent), y, leftWidth, height};
    auto [titleBound, _] = drawTextBlocks(painter, option, titleRect, title, Qt::AlignLeft);

    if(info.empty()) {
        rightBound = {(right - 5), y, 5, height};
    }
    painter->setPen(linePen);
    const QLineF titleLine((titleBound.x() + titleBound.width() + 5), (titleBound.y() + (titleBound.height() / 2)),
                           (rightBound.x() - 5), (rightBound.y()) + (rightBound.height() / 2));
    painter->drawLine(titleLine);
}

void paintTrack(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    QStyleOptionViewItem opt = option;

    const int x         = opt.rect.x();
    const int y         = opt.rect.y();
    const int width     = opt.rect.width();
    const int height    = opt.rect.height();
    const int right     = x + width;
    const int semiWidth = width / 2;
    const int offset    = 10;

    const auto background = index.data(Qt::BackgroundRole).value<QPalette::ColorRole>();
    const auto leftSide   = index.data(PlaylistItem::Role::Left).value<TextBlockList>();
    const auto rightSide  = index.data(PlaylistItem::Role::Right).value<TextBlockList>();
    const bool isPlaying  = index.data(PlaylistItem::Role::Playing).toBool();
    const auto pixmap     = index.data(Qt::DecorationRole).value<QPixmap>();
    int indent            = index.data(PlaylistItem::Role::Indentation).toInt();

    QColor playColour{opt.palette.highlight().color()};
    playColour.setAlpha(90);

    if(isPlaying) {
        indent += 30;
    }

    painter->fillRect(option.rect, isPlaying ? playColour : option.palette.color(background));
    opt.widget->style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter);

    QRect rightRect{(right - semiWidth), y, semiWidth - offset, height};
    auto [_, totalRightWidth]
        = drawTextBlocks(painter, opt, rightRect, rightSide | std::views::reverse, Qt::AlignRight, isPlaying);

    const int leftWidth = width - totalRightWidth - indent - (offset * 2) - 20;
    QRect leftRect{(x + offset + indent), y, leftWidth, height};
    drawTextBlocks(painter, opt, leftRect, leftSide, Qt::AlignLeft, isPlaying);

    if(isPlaying) {
        const QRect playRect{x + offset, y, 20, height};
        opt.widget->style()->drawItemPixmap(painter, playRect, Qt::AlignLeft | Qt::AlignVCenter, pixmap);
    }
}

PlaylistDelegate::PlaylistDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{ }

QSize PlaylistDelegate::sizeHint(const QStyleOptionViewItem& /*option*/, const QModelIndex& index) const
{
    return index.data(Qt::SizeHintRole).toSize();
}

void PlaylistDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    painter->save();

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    const auto type = index.data(PlaylistItem::Type).toInt();
    switch(type) {
        case(PlaylistItem::Header): {
            const auto simple = index.data(PlaylistItem::Simple).toBool();
            simple ? paintSimpleHeader(painter, opt, index) : paintHeader(painter, opt, index);
            break;
        }
        case(PlaylistItem::Track): {
            paintTrack(painter, opt, index);
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
} // namespace Fooyin

#include "moc_playlistdelegate.cpp"
