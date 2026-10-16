/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <gui/widgets/widgetbackground.h>

#include <QHeaderView>
#include <QMouseEvent>
#include <QPainter>

namespace Fooyin {
QueueViewerView::QueueViewerView(QWidget* parent)
    : ExpandedTreeView{parent}
    , m_background{new WidgetBackground(viewport())}
{
    setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
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

void QueueViewerView::setBackgroundConfig(const BackgroundConfig& config)
{
    m_background->setConfig(config);
    setProperty("transparent_base_rows", m_background->hasImage());
}

void QueueViewerView::setBackgroundPixmap(const QPixmap& pixmap)
{
    m_background->setPixmap(pixmap);
    setProperty("transparent_base_rows", m_background->hasImage());
}

void QueueViewerView::paintEvent(QPaintEvent* event)
{
    QPainter painter{viewport()};
    painter.fillRect(viewport()->rect(), palette().brush(QPalette::Base));
    m_background->draw(painter);

    if(!model() || model()->rowCount({}) <= 0) {
        const QString text = tr("Empty Queue");
        QRect textRect     = painter.fontMetrics().boundingRect(text);
        textRect.moveCenter(viewport()->rect().center());
        painter.drawText(textRect, Qt::AlignCenter, text);
        return;
    }

    painter.end();
    ExpandedTreeView::paintEvent(event);
}
} // namespace Fooyin
