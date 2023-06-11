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

namespace Fy::Gui::Widgets::Playlist {
PlaylistDelegate::PlaylistDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{ }

QSize PlaylistDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    Q_UNUSED(option)
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
            simple ? paintSimpleHeader(painter, option, index) : paintHeader(painter, option, index);
            break;
        }
        case(PlaylistItem::Track): {
            paintTrack(painter, option, index);
            break;
        }
        case(PlaylistItem::Subheader): {
            paintSubheader(painter, option, index);
            break;
        }
        default: {
            break;
        }
    }
    painter->restore();
}

void PlaylistDelegate::paintSelectionBackground(QPainter* painter, const QStyleOptionViewItem& option)
{
    const QColor selectColour = option.palette.highlight().color();
    const QColor hoverColour  = QColor(selectColour.red(), selectColour.green(), selectColour.blue(), 70);

    if((option.state & QStyle::State_Selected)) {
        painter->fillRect(option.rect, option.palette.highlight());
    }

    if((option.state & QStyle::State_MouseOver)) {
        painter->fillRect(option.rect, hoverColour);
    }
}

void PlaylistDelegate::paintHeader(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    const int x      = option.rect.x();
    const int y      = option.rect.y();
    const int width  = option.rect.width();
    const int height = option.rect.height();

    QPen linePen = painter->pen();
    linePen.setWidth(1);
    QColor lineColour = option.palette.color(QPalette::Text);
    lineColour.setAlpha(40);
    linePen.setColor(lineColour);

    const auto title     = index.data(PlaylistItem::Role::Title).value<TextBlock>();
    const auto subtitle  = index.data(PlaylistItem::Role::Subtitle).value<TextBlock>();
    const auto right     = index.data(PlaylistItem::Role::Right).value<TextBlock>();
    const auto info      = index.data(PlaylistItem::Role::Info).value<TextBlock>();
    const auto showCover = index.data(PlaylistItem::Role::ShowCover).toBool();
    const auto cover     = index.data(PlaylistItem::Role::Cover).value<QPixmap>();

    paintSelectionBackground(painter, option);

    const int coverSize        = 58;
    const int coverMargin      = 10;
    const int coverFrameWidth  = 2;
    const int coverFrameOffset = coverFrameWidth / 2;

    const int textMargin = 10;
    const int yearWidth  = 100;
    const int lineMargin = 8;

    const QRect coverRect{x + coverMargin, y + coverMargin, coverSize, height - 2 * coverMargin};
    const auto coverFrameRect = showCover
                                  ? QRect{coverRect.x() - coverFrameOffset, coverRect.y() - coverFrameOffset,
                                          coverRect.width() + coverFrameWidth, coverRect.height() + coverFrameWidth}
                                  : QRect{};

    const QRect titleRect{coverFrameRect.right() + textMargin, y - 20, width - 2 * textMargin - coverFrameRect.right(),
                          height};
    const QRect subtitleRect{coverFrameRect.right() + textMargin, y, width - 2 * textMargin - coverFrameRect.right(),
                             height};
    const QRect infoRect{coverFrameRect.right() + textMargin, y + 20, width - 2 * textMargin - coverFrameRect.right(),
                         height};
    const QRect rightRect{x + width - yearWidth - textMargin, y, yearWidth, height};

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

    painter->setFont(title.font);
    painter->setPen(title.colour);
    option.widget->style()->drawItemText(
        painter, titleRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, true,
        painter->fontMetrics().elidedText(title.text, Qt::ElideRight, titleRect.width()));

    painter->setPen(option.palette.color(QPalette::Text));
    painter->setFont(subtitle.font);
    painter->setPen(subtitle.colour);
    const QRect subtitleBound
        = painter->boundingRect(subtitleRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWrapAnywhere, subtitle.text);
    option.widget->style()->drawItemText(
        painter, subtitleRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, true,
        painter->fontMetrics().elidedText(subtitle.text, Qt::ElideRight, subtitleRect.width()));

    painter->setFont(right.font);
    painter->setPen(right.colour);
    const QRect rightBound = painter->boundingRect(rightRect, Qt::AlignRight | Qt::AlignVCenter, right.text);
    if(width > 160) {
        option.widget->style()->drawItemText(painter, rightRect, Qt::AlignRight | Qt::AlignVCenter, option.palette,
                                             true, right.text);
    }

    painter->setFont(info.font);
    painter->setPen(info.colour);
    option.widget->style()->drawItemText(
        painter, infoRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, true,
        painter->fontMetrics().elidedText(info.text, Qt::ElideRight, infoRect.width()));

    painter->setPen(linePen);
    const QLineF rightLine((subtitleBound.x() + subtitleBound.width() + textMargin),
                           (subtitleBound.y() + (subtitleBound.height() / 2)), (rightBound.x() - textMargin),
                           (rightBound.y()) + (rightBound.height() / 2));
    const QLineF headerLine(x + coverSize + 2 * coverMargin, (y + height) - lineMargin, (x + width) - textMargin,
                            (y + height) - lineMargin);
    if(!subtitle.text.isEmpty() && !right.text.isEmpty() && width > 160) {
        painter->drawLine(rightLine);
    }
    painter->drawLine(headerLine);

    if(showCover) {
        drawCover();
    }
}

