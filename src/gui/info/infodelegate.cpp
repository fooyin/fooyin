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

#include "infodelegate.h"

#include "infomodel.h"

#include <QPainter>

namespace {
void paintHeader(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    const int x       = option.rect.x();
    const int y       = option.rect.y();
    const int width   = option.rect.width();
    const int height  = option.rect.height();
    const int right   = x + width;
    const int centreY = y + (height / 2);

    QPen linePen = painter->pen();
    linePen.setWidth(1);
    QColor lineColour = option.palette.color(QPalette::Text);
    lineColour.setAlpha(40);
    linePen.setColor(lineColour);

    const QString title = index.data(Qt::DisplayRole).toString();

    QFont titleFont = painter->font();
    titleFont.setPixelSize(13);

    const QRect titleRect = {x + 7, y, width, height};

    painter->setFont(titleFont);
    option.widget->style()->drawItemText(painter, titleRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, true,
                                         painter->fontMetrics().elidedText(title, Qt::ElideRight, titleRect.width()));

    if(!title.isEmpty()) {
        painter->setPen(linePen);
        const QRect titleBound = painter->boundingRect(titleRect, Qt::AlignLeft | Qt::AlignVCenter, title);
        const QLineF headerLine((titleBound.right() + 10), centreY, (right - 10), centreY);
        painter->drawLine(headerLine);
    }
}

void paintEntry(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    const int x      = option.rect.x();
    const int y      = option.rect.y();
    const int width  = option.rect.width();
    const int height = option.rect.height();
    const int offset = 8;

    const QString title = index.data(Qt::DisplayRole).toString();

    QFont titleFont = painter->font();
    titleFont.setPixelSize(12);

    const QRect titleRect = {x + offset, y, width - offset, height};

    painter->setFont(titleFont);
    option.widget->style()->drawItemText(painter, titleRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, true,
                                         painter->fontMetrics().elidedText(title, Qt::ElideRight, titleRect.width()));
}
} // namespace

namespace Fooyin {
ItemDelegate::ItemDelegate(QObject* parent)
    : QStyledItemDelegate{parent}
{ }

QSize ItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const auto text = index.data(Qt::DisplayRole).toString();
    auto height     = option.fontMetrics.height();

    QFont textFont = option.font;
    // Set font slightly larger than actual to eliminate clipping when resizing
    textFont.setPixelSize(15);

    const QFontMetrics fm{textFont};
    const int width = fm.boundingRect(text).width();

    const auto type = index.data(InfoItem::Type).value<InfoItem::ItemType>();

    switch(type) {
        case(InfoItem::ItemType::Header): {
            height += 13;
            break;
        }
        case(InfoItem::ItemType::Entry): {
            height += 7;
            break;
        }
    }
    return {width, height};
}

void ItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    painter->save();

    QStyleOptionViewItem opt = option;
    const auto type          = index.data(InfoItem::Type).value<InfoItem::ItemType>();

    initStyleOption(&opt, index);

    QFont font = painter->font();
    font.setBold(true);
    font.setPixelSize(12);
    painter->setFont(font);

    switch(type) {
        case(InfoItem::ItemType::Header): {
            paintHeader(painter, opt, index);
            break;
        }
        case(InfoItem::ItemType::Entry): {
            paintEntry(painter, opt, index);
            break;
        }
    }
    painter->restore();
}
} // namespace Fooyin

#include "moc_infodelegate.cpp"
