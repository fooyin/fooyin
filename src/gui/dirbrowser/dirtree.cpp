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

#include "dirtree.h"

#include <QHeaderView>
#include <QMouseEvent>
#include <QScrollBar>

namespace Fooyin {
DirTree::DirTree(QWidget* parent)
    : QTreeView{parent}
    , m_elideText{true}
{
    setUniformRowHeights(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    setDefaultDropAction(Qt::CopyAction);
    setHeaderHidden(true);
    setTextElideMode(Qt::ElideRight);
    setAllColumnsShowFocus(true);
    header()->setStretchLastSection(true);

    QObject::connect(this, &QTreeView::expanded, this, &DirTree::resizeView);
    QObject::connect(this, &QTreeView::collapsed, this, &DirTree::resizeView);
}

void DirTree::resizeView()
{
    if(!m_elideText) {
        resizeColumnToContents(0);
    }
    else {
        setColumnWidth(0, 1);
    }
}

void DirTree::setElideText(bool enabled)
{
    if(std::exchange(m_elideText, enabled) != enabled) {
        resizeView();
    }
}

void DirTree::resizeEvent(QResizeEvent* event)
{
    QTreeView::resizeEvent(event);
    resizeView();
}

void DirTree::mousePressEvent(QMouseEvent* event)
{
    const auto button = event->button();

    if(button == Qt::ForwardButton) {
        emit forwardClicked();
    }
    else if(button == Qt::BackButton) {
        emit backClicked();
    }
    else if(button == Qt::MiddleButton) {
        QTreeView::mousePressEvent(event);
        emit middleClicked();
    }
    else {
        QTreeView::mousePressEvent(event);
    }
}

void DirTree::mouseDoubleClickEvent(QMouseEvent* event)
{
    if(event->button() != Qt::LeftButton) {
        return;
    }

    QTreeView::mouseDoubleClickEvent(event);
}
} // namespace Fooyin

#include "moc_dirtree.cpp"
