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

#include "dirdelegate.h"

#include <QApplication>
#include <QPainter>

namespace Fooyin {
void DirDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    QStyle* style = option.widget ? option.widget->style() : QApplication::style();

    painter->save();

    if(opt.backgroundBrush.style() != Qt::NoBrush) {
        painter->fillRect(option.rect, opt.backgroundBrush);
        opt.backgroundBrush = {};
    }
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, option.widget);

    painter->restore();
}
} // namespace Fooyin
