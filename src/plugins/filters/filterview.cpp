/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

namespace Fooyin::Filters {
FilterView::FilterView(QWidget* parent)
    : QTreeView(parent)
{
    setObjectName("FilterView");

    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setMouseTracking(true);
    setRootIsDecorated(false);
    setItemsExpandable(false);
    setIndentation(0);
    setExpandsOnDoubleClick(false);
    setWordWrap(true);
    setTextElideMode(Qt::ElideLeft);
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    setDefaultDropAction(Qt::CopyAction);
    setDropIndicatorShown(true);
    setSortingEnabled(false);
    setUniformRowHeights(true);
}

void FilterView::mousePressEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton && event->modifiers() == Qt::NoModifier) {
        selectionModel()->clear();
    }
    QTreeView::mousePressEvent(event);

    if(event->button() == Qt::MiddleButton) {
        emit middleClicked();
    }
}

void FilterView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if(event->button() == Qt::MiddleButton) {
        return;
    }
    QTreeView::mouseDoubleClickEvent(event);
}

void FilterView::keyPressEvent(QKeyEvent* event)
{
    const auto key = event->key();

    if(key == Qt::Key_Enter || key == Qt::Key_Return) { }
    QTreeView::keyPressEvent(event);
}
} // namespace Fooyin::Filters

#include "moc_filterview.cpp"
