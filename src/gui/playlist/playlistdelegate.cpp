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
#include "playlistmodel.h"

#include <QPainter>

namespace Gui::Widgets {
PlaylistDelegate::PlaylistDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{ }

QSize PlaylistDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const auto width = index.data(Qt::SizeHintRole).toSize().width();
    auto height      = option.fontMetrics.height();

    const auto type   = index.data(Playlist::Type).value<PlaylistItem::Type>();
    const auto simple = index.data(Playlist::Mode).toBool();

    switch(type) {
        case(PlaylistItem::Album): {
            height += !simple ? 60 : 10;
            break;
        }
        case(PlaylistItem::Track): {
            height += 7;
            break;
        }
        case(PlaylistItem::Disc): {
            height += 3;
            break;
        }
        default: {
        }
    }
    return {width, height};
}

void PlaylistDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    painter->save();

    QStyleOptionViewItem opt = option;
    const auto type          = index.data(Playlist::Type).value<PlaylistItem::Type>();
    const auto simple        = index.data(Playlist::Mode).toBool();

    initStyleOption(&opt, index);

    QFont font = painter->font();
    font.setBold(true);
    font.setPixelSize(11);
    painter->setFont(font);

    switch(type) {
        case(PlaylistItem::Album): {
            simple ? paintSimpleAlbum(painter, option, index) : paintAlbum(painter, option, index);
            break;
        }
        case(PlaylistItem::Track): {
            paintTrack(painter, option, index);
            break;
        }
        case(PlaylistItem::Disc): {
            paintDisc(painter, option, index);
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

void PlaylistDelegate::paintAlbum(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    const int x      = option.rect.x();
    const int y      = option.rect.y();
    const int width  = option.rect.width();
    const int height = option.rect.height();
    const int right  = x + width;

    QPen linePen = painter->pen();
    linePen.setWidth(1);
    QColor lineColour = option.palette.color(QPalette::BrightText);
    lineColour.setAlpha(40);
    linePen.setColor(lineColour);

    const QString albumTitle    = index.data(Qt::DisplayRole).toString();
    const QString albumArtist   = index.data(PlaylistItem::Role::Artist).toString();
    const QString albumYear     = index.data(PlaylistItem::Role::Year).toString();
    const QString albumDuration = index.data(PlaylistItem::Role::Duration).toString();
    const auto albumCover       = index.data(PlaylistItem::Role::Cover).value<QPixmap>();

    QFont artistFont = painter->font();
    artistFont.setPixelSize(15);
    QFont titleFont = painter->font();
    titleFont.setPixelSize(14);
    QFont yearFont = painter->font();
    yearFont.setPixelSize(14);
    QFont totalFont = painter->font();
    totalFont.setPixelSize(10);

    paintSelectionBackground(painter, option);

    const auto coverFrameWidth  = 2;
    const auto coverFrameOffset = coverFrameWidth / 2;

    const QRect coverRect{x + 10, y + 10, 58, height - 19};
    const QRect coverFrameRect{coverRect.x() - coverFrameOffset, coverRect.y() - coverFrameOffset,
                               coverRect.width() + coverFrameWidth, coverRect.height() + coverFrameWidth};
    const QRect artistRect{coverFrameRect.right() + 10, y - 20, ((right - 80) - (x + 80)), height};
    const QRect titleRect{coverFrameRect.right() + 10, y, ((right - 80) - (x + 80)), height};
    const QRect subRect{coverFrameRect.right() + 10, y + 20, ((right - 80) - (x + 80)), height};
    const QRect yearRect{right - 110, y, 100, height};

    QPen coverPen     = painter->pen();
    QColor coverColor = option.palette.color(QPalette::Shadow);
    coverColor.setAlpha(65);
    coverPen.setColor(coverColor);
    coverPen.setWidth(coverFrameWidth);

    painter->drawPixmap(coverRect, albumCover);

    painter->setFont(artistFont);
    option.widget->style()->drawItemText(
        painter, artistRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, true,
        painter->fontMetrics().elidedText(albumArtist, Qt::ElideRight, artistRect.width()));

    painter->setPen(option.palette.color(QPalette::Text));
    painter->setFont(titleFont);
    const QRect titleBound
        = painter->boundingRect(titleRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWrapAnywhere, albumTitle);
    option.widget->style()->drawItemText(
        painter, titleRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, true,
        painter->fontMetrics().elidedText(albumTitle, Qt::ElideRight, titleRect.width()));

    painter->setFont(yearFont);
    const QRect yearBound = painter->boundingRect(yearRect, Qt::AlignRight | Qt::AlignVCenter, albumYear);
    if(width > 160) {
        option.widget->style()->drawItemText(painter, yearRect, Qt::AlignRight | Qt::AlignVCenter, option.palette, true,
                                             albumYear);
    }

    painter->setFont(totalFont);
    option.widget->style()->drawItemText(
        painter, subRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, true,
        painter->fontMetrics().elidedText(albumDuration, Qt::ElideRight, subRect.width()));

    painter->setPen(linePen);
    const QLineF yearLine((titleBound.x() + titleBound.width() + 10), (titleBound.y() + (titleBound.height() / 2)),
                          (yearBound.x() - 10), (yearBound.y()) + (yearBound.height() / 2));
    const QLineF headerLine(x + 77, (y + height) - 8, (right)-10, (y + height) - 8);
    if(!albumTitle.isEmpty() && width > 160) {
        painter->drawLine(yearLine);
    }
    painter->drawLine(headerLine);

    painter->setPen(coverPen);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->drawRect(coverFrameRect);
}

void PlaylistDelegate::paintSimpleAlbum(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    const int x      = option.rect.x();
    const int y      = option.rect.y();
    const int width  = option.rect.width();
    const int height = option.rect.height();
    const int right  = x + width;

    QPen linePen = painter->pen();
    linePen.setWidth(1);
    QColor lineColour = option.palette.color(QPalette::BrightText);
    lineColour.setAlpha(40);
    linePen.setColor(lineColour);

    const QString albumTitle  = index.data(Qt::DisplayRole).toString();
    const QString albumArtist = index.data(PlaylistItem::Role::Artist).toString();
    const QString albumYear   = index.data(PlaylistItem::Role::Year).toString();

    QFont artistFont = painter->font();
    artistFont.setPixelSize(15);
    QFont titleFont = painter->font();
    titleFont.setPixelSize(14);
    QFont yearFont = painter->font();
    yearFont.setPixelSize(14);
    QFont totalFont = painter->font();
    totalFont.setPixelSize(10);

    paintSelectionBackground(painter, option);

    artistFont.setPixelSize(13);
    titleFont.setPixelSize(12);
    yearFont.setPixelSize(12);

    const QRect artistRect{x + 10, y, ((right - 80) - (x + 80)), height};
    const QRect yearRect{right - 60, y, 50, height};

    const QString titleSpacing = !albumTitle.isEmpty() && !albumArtist.isEmpty() ? "  -  " : "";

    painter->setFont(artistFont);
    option.widget->style()->drawItemText(
        painter, artistRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, true,
        painter->fontMetrics().elidedText(albumArtist + titleSpacing, Qt::ElideRight, artistRect.width()));

    const QRect artistBound = painter->boundingRect(artistRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWrapAnywhere,
                                                    albumArtist + titleSpacing);

    const QRect titleRect = QRect(artistBound.right(), y, ((right - 80) - (x + 80)), height);

    painter->setPen(option.palette.color(QPalette::Text));
    painter->setFont(titleFont);
    const QRect titleBound
        = painter->boundingRect(titleRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWrapAnywhere, albumTitle);
    option.widget->style()->drawItemText(
        painter, titleRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, true,
        painter->fontMetrics().elidedText(albumTitle, Qt::ElideRight, titleRect.width()));

    painter->setFont(yearFont);
    const QRect yearBound = painter->boundingRect(yearRect, Qt::AlignRight | Qt::AlignVCenter, albumYear);
    option.widget->style()->drawItemText(painter, yearRect, Qt::AlignRight | Qt::AlignVCenter, option.palette, true,
                                         albumYear);

    painter->setPen(lineColour);
    const QLineF yearLine((titleBound.x() + titleBound.width() + 10), (titleBound.y() + (titleBound.height() / 2)),
                          (yearBound.x() - 10), (yearBound.y()) + (yearBound.height() / 2));

    if(!albumTitle.isEmpty()) {
        painter->drawLine(yearLine);
    }
}

void PlaylistDelegate::paintTrack(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    const int x      = option.rect.x();
    const int y      = option.rect.y();
    const int width  = option.rect.width();
    const int height = option.rect.height();
    const int right  = x + width;

    const auto background        = index.data(Qt::BackgroundRole).value<QPalette::ColorRole>();
    const QString trackNumber    = index.data(PlaylistItem::Role::Number).toString();
    const QString trackTitle     = index.data(Qt::DisplayRole).toString();
    const QString trackArtists   = index.data(PlaylistItem::Role::Artist).toString();
    const QString trackPlayCount = index.data(PlaylistItem::Role::PlayCount).toString();
    const QString trackDuration  = index.data(PlaylistItem::Role::Duration).toString();
    const bool multiDiscs        = index.data(PlaylistItem::Role::MultiDisk).toBool();
    const bool isPlaying         = index.data(PlaylistItem::Role::Playing).toBool();
    const auto pixmap            = index.data(Qt::DecorationRole).value<QPixmap>();

    const QColor textColour{option.palette.text().color()};
    QColor subTextColour{textColour};
    subTextColour.setAlpha(90);
    QColor playColour{option.palette.highlight().color()};
    playColour.setAlpha(45);

    int offset = 0;

    if(multiDiscs) {
        offset += 20;
    }

    if(isPlaying) {
        offset += 30;
    }

    const QRect playRect{x + 10, y, 20, height};
    QRect titleRect{(x + 45 + offset), y, ((right - 80) - (x + 45 + offset)), height};
    const QRect numRect{(x + 10 + offset), y, 15, height};
    const QRect countRect{(right - 110), y, 35, height};
    const QRect durRect{(right - 60), y, 50, height};

    const QRect titleBound
        = painter->boundingRect(titleRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWrapAnywhere, trackTitle);

    painter->fillRect(option.rect, isPlaying ? playColour : option.palette.color(background));
    paintSelectionBackground(painter, option);

    option.widget->style()->drawItemText(painter, numRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, true,
                                         trackNumber);

    option.widget->style()->drawItemText(
        painter, titleRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, true,
        painter->fontMetrics().elidedText(trackTitle, Qt::ElideRight, titleRect.width()));

    titleRect.setWidth(titleRect.width() - titleBound.width());
    titleRect.moveLeft(titleRect.x() + titleBound.width());

    painter->setPen(subTextColour);

    option.widget->style()->drawItemText(
        painter, titleRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, true,
        painter->fontMetrics().elidedText(trackArtists, Qt::ElideRight, titleRect.width()));

    painter->setPen(textColour);

    option.widget->style()->drawItemText(painter, countRect, Qt::AlignRight | Qt::AlignVCenter, option.palette, true,
                                         trackPlayCount);

    option.widget->style()->drawItemText(painter, durRect, Qt::AlignRight | Qt::AlignVCenter, option.palette, true,
                                         trackDuration);

    if(isPlaying) {
        option.widget->style()->drawItemPixmap(painter, playRect, Qt::AlignLeft | Qt::AlignVCenter, pixmap);
    }
}

void PlaylistDelegate::paintDisc(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    const int x      = option.rect.x();
    const int y      = option.rect.y();
    const int width  = option.rect.width();
    const int height = option.rect.height();
    const int right  = x + width;

    const QString discNumber   = index.data(Qt::DisplayRole).toString();
    const QString discDuration = index.data(PlaylistItem::Role::Duration).toString();

    paintSelectionBackground(painter, option);

    const QRect discRect{x + 10, y, 50, height};
    const QRect durationRect{right - 60, y, 50, height};

    QColor lineColour = option.palette.color(QPalette::BrightText);
    lineColour.setAlpha(50);

    const QRect discBound
        = painter->boundingRect(discRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWrapAnywhere, discNumber);
    const QRect durationBound
        = painter->boundingRect(durationRect, Qt::AlignRight | Qt::AlignVCenter | Qt::TextWrapAnywhere, discDuration);

    painter->setPen(option.palette.color(QPalette::Text));
    option.widget->style()->drawItemText(painter, discRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, true,
                                         discNumber);

    option.widget->style()->drawItemText(painter, durationRect, Qt::AlignRight | Qt::AlignVCenter, option.palette, true,
                                         discDuration);

    painter->setPen(lineColour);
    const QLineF discLine((discBound.x() + discBound.width() + 5), (discBound.y() + (discBound.height() / 2)),
                          (durationBound.x() - 5), (durationBound.y()) + (durationBound.height() / 2));
    painter->drawLine(discLine);
}
} // namespace Gui::Widgets