void PlaylistDelegate::paintSimpleHeader(QPainter* painter, const QStyleOptionViewItem& option,
                                         const QModelIndex& index)
{
    const int x      = option.rect.x();
    const int y      = option.rect.y();
    const int width  = option.rect.width();
    const int height = option.rect.height();
    const int right  = x + width;

    QPen linePen = painter->pen();
    linePen.setWidth(1);
    QColor lineColour = option.palette.color(QPalette::Text);
    lineColour.setAlpha(40);
    linePen.setColor(lineColour);

    const auto title     = index.data(PlaylistItem::Role::Title).value<TextBlock>();
    const auto subtitle  = index.data(PlaylistItem::Role::Subtitle).value<TextBlock>();
    const auto rightText = index.data(PlaylistItem::Role::Right).value<TextBlock>();

    paintSelectionBackground(painter, option);

    const int titleMargin = 10;
    const int rightWidth  = 50;
    const int rightMargin = 60;
    const int lineSpacing = 10;

    const QRect titleRect{x + titleMargin, y, ((right - 80) - (x + 80)), height};
    const QRect rightRect{right - rightMargin, y, rightWidth, height};

    painter->setFont(title.font);
    painter->setPen(title.colour);
    option.widget->style()->drawItemText(
        painter, titleRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, true,
        painter->fontMetrics().elidedText(title.text, Qt::ElideRight, titleRect.width()));

    const QRect titleBound
        = painter->boundingRect(titleRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWrapAnywhere, title.text);

    const QRect subtitleRect = QRect(titleBound.right(), y, ((right - rightMargin) - (x + rightMargin)), height);

    painter->setPen(option.palette.color(QPalette::Text));
    painter->setFont(subtitle.font);
    painter->setPen(subtitle.colour);
    QRect subtitleBound
        = painter->boundingRect(subtitleRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWrapAnywhere, subtitle.text);
    option.widget->style()->drawItemText(
        painter, subtitleRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, true,
        painter->fontMetrics().elidedText(subtitle.text, Qt::ElideRight, subtitleRect.width()));

    painter->setFont(rightText.font);
    const QRect rightBound = painter->boundingRect(rightRect, Qt::AlignRight | Qt::AlignVCenter, rightText.text);
    option.widget->style()->drawItemText(painter, rightRect, Qt::AlignRight | Qt::AlignVCenter, option.palette, true,
                                         rightText.text);

    painter->setPen(lineColour);
    if(subtitle.text.isEmpty()) {
        subtitleBound = titleBound;
    }
    const QLineF rightLine((subtitleBound.x() + subtitleBound.width() + lineSpacing),
                           (subtitleBound.y() + (subtitleBound.height() / 2)), (rightBound.x() - lineSpacing),
                           (rightBound.y()) + (rightBound.height() / 2));

    if(!subtitle.text.isEmpty() && !rightText.text.isEmpty()) {
        painter->drawLine(rightLine);
    }
}

