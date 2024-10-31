/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "artworkdelegate.h"

#include "artworkitem.h"

#include <QApplication>
#include <QPainter>

using namespace Qt::StringLiterals;

constexpr auto HoriPadding = 4;
constexpr auto VertPadding = 6;

namespace Fooyin {
void ArtworkDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyledItemDelegate::paint(painter, option, index);

    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);

    const QVariant userData = index.data(ArtworkItem::Caption);

    QStyle* style        = opt.widget ? opt.widget->style() : QApplication::style();
    const QRect iconRect = style->subElementRect(QStyle::SE_ItemViewItemDecoration, &opt, opt.widget);

    if(userData.canConvert<QSize>()) {
        // Display size
        const QSize size       = userData.toSize();
        const QString sizeText = u"%1x%2"_s.arg(size.width()).arg(size.height());
        const QRect textRect   = iconRect.adjusted(0, iconRect.height() - 18, 0, 0);

        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor{0, 0, 0, 150});
        painter->drawRect(textRect);
        painter->restore();

        painter->drawText(textRect, Qt::AlignCenter, sizeText);
    }
    else {
        // Display download progress
        const int progress = userData.toInt();

        const QRect barRect         = iconRect.adjusted(0, iconRect.height() - 18, 0, 0);
        const QRect progressBarRect = barRect.adjusted(HoriPadding, VertPadding, -HoriPadding, -VertPadding);

        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor{0, 0, 0, 150});
        painter->drawRect(barRect);

        painter->setBrush(Qt::lightGray);
        painter->drawRect(progressBarRect);

        const int filledWidth = static_cast<int>(progressBarRect.width() * (progress / 100.0));
        painter->setBrush(Qt::green);
        painter->drawRect(progressBarRect.x(), progressBarRect.y(), filledWidth, progressBarRect.height());
    }
}
} // namespace Fooyin
