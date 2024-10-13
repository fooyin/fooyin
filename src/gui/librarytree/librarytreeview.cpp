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

#include "librarytreeview.h"

#include <QHeaderView>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QWheelEvent>

namespace Fooyin {
LibraryTreeView::LibraryTreeView(QWidget* parent)
    : QTreeView{parent}
    , m_isLoading{false}
{
    setUniformRowHeights(false);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setHeaderHidden(true);
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    setSortingEnabled(true);

    header()->setSortIndicatorShown(false);
    header()->setSectionsClickable(false);
    header()->setSortIndicator(0, Qt::AscendingOrder);
    header()->setContextMenuPolicy(Qt::CustomContextMenu);
    setTextElideMode(Qt::ElideRight);
}

void LibraryTreeView::setLoading(bool isLoading)
{
    m_isLoading = isLoading;
    viewport()->update();
}

void LibraryTreeView::mousePressEvent(QMouseEvent* event)
{
    QTreeView::mousePressEvent(event);

    const QModelIndex index = indexAt(event->position().toPoint());

    if(!index.isValid()) {
        clearSelection();
    }

    if(event->button() == Qt::MiddleButton) {
        emit middleClicked(index);
    }
}

void LibraryTreeView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if(event->button() == Qt::MiddleButton) {
        return;
    }
    QTreeView::mouseDoubleClickEvent(event);
}

void LibraryTreeView::wheelEvent(QWheelEvent* event)
{
    if(!(event->modifiers() & Qt::ControlModifier)) {
        QAbstractItemView::wheelEvent(event);
        return;
    }

    const int delta     = event->angleDelta().y();
    const int increment = (delta > 0) ? 1 : -1;
    int newSize         = iconSize().width() + increment * 2;
    newSize             = std::clamp(newSize, 16, 1024);
    setIconSize({newSize, newSize});

    event->accept();
}

void LibraryTreeView::paintEvent(QPaintEvent* event)
{
    QPainter painter{viewport()};

    auto drawCentreText = [this, &painter](const QString& text) {
        QRect textRect = painter.fontMetrics().boundingRect(text);
        textRect.moveCenter(viewport()->rect().center());
        painter.drawText(textRect, Qt::AlignCenter, text);
    };

    if(m_isLoading) {
        drawCentreText(tr("Loading Library…"));
        return;
    }

    QTreeView::paintEvent(event);
}

QModelIndex LibraryTreeView::moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers)
{
    const QModelIndex current = currentIndex();

    if(cursorAction == MoveLeft) {
        if(isExpanded(current)) {
            collapse(current);
            return current;
        }
        if(current.parent().isValid()) {
            // Move to parent if collapsed
            return current.parent();
        }
    }

    return QTreeView::moveCursor(cursorAction, modifiers);
}
} // namespace Fooyin

#include "moc_librarytreeview.cpp"
