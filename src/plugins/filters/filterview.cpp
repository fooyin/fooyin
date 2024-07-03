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

#include "filterview.h"

#include <QHeaderView>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>

namespace Fooyin::Filters {
FilterView::FilterView(QWidget* parent)
    : ExpandedTreeView{parent}
{
    setObjectName(QStringLiteral("FilterView"));

    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setTextElideMode(Qt::ElideRight);
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    setDefaultDropAction(Qt::CopyAction);
    setDropIndicatorShown(true);
    setUniformRowHeights(true);
    setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
}

void FilterView::mousePressEvent(QMouseEvent* event)
{
    const QModelIndex index = indexAt(event->position().toPoint());

    if(index.isValid()) {
        // Prevent drag-and-drop when first selecting items
        setDragEnabled(selectionModel()->isSelected(index));
    }

    ExpandedTreeView::mousePressEvent(event);

    if(!index.isValid()) {
        clearSelection();
    }

    if(event->button() == Qt::MiddleButton) {
        emit middleClicked();
    }
}

void FilterView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if(event->button() == Qt::MiddleButton) {
        return;
    }

    ExpandedTreeView::mouseDoubleClickEvent(event);
}
} // namespace Fooyin::Filters

#include "moc_filterview.cpp"
