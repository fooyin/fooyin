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

#include "queueviewerview.h"

#include <QHeaderView>
#include <QMouseEvent>
#include <QPainter>

namespace Fooyin {
QueueViewerView::QueueViewerView(QWidget* parent)
    : ExpandedTreeView{parent}
{
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setTextElideMode(Qt::ElideRight);
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::MoveAction);
    setDropIndicatorShown(true);
    setUniformRowHeights(true);

    header()->setStretchLastSection(true);
}

void QueueViewerView::mousePressEvent(QMouseEvent* event)
{
    ExpandedTreeView::mousePressEvent(event);

    if(!indexAt(event->position().toPoint()).isValid()) {
        clearSelection();
    }

    if(event->button() == Qt::MiddleButton) {
        emit middleClicked();
    }
}

void QueueViewerView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if(event->button() == Qt::MiddleButton) {
        return;
    }
    ExpandedTreeView::mouseDoubleClickEvent(event);
}

void QueueViewerView::paintEvent(QPaintEvent* event)
{
    QPainter painter{viewport()};

    auto drawCentreText = [this, &painter](const QString& text) {
        QRect textRect = painter.fontMetrics().boundingRect(text);
        textRect.moveCenter(viewport()->rect().center());
        painter.drawText(textRect, Qt::AlignCenter, text);
    };

    if(model()->rowCount({}) <= 0) {
        drawCentreText(tr("Empty Queue"));
        return;
    }

    ExpandedTreeView::paintEvent(event);
}
} // namespace Fooyin
