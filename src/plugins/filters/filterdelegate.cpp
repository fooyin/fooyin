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

#include <utils/utils.h>

#include <QApplication>
#include <QPainter>

namespace Fooyin::Filters {
void FilterDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);

    // initStyleOption will override decorationSize when converting the QPixmap to a QIcon, so restore it here
    opt.decorationSize = option.decorationSize;
    const auto image   = index.data(Qt::DecorationRole).value<QPixmap>();
    if(!image.isNull()) {
        opt.icon = Utils::scalePixmap(image, opt.decorationSize, opt.widget->devicePixelRatioF(), true);
    }

    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();

    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
}

QSize FilterDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);

    opt.decorationSize = option.decorationSize;

    const QWidget* widget = opt.widget;
    const QStyle* style   = widget ? widget->style() : QApplication::style();

    QSize size = style->sizeFromContents(QStyle::CT_ItemViewItem, &opt, {}, widget);

    const QSize sizeHint = index.data(Qt::SizeHintRole).toSize();
    if(sizeHint.height() > 0) {
        size.setHeight(sizeHint.height());
    }

    return size;
}
} // namespace Fooyin::Filters

#include "moc_filterdelegate.cpp"
