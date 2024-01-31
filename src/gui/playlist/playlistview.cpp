/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include "playlistview.h"

#include "playlistitem.h"

#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QScrollBar>
#include <QTimer>

#include <chrono>

using namespace std::chrono_literals;

namespace Fooyin {
PlaylistView::PlaylistView(QWidget* parent)
    : QTreeView{parent}
{
    setObjectName("PlaylistView");
    setupView();

    QObject::connect(&m_autoScrollTimer, &QTimer::timeout, this, &PlaylistView::doAutoScroll);
}

void PlaylistView::setupView()
{
    setRootIsDecorated(false);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setMouseTracking(true);
    setItemsExpandable(false);
    viewport()->setAcceptDrops(true);
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::MoveAction);
    setDropIndicatorShown(true);
    setIndentation(0);
    setExpandsOnDoubleClick(false);
    setWordWrap(true);
    setTextElideMode(Qt::ElideLeft);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setSortingEnabled(false);
    header()->setSectionsClickable(true);
    header()->setContextMenuPolicy(Qt::CustomContextMenu);
}

void PlaylistView::dragMoveEvent(QDragMoveEvent* event)
{
    const QPoint pos        = event->position().toPoint();
    const QModelIndex index = indexAt(pos);

    event->ignore();

    if(event->modifiers() & Qt::ControlModifier) {
        event->setDropAction(Qt::CopyAction);
    }
    else {
        event->setDropAction(Qt::MoveAction);
    }

    if(index.isValid() && showDropIndicator()) {
        const QRect rect      = visualRect(index);
        const QRect rectLeft  = visualRect(index.sibling(index.row(), 0));
        const QRect rectRight = visualRect(index.sibling(index.row(), 0));
        dropIndicatorPos      = position(pos, rect, index);
        switch(dropIndicatorPos) {
            case(AboveItem):
                if(isIndexDropEnabled(index.parent())) {
                    dropIndicatorRect = QRect(rectLeft.left(), rectLeft.top(), rectRight.right() - rectLeft.left(), 0);
                    event->acceptProposedAction();
                }
                else {
                    dropIndicatorRect = {};
                }
                break;
            case(BelowItem):
                if(isIndexDropEnabled(index.parent())) {
                    dropIndicatorRect
                        = QRect(rectLeft.left(), rectLeft.bottom(), rectRight.right() - rectLeft.left(), 0);
                    event->acceptProposedAction();
                }
                else {
                    dropIndicatorRect = {};
                }
                break;
            case(OnItem):
                dropIndicatorRect = {};
                event->ignore();
                break;
            case(OnViewport):
                dropIndicatorRect = {};
                if(isIndexDropEnabled({})) {
                    event->acceptProposedAction();
                }
                break;
        }
    }
    else {
        dropIndicatorRect = {};
        dropIndicatorPos  = OnViewport;
        if(isIndexDropEnabled({})) {
            event->acceptProposedAction();
        }
    }

    viewport()->update();

    if(shouldAutoScroll(pos)) {
        startAutoScroll();
    }
}

void PlaylistView::mousePressEvent(QMouseEvent* event)
{
    const QModelIndex index = indexAt(event->position().toPoint());
    const auto type         = index.data(PlaylistItem::Type).toInt();
    if(index.isValid()) {
        if(type != PlaylistItem::Track) {
            setDragEnabled(true);
        }
        else {
            // Prevent drag-and-drop when first selecting items
            setDragEnabled(selectionModel()->isSelected(index));
        }
    }

    QTreeView::mousePressEvent(event);
}

void PlaylistView::dropEvent(QDropEvent* event)
{
    QModelIndex index = indexAt(event->position().toPoint());

    if(dragDropMode() == InternalMove) {
        if(event->source() != this || !(event->possibleActions() & Qt::MoveAction)) {
            return;
        }
    }

    int col = -1;
    int row = -1;
    if(dropOn(event, row, col, index)) {
        const Qt::DropAction action = dragDropMode() == InternalMove ? Qt::MoveAction : event->dropAction();
        if(model()->dropMimeData(event->mimeData(), action, row, col, index)) {
            if(action != event->dropAction()) {
                event->setDropAction(action);
                event->accept();
            }
            else {
                event->acceptProposedAction();
            }
        }
    }
    stopAutoScroll();
    setState(NoState);
    viewport()->update();
}

void PlaylistView::drawBranches(QPainter* /*painter*/, const QRect& /*rect*/, const QModelIndex& /*index*/) const { }

void PlaylistView::drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if(model()->hasChildren(index)) {
        // Span first column of headers/subheaders
        // Used instead of setFirstColumnSpanned to account for header section moves
        if(index.column() == 0) {
            QStyleOptionViewItem opt = option;

            QRect rect = option.rect;

            for(int i{1}; i < model()->columnCount(); ++i) {
                rect.setRight(rect.right() + columnWidth(i));
            }

            itemDelegateForIndex(index)->paint(painter, opt, index);
        }
    }
    else {
        QTreeView::drawRow(painter, option, index);
    }
}

