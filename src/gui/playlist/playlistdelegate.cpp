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
    QColor lineColour = option.palette.color(QPalette::BrightText);
    lineColour.setAlpha(40);
    linePen.setColor(lineColour);

    const QString title     = index.data(Qt::DisplayRole).toString();
    const QString subtitle  = index.data(PlaylistItem::Role::Subtitle).toString();
    const QString rightText = index.data(PlaylistItem::Role::RightText).toString();
    const QString info      = index.data(PlaylistItem::Role::Info).toString();
    const auto showCover    = index.data(PlaylistItem::Role::ShowCover).toBool();
    const auto cover        = index.data(PlaylistItem::Role::Cover).value<QPixmap>();

    const auto titleFont    = index.data(PlaylistItem::Role::TitleFont).value<QFont>();
    const auto subtitleFont = index.data(PlaylistItem::Role::SubtitleFont).value<QFont>();
    const auto rightFont    = index.data(PlaylistItem::Role::RightTextFont).value<QFont>();

    QFont infoFont{painter->font()};
    infoFont.setPixelSize(11);

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

    if(showCover) {
        QPen coverPen     = painter->pen();
        QColor coverColor = option.palette.color(QPalette::Shadow);
        coverColor.setAlpha(65);
        coverPen.setColor(coverColor);
        coverPen.setWidth(coverFrameWidth);

        painter->setRenderHint(QPainter::Antialiasing);
        painter->drawRect(coverFrameRect);
        painter->drawPixmap(coverRect, cover);
    }

    painter->setFont(titleFont);
    option.widget->style()->drawItemText(painter, titleRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, true,
                                         painter->fontMetrics().elidedText(title, Qt::ElideRight, titleRect.width()));

    painter->setPen(option.palette.color(QPalette::Text));
    painter->setFont(subtitleFont);
    const QRect subtitleBound
        = painter->boundingRect(subtitleRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWrapAnywhere, subtitle);
    option.widget->style()->drawItemText(
        painter, subtitleRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, true,
        painter->fontMetrics().elidedText(subtitle, Qt::ElideRight, subtitleRect.width()));

    painter->setFont(rightFont);
    const QRect rightBound = painter->boundingRect(rightRect, Qt::AlignRight | Qt::AlignVCenter, rightText);
    if(width > 160) {
        option.widget->style()->drawItemText(painter, rightRect, Qt::AlignRight | Qt::AlignVCenter, option.palette,
                                             true, rightText);
    }

    painter->setFont(infoFont);
    option.widget->style()->drawItemText(painter, infoRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, true,
                                         painter->fontMetrics().elidedText(info, Qt::ElideRight, infoRect.width()));

    painter->setPen(linePen);
    const QLineF rightLine((subtitleBound.x() + subtitleBound.width() + textMargin),
                           (subtitleBound.y() + (subtitleBound.height() / 2)), (rightBound.x() - textMargin),
                           (rightBound.y()) + (rightBound.height() / 2));
    const QLineF headerLine(x + coverSize + 2 * coverMargin, (y + height) - lineMargin, (x + width) - textMargin,
                            (y + height) - lineMargin);
    if(!subtitle.isEmpty() && !rightText.isEmpty() && width > 160) {
        painter->drawLine(rightLine);
    }
    painter->drawLine(headerLine);
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
    QColor lineColour = option.palette.color(QPalette::BrightText);
    lineColour.setAlpha(40);
    linePen.setColor(lineColour);

    const QString title     = index.data(Qt::DisplayRole).toString();
    const QString subtitle  = index.data(PlaylistItem::Role::Subtitle).toString();
    const QString rightText = index.data(PlaylistItem::Role::RightText).toString();

    const auto titleFont    = index.data(PlaylistItem::Role::TitleFont).value<QFont>();
    const auto subtitleFont = index.data(PlaylistItem::Role::SubtitleFont).value<QFont>();
    const auto rightFont    = index.data(PlaylistItem::Role::RightTextFont).value<QFont>();

    paintSelectionBackground(painter, option);

    const int titleMargin = 10;
    const int rightWidth  = 50;
    const int rightMargin = 60;
    const int lineSpacing = 10;

    const QRect titleRect{x + titleMargin, y, ((right - 80) - (x + 80)), height};
    const QRect rightRect{right - rightMargin, y, rightWidth, height};

    const QString titleSpacing = !title.isEmpty() && !subtitle.isEmpty() ? "  -  " : "";

    painter->setFont(titleFont);
    option.widget->style()->drawItemText(
        painter, titleRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, true,
        painter->fontMetrics().elidedText(title + titleSpacing, Qt::ElideRight, titleRect.width()));

    const QRect titleBound = painter->boundingRect(titleRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWrapAnywhere,
                                                   title + titleSpacing);

    const QRect subtitleRect = QRect(titleBound.right(), y, ((right - rightMargin) - (x + rightMargin)), height);

    painter->setPen(option.palette.color(QPalette::Text));
    painter->setFont(subtitleFont);
    QRect subtitleBound
        = painter->boundingRect(subtitleRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWrapAnywhere, subtitle);
    option.widget->style()->drawItemText(
        painter, subtitleRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, true,
        painter->fontMetrics().elidedText(subtitle, Qt::ElideRight, subtitleRect.width()));

    painter->setFont(rightFont);
    const QRect rightBound = painter->boundingRect(rightRect, Qt::AlignRight | Qt::AlignVCenter, rightText);
    option.widget->style()->drawItemText(painter, rightRect, Qt::AlignRight | Qt::AlignVCenter, option.palette, true,
                                         rightText);

    painter->setPen(lineColour);
    if(subtitle.isEmpty()) {
        subtitleBound = titleBound;
    }
    const QLineF yearLine((subtitleBound.x() + subtitleBound.width() + lineSpacing),
                          (subtitleBound.y() + (subtitleBound.height() / 2)), (rightBound.x() - lineSpacing),
                          (rightBound.y()) + (rightBound.height() / 2));

    if(!subtitle.isEmpty() && !rightText.isEmpty()) {
        painter->drawLine(yearLine);
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
    const auto leftSide   = index.data(Qt::DisplayRole).toStringList();
    const auto rightSide  = index.data(PlaylistItem::Role::RightText).toStringList();
    const bool isPlaying  = index.data(PlaylistItem::Role::Playing).toBool();
    const auto pixmap     = index.data(Qt::DecorationRole).value<QPixmap>();
    int indent            = index.data(PlaylistItem::Role::Indentation).toInt();

    const auto leftFont  = index.data(PlaylistItem::Role::LeftFont).value<QFont>();
    const auto rightFont = index.data(PlaylistItem::Role::RightFont).value<QFont>();

    const QColor textColour{option.palette.text().color()};
    QColor subTextColour{textColour};
    subTextColour.setAlpha(90);
    QColor playColour{option.palette.highlight().color()};
    playColour.setAlpha(45);

    if(isPlaying) {
        indent += 30;
    }

    const QRect playRect{x + offset, y, 20, height};

    QRect rightRect{(right - semiWidth), y, semiWidth - offset, height};
    painter->setFont(rightFont);
    QRect rightBound = painter->boundingRect(rightRect, Qt::AlignRight | Qt::AlignVCenter | Qt::TextWrapAnywhere,
                                             rightSide.join(", "));

    const int leftWidth = (width - rightBound.width() - indent - (offset * 2) - 20);
    QRect leftRect{(x + offset + indent), y, leftWidth, height};

    painter->fillRect(option.rect, isPlaying ? playColour : option.palette.color(background));
    paintSelectionBackground(painter, option);

    painter->setFont(leftFont);
    for(const QString& text : leftSide) {
        const QRect leftBound
            = painter->boundingRect(leftRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWrapAnywhere, text);
        option.widget->style()->drawItemText(painter, leftRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, true,
                                             painter->fontMetrics().elidedText(text, Qt::ElideRight, leftRect.width()));

        leftRect.setWidth(leftRect.width() - leftBound.width());
        leftRect.moveLeft(leftRect.x() + leftBound.width() + offset);
    }

    painter->setFont(rightFont);
    for(int i = rightSide.size() - 1; i >= 0; --i) {
        rightBound
            = painter->boundingRect(rightRect, Qt::AlignRight | Qt::AlignVCenter | Qt::TextWrapAnywhere, rightSide[i]);
        option.widget->style()->drawItemText(painter, rightRect, Qt::AlignRight | Qt::AlignVCenter, option.palette,
                                             true, rightSide[i]);

        rightRect.moveRight((rightRect.x() + rightRect.width()) - rightBound.width() - offset);
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

    const QString title    = index.data(Qt::DisplayRole).toString();
    const QString duration = index.data(PlaylistItem::Role::Duration).toString();
    const auto indentation = index.data(PlaylistItem::Role::Indentation).toInt();

    const auto titleFont = index.data(PlaylistItem::Role::TitleFont).value<QFont>();

    paintSelectionBackground(painter, option);

    const QRect titleRect{x + offset + indentation, y, semiWidth, height};
    const QRect durationRect{(right - semiWidth), y, semiWidth - offset, height};

    QColor lineColour = option.palette.color(QPalette::BrightText);
    lineColour.setAlpha(50);

    painter->setFont(titleFont);
    painter->setPen(option.palette.color(QPalette::Text));
    const QRect titleBound
        = painter->boundingRect(titleRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWrapAnywhere, title);
    option.widget->style()->drawItemText(painter, titleRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, true,
                                         title);

    const QRect durationBound
        = painter->boundingRect(durationRect, Qt::AlignRight | Qt::AlignVCenter | Qt::TextWrapAnywhere, duration);
    option.widget->style()->drawItemText(painter, durationRect, Qt::AlignRight | Qt::AlignVCenter, option.palette, true,
                                         duration);

    painter->setPen(lineColour);
    const QLineF titleLine((titleBound.x() + titleBound.width() + 5), (titleBound.y() + (titleBound.height() / 2)),
                           (durationBound.x() - 5), (durationBound.y()) + (durationBound.height() / 2));
    painter->drawLine(titleLine);
}
} // namespace Fy::Gui::Widgets::Playlist
