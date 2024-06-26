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

#include "infoview.h"

#include <QPainter>

namespace Fooyin {
InfoView::InfoView(QWidget* parent)
    : QTreeView{parent}
{ }

void InfoView::paintEvent(QPaintEvent* event)
{
    if(model() && model()->rowCount({}) == 0) {
        QPainter painter{viewport()};
        const QString text = tr("No Selection");
        QRect textRect     = painter.fontMetrics().boundingRect(text);
        textRect.moveCenter(viewport()->rect().center());
        painter.drawText(textRect, Qt::AlignCenter, text);
    }

    QTreeView::paintEvent(event);
}

void InfoView::drawBranches(QPainter* /*painter*/, const QRect& /*rect*/, const QModelIndex& /*index*/) const { }
} // namespace Fooyin
