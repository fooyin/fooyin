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

#include "lyricsmodel.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QWheelEvent>

using namespace Qt::StringLiterals;

namespace Fooyin::Lyrics {
LyricsView::LyricsView(QWidget* parent)
    : QListView{parent}
    , m_leftButtonDown{false}
    , m_dragSeeking{false}
    , m_suppressContextMenu{false}
    , m_suppressNextLeftRelease{false}
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

bool LyricsView::isDragSeeking() const
{
    return m_dragSeeking;
}

void LyricsView::contextMenuEvent(QContextMenuEvent* event)
{
    if(m_suppressContextMenu) {
        m_suppressContextMenu = false;
        event->accept();
        return;
    }

    QListView::contextMenuEvent(event);
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

        if(m_dragSeeking && isSeekableIndex(m_dragIndex)) {
            // TODO: Make configurable
            QPen pen{palette().color(QPalette::Text)};
            pen.setStyle(Qt::DotLine);
            pen.setDashPattern({2.0, 4.0});
            pen.setCosmetic(true);
            pen.setWidth(1);

            QPainter painter{viewport()};
            painter.setOpacity(0.6);
            painter.setPen(pen);

            const int guideY = std::clamp(m_dragPos.y(), 0, viewport()->height() - 1);
            painter.drawLine(0, guideY, viewport()->width(), guideY);
        }
    }
}

void LyricsView::mousePressEvent(QMouseEvent* event)
{
    if(event->button() == Qt::RightButton && m_dragSeeking) {
        clearDragPreview();
        m_leftButtonDown          = false;
        m_suppressContextMenu     = true;
        m_suppressNextLeftRelease = true;
        event->accept();
        return;
    }

    QListView::mousePressEvent(event);

    if(event->button() != Qt::LeftButton || !m_displayString.isEmpty()) {
        event->ignore();
        return;
    }

    const QModelIndex index = seekableIndexAt(event->position().toPoint());
    if(!isSeekableIndex(index)) {
        return;
    }

    m_leftButtonDown = true;
    m_dragSeeking    = false;
    m_pressPos       = event->position().toPoint();
    m_dragPos        = m_pressPos;
    m_dragIndex      = index;
}

void LyricsView::mouseMoveEvent(QMouseEvent* event)
{
    QListView::mouseMoveEvent(event);

    if(!m_leftButtonDown || !(event->buttons() & Qt::LeftButton) || !m_displayString.isEmpty()) {
        return;
    }

    const QPoint pos = event->position().toPoint();
    if(!m_dragSeeking && (m_pressPos - pos).manhattanLength() < QApplication::startDragDistance()) {
        return;
    }

    if(!m_dragSeeking) {
        m_dragSeeking = true;
        emit dragSeekingChanged(true);
    }

    emit userScrolling();
    updateDragPreview(pos);
}

void LyricsView::mouseReleaseEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton && m_suppressNextLeftRelease) {
        m_suppressNextLeftRelease = false;
        event->accept();
        return;
    }

    QListView::mouseReleaseEvent(event);

    if(event->button() != Qt::LeftButton) {
        return;
    }

    const QPoint pos = event->position().toPoint();
    if(m_dragSeeking) {
        updateDragPreview(pos);
        emit lineDragSeekRequested(m_dragIndex, pos);
        clearDragPreview();
        m_leftButtonDown = false;
        return;
    }

    const QModelIndex index = indexAt(pos);
    emit lineClicked(index, pos);

    m_leftButtonDown = false;
    m_dragIndex      = QPersistentModelIndex{};
}

void LyricsView::resizeEvent(QResizeEvent* event)
{
    QListView::resizeEvent(event);
    emit viewportResized();
}

void LyricsView::wheelEvent(QWheelEvent* event)
{
    emit userScrolling();
    QListView::wheelEvent(event);
}

QModelIndex LyricsView::seekableIndexAt(const QPoint& pos) const
{
    const auto* model = this->model();
    if(!model || model->rowCount() <= 2) {
        return {};
    }

    const QModelIndex index = indexAt(pos);
    if(isSeekableIndex(index)) {
        return index;
    }

    if(index.isValid() && index.data(LyricsModel::IsPaddingRole).toBool()) {
        if(index.row() == 0) {
            return model->index(1, 0);
        }
        if(index.row() == model->rowCount({}) - 1) {
            return model->index(model->rowCount({}) - 2, 0);
        }
    }

    if(pos.y() < viewport()->rect().center().y()) {
        return model->index(1, 0);
    }

    return model->index(model->rowCount() - 2, 0);
}

bool LyricsView::isSeekableIndex(const QModelIndex& index) const
{
    return index.isValid() && !index.data(LyricsModel::IsPaddingRole).toBool();
}

void LyricsView::updateDragPreview(const QPoint& pos)
{
    const QModelIndex index = seekableIndexAt(pos);
    if(!isSeekableIndex(index)) {
        return;
    }

    m_dragPos   = pos;
    m_dragIndex = index;

    viewport()->update();
}

void LyricsView::clearDragPreview()
{
    const bool wasDragSeeking = std::exchange(m_dragSeeking, false);

    m_dragIndex = QPersistentModelIndex{};
    m_dragPos   = {};

    viewport()->update();

    if(wasDragSeeking) {
        emit dragSeekingChanged(false);
    }
}
} // namespace Fooyin::Lyrics
