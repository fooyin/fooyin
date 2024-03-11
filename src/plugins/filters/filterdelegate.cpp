/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <QApplication>
#include <QPainter>

namespace Fooyin::Filters {
void FilterDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    QStyle* style = option.widget ? option.widget->style() : qApp->style();

    painter->save();

    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, option.widget);

    painter->restore();
}

QSize FilterDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    const QStyle* style = option.widget ? option.widget->style() : qApp->style();

    const QSize size = index.data(Qt::SizeHintRole).toSize();

    const int margin = style->pixelMetric(QStyle::PM_FocusFrameHMargin) * 2;
    const int width  = opt.fontMetrics.horizontalAdvance(opt.text) + margin;

    return {width, size.height()};
}
} // namespace Fooyin::Filters

#include "moc_filterdelegate.cpp"