void PlaylistView::paintEvent(QPaintEvent* event)
{
    QPainter painter{viewport()};

    if(model() && model()->rowCount() > 0) {
        drawTree(&painter, event->region());
        if(state() == QAbstractItemView::DraggingState) {
            QStyleOptionFrame opt;
            initStyleOption(&opt);
            opt.rect = dropIndicatorRect;
            //            painter.setRenderHint(QPainter::Antialiasing, true);
            //            const QBrush brush(Qt::green);
            //            const QPen pen{brush, 4, Qt::DashLine};
            //            painter.setPen(pen);
            style()->drawPrimitive(QStyle::PE_IndicatorItemViewItemDrop, &opt, &painter);
        }
    }
    else {
        // Empty playlist
        const QString text{tr("Empty Playlist")};

        QRect textRect = painter.fontMetrics().boundingRect(text);
        textRect.moveCenter(viewport()->rect().center());
        painter.drawText(textRect, Qt::AlignCenter, text);
    }
}

QAbstractItemView::DropIndicatorPosition PlaylistView::position(const QPoint& pos, const QRect& rect,
                                                                const QModelIndex& index) const
{
    auto dropPos    = QAbstractItemView::OnViewport;
    const int mid   = static_cast<int>(std::round(static_cast<double>((rect.height())) / 2));
    const auto type = index.data(PlaylistItem::Type);

    if(type == PlaylistItem::Subheader) {
        dropPos = QAbstractItemView::OnItem;
    }
    else if(pos.y() - rect.top() < mid || type == PlaylistItem::Header) {
        dropPos = QAbstractItemView::AboveItem;
    }
    else if(rect.bottom() - pos.y() < mid) {
        dropPos = QAbstractItemView::BelowItem;
    }

    return dropPos;
}

bool PlaylistView::isIndexDropEnabled(const QModelIndex& index) const
{
    return (model()->flags(index) & Qt::ItemIsDropEnabled);
}

bool PlaylistView::shouldAutoScroll(const QPoint& pos) const
{
    if(!hasAutoScroll()) {
        return false;
    }
    const QRect area = viewport()->rect();
    return (pos.y() - area.top() < autoScrollMargin()) || (area.bottom() - pos.y() < autoScrollMargin())
        || (pos.x() - area.left() < autoScrollMargin()) || (area.right() - pos.x() < autoScrollMargin());
}

void PlaylistView::startAutoScroll()
{
    m_autoScrollTimer.start(50ms);
}

void PlaylistView::stopAutoScroll()
{
    m_autoScrollTimer.stop();
    m_autoScrollCount = 0;
}

void PlaylistView::doAutoScroll()
{
    QScrollBar* scroll = verticalScrollBar();
    if(m_autoScrollCount < scroll->pageStep()) {
        ++m_autoScrollCount;
    }

    const int value  = scroll->value();
    const QPoint pos = viewport()->mapFromGlobal(QCursor::pos());
    const QRect area = viewport()->rect();

    if(pos.y() - area.top() < autoScrollMargin()) {
        scroll->setValue(value - m_autoScrollCount);
    }
    else if(area.bottom() - pos.y() < autoScrollMargin()) {
        scroll->setValue(value + m_autoScrollCount);
    }
    const bool verticalUnchanged = value == scroll->value();
    if(verticalUnchanged) {
        stopAutoScroll();
    }
}

bool PlaylistView::dropOn(QDropEvent* event, int& dropRow, int& dropCol, QModelIndex& dropIndex)
{
    if(event->isAccepted()) {
        return false;
    }
    QModelIndex index;
    const QPoint pos = event->position().toPoint();
    if(viewport()->rect().contains(pos)) {
        index = indexAt(pos);
        if(!index.isValid() || !visualRect(index).contains(pos)) {
            index = {};
        }
    }
    // If we are allowed to do the drop
    if(model()->supportedDropActions() & event->dropAction()) {
        int row = -1;
        int col = -1;
        if(index.isValid()) {
            dropIndicatorPos = position(pos, visualRect(index), index);
            switch(dropIndicatorPos) {
                case(QAbstractItemView::AboveItem):
                    row   = index.row();
                    col   = index.column();
                    index = index.parent();
                    break;
                case(QAbstractItemView::BelowItem):
                    row   = index.row() + 1;
                    col   = index.column();
                    index = index.parent();
                    break;
                case(QAbstractItemView::OnItem):
                case(QAbstractItemView::OnViewport):
                    break;
            }
        }
        else {
            dropIndicatorPos = QAbstractItemView::OnViewport;
            row              = model()->rowCount({});
        }
        dropIndex = index;
        dropRow   = row;
        dropCol   = col;
        return true;
    }
    return false;
}
} // namespace Fooyin

#include "moc_playlistview.cpp"