void PlaylistDelegate::paintTrack(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    const int x         = option.rect.x();
    const int y         = option.rect.y();
    const int width     = option.rect.width();
    const int height    = option.rect.height();
    const int right     = x + width;
    const int semiWidth = width / 2;
    const int offset    = 10;

    const auto background = index.data(Qt::BackgroundRole).value<QPalette::ColorRole>();
    const auto leftSide   = index.data(PlaylistItem::Role::Left).value<TextBlockList>();
    const auto rightSide  = index.data(PlaylistItem::Role::Right).value<TextBlockList>();
    const bool isPlaying  = index.data(PlaylistItem::Role::Playing).toBool();
    const auto pixmap     = index.data(Qt::DecorationRole).value<QPixmap>();
    int indent            = index.data(PlaylistItem::Role::Indentation).toInt();

    QColor playColour{option.palette.highlight().color()};
    playColour.setAlpha(45);

    if(isPlaying) {
        indent += 30;
    }

    painter->fillRect(option.rect, isPlaying ? playColour : option.palette.color(background));
    paintSelectionBackground(painter, option);

    QRect rightBound;
    QRect rightRect{(right - semiWidth), y, semiWidth - offset, height};

    for(auto it = std::rbegin(rightSide); it != std::rend(rightSide); ++it) {
        const auto& block = *it;
        painter->setFont(block.font);
        painter->setPen(block.colour);
        rightBound
            = painter->boundingRect(rightRect, Qt::AlignRight | Qt::AlignVCenter | Qt::TextWrapAnywhere, block.text);
        option.widget->style()->drawItemText(painter, rightRect, Qt::AlignRight | Qt::AlignVCenter, option.palette,
                                             true, block.text);

        rightRect.moveRight((rightRect.x() + rightRect.width()) - rightBound.width() - offset);
    }

    const QRect playRect{x + offset, y, 20, height};

    const int rightWidth = right - rightBound.x();
    const int leftWidth  = width - rightWidth - indent - (offset * 2) - 20;
    QRect leftRect{(x + offset + indent), y, leftWidth, height};

    for(const auto& block : leftSide) {
        painter->setFont(block.font);
        painter->setPen(block.colour);
        const QRect leftBound
            = painter->boundingRect(leftRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWrapAnywhere, block.text);
        option.widget->style()->drawItemText(
            painter, leftRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, true,
            painter->fontMetrics().elidedText(block.text, Qt::ElideRight, leftRect.width()));

        leftRect.setWidth(leftRect.width() - leftBound.width());
        leftRect.moveLeft(leftRect.x() + leftBound.width() + offset);
    }

    if(isPlaying) {
        option.widget->style()->drawItemPixmap(painter, playRect, Qt::AlignLeft | Qt::AlignVCenter, pixmap);
    }
}

void PlaylistDelegate::paintSubheader(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    const int x         = option.rect.x();
    const int y         = option.rect.y();
    const int width     = option.rect.width();
    const int height    = option.rect.height();
    const int right     = x + width;
    const int semiWidth = width / 2;
    const int offset    = 10;

    const auto title       = index.data(PlaylistItem::Role::Title).value<TextBlockList>();
    const auto subtitle    = index.data(PlaylistItem::Role::Subtitle).value<TextBlockList>();
    const auto indentation = index.data(PlaylistItem::Role::Indentation).toInt();

    paintSelectionBackground(painter, option);

    QRect titleRect{x + offset + indentation, y, semiWidth, height};
    QRect subtitleRect{(right - semiWidth), y, semiWidth - offset, height};

    QPen linePen = painter->pen();
    linePen.setWidth(1);
    QColor lineColour = option.palette.color(QPalette::Text);
    lineColour.setAlpha(40);
    linePen.setColor(lineColour);

    QRect titleBound;

    for(const auto& block : title) {
        painter->setFont(block.font);
        painter->setPen(block.colour);
        titleBound
            = painter->boundingRect(titleRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWrapAnywhere, block.text);
        option.widget->style()->drawItemText(
            painter, titleRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWrapAnywhere, option.palette, true,
            painter->fontMetrics().elidedText(block.text, Qt::ElideRight, titleRect.width()));

        titleRect.setWidth(titleRect.width() - titleRect.width());
        titleRect.moveLeft(titleRect.x() + titleRect.width() + offset);
    }

    QRect subtitleBound;

    for(auto it = std::rbegin(subtitle); it != std::rend(subtitle); ++it) {
        const auto& block = *it;
        painter->setFont(block.font);
        painter->setPen(block.colour);
        subtitleBound
            = painter->boundingRect(subtitleRect, Qt::AlignRight | Qt::AlignVCenter | Qt::TextWrapAnywhere, block.text);
        option.widget->style()->drawItemText(painter, subtitleRect, Qt::AlignRight | Qt::AlignVCenter, option.palette,
                                             true, block.text);

        subtitleRect.moveRight((subtitleRect.x() + subtitleRect.width()) - subtitleBound.width() - offset);
    }

    if(subtitle.empty()) {
        subtitleBound = {(right - 5), y, 5, height};
    }
    painter->setPen(linePen);
    const QLineF titleLine((titleBound.x() + titleBound.width() + 5), (titleBound.y() + (titleBound.height() / 2)),
                           (subtitleBound.x() - 5), (subtitleBound.y()) + (subtitleBound.height() / 2));
    painter->drawLine(titleLine);
}
} // namespace Fy::Gui::Widgets::Playlist
