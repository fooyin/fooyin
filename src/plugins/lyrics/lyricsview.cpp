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

#include "lyricsview.h"

#include <QApplication>
#include <QPainter>
#include <QWheelEvent>

using namespace Qt::StringLiterals;

namespace Fooyin::Lyrics {
LyricsView::LyricsView(QWidget* parent)
    : QListView{parent}
{
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSelectionMode(QAbstractItemView::NoSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setFocusPolicy(Qt::NoFocus);
    setUniformItemSizes(false);
    setResizeMode(QListView::Adjust);
    setWrapping(false);
    setFlow(QListView::TopToBottom);
    setFrameShape(QFrame::NoFrame);
    viewport()->setAutoFillBackground(true);
}

void LyricsView::setDisplayString(const QString& string)
{
    m_displayString = string;
    viewport()->update();
}

void LyricsView::paintEvent(QPaintEvent* event)
{
    if(!m_displayString.isEmpty()) {
        QPainter painter{viewport()};
        QRect textRect = painter.fontMetrics().boundingRect(viewport()->rect(), Qt::AlignCenter, m_displayString);
        textRect.moveCenter(viewport()->rect().center());
        painter.drawText(textRect, Qt::AlignCenter, m_displayString);
    }
    else {
        QListView::paintEvent(event);
    }
}

void LyricsView::mouseReleaseEvent(QMouseEvent* event)
{
    QListView::mouseReleaseEvent(event);

    const QPoint pos        = event->position().toPoint();
    const QModelIndex index = indexAt(pos);

    emit lineClicked(index, pos);
}

void LyricsView::wheelEvent(QWheelEvent* event)
{
    emit userScrolling();
    QListView::wheelEvent(event);
}
} // namespace Fooyin::Lyrics
