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

#include "replaygaindelegate.h"

#include "replaygainitem.h"

#include <QApplication>
#include <QPainter>

namespace {
void paintHeader(QPainter* painter, const QStyleOptionViewItem& opt, const QModelIndex& /*index*/)
{
    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();

    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    const QRect& rect    = opt.rect;
    constexpr int offset = 10;

    QPen linePen = painter->pen();
    linePen.setWidth(1);
    QColor lineColour = opt.palette.color(QPalette::Text);
    lineColour.setAlpha(40);
    linePen.setColor(lineColour);

    if(!opt.text.isEmpty()) {
        const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt);
        painter->setPen(linePen);
        const QRect titleBound = painter->boundingRect(textRect, static_cast<int>(opt.displayAlignment), opt.text);
        const QLineF headerLine(titleBound.right() + (2 * offset), titleBound.center().y() + 1, rect.right() - offset,
                                titleBound.center().y() + 1);
        painter->drawLine(headerLine);
    }
}

void paintEntry(QPainter* painter, const QStyleOptionViewItem& opt, const QModelIndex& index)
{
    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    if(index.column() > 0 && !index.data(Fooyin::ReplayGainItem::IsSummary).toBool()) {
        QPen linePen = painter->pen();
        linePen.setWidth(1);
        QColor lineColour = opt.palette.color(QPalette::Text);
        lineColour.setAlpha(40);
        linePen.setColor(lineColour);

        painter->setPen(linePen);
        const int x = opt.rect.left();
        painter->drawLine(x, opt.rect.top(), x, opt.rect.bottom());
    }
}
} // namespace

namespace Fooyin {
void ReplayGainDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);

    painter->save();

    if(index.flags() & Qt::ItemNeverHasChildren) {
        paintEntry(painter, opt, index);
    }
    else {
        paintHeader(painter, opt, index);
    }

    painter->restore();
}
} // namespace Fooyin

#include "moc_replaygaindelegate.cpp"
