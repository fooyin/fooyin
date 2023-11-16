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

#include "filterdelegate.h"

#include <QPainter>

namespace Fooyin::Filters {
FilterDelegate::FilterDelegate(QObject* parent)
    : QStyledItemDelegate{parent}
{ }

void FilterDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    painter->save();

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    const auto rect       = opt.rect;
    const QRect titleRect = QRect(rect.x() + 5, rect.y(), rect.width(), rect.height());

    if((opt.state & QStyle::State_Selected)) {
        painter->fillRect(rect, opt.palette.highlight());
    }

    if((opt.state & QStyle::State_MouseOver)) {
        const QColor selectColour = opt.palette.highlight().color();
        const QColor hoverCol     = QColor(selectColour.red(), selectColour.green(), selectColour.blue(), 70);
        painter->fillRect(rect, hoverCol);
    }

    const QString title = index.data(Qt::DisplayRole).toString();
    const auto font     = index.data(Qt::FontRole).value<QFont>();
    if(font.pixelSize() > 0 || font.pointSize() > 0) {
        painter->setFont(font);
    }
    const auto colour = index.data(Qt::ForegroundRole).value<QColor>();
    if(colour.isValid()) {
        painter->setPen(colour);
    }

    opt.widget->style()->drawItemText(painter, titleRect, Qt::AlignLeft | Qt::AlignVCenter, opt.palette, true,
                                      painter->fontMetrics().elidedText(title, Qt::ElideRight, rect.width()));
    painter->restore();
}
} // namespace Fooyin::Filters

#include "moc_filterdelegate.cpp"
