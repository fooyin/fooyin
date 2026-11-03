/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "filterexpansiontrackview.h"

#include "filterexpansiontrackdelegate.h"
#include "scripting/inlinetrackeditors.h"

#include <utils/tooltipfilter.h>

#include <QContextMenuEvent>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QStyleOptionViewItem>

namespace Fooyin::Filters {
ExpansionTrackView::ExpansionTrackView(QWidget* parent)
    : ExpandedTreeView{parent}
    , m_delegate{new ExpansionTrackDelegate(this)}
    , m_previewMode{false}
{
    setItemDelegate(m_delegate);
    setMouseTracking(true);
    setViewMode(ViewMode::Flow);
    setFrameShape(NoFrame);
    setAutoFillBackground(false);
    setProperty("transparent_base_rows", true);
    setHeaderHidden(true);
    setIndentation(0);
    setFlowMinimumRows(4);
    setFlowStretchColumns(false);
    setUniformRowHeights(false);
    setSelectIgnoreParents(true);
    setSelectionBehavior(SelectRows);
    setSelectionMode(ExtendedSelection);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollMode(ScrollPerPixel);
    setEditTriggers(NoEditTriggers);
    setDragEnabled(true);
    setDragDropMode(DragOnly);
    setDefaultDropAction(Qt::CopyAction);
    header()->setSectionResizeMode(QHeaderView::Stretch);
    viewport()->setAutoFillBackground(false);
    viewport()->installEventFilter(new ToolTipFilter(this));

    QObject::connect(this, &ExpandedTreeView::doubleClicked, this, &ExpansionTrackView::activateTrack);
    QObject::connect(this, &ExpandedTreeView::middleClicked, this, [this](const QModelIndex& index) {
        if(index.isValid() && !m_previewMode) {
            Q_EMIT selectionUpdated();
            Q_EMIT trackMiddleClicked(index);
        }
    });
}

void ExpansionTrackView::setPreviewMode()
{
    m_previewMode = true;
    clearEditorHover();
    setDragEnabled(false);
    setSelectionMode(NoSelection);
    setFocusPolicy(Qt::NoFocus);
}

void ExpansionTrackView::reset()
{
    clearEditorHover();
    ExpandedTreeView::reset();
}

void ExpansionTrackView::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    ExpandedTreeView::selectionChanged(selected, deselected);
    Q_EMIT selectionUpdated();
}

void ExpansionTrackView::currentChanged(const QModelIndex& current, const QModelIndex& previous)
{
    ExpandedTreeView::currentChanged(current, previous);
    if(current.isValid() && hasFocus()) {
        Q_EMIT trackFocused(visualRect(current));
    }
}

void ExpansionTrackView::keyPressEvent(QKeyEvent* event)
{
    if(!m_previewMode) {
        if(event->key() == Qt::Key_Escape) {
            Q_EMIT closeRequested();
            event->accept();
            return;
        }
        if(event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            Q_EMIT trackActivated(currentIndex());
            event->accept();
            return;
        }
    }
    ExpandedTreeView::keyPressEvent(event);
}

void ExpansionTrackView::contextMenuEvent(QContextMenuEvent* event)
{
    if(!m_previewMode) {
        Q_EMIT requestContextMenu(event->globalPos(), selectionModel()->hasSelection());
        event->accept();
        return;
    }
    ExpandedTreeView::contextMenuEvent(event);
}

void ExpansionTrackView::mouseMoveEvent(QMouseEvent* event)
{
    if(!m_previewMode) {
        const QModelIndex index = indexAt(event->pos());
        const auto hit          = editorAt(index, event->pos());
        m_delegate->setHover(index, hit, event->pos());
        m_delegate->setHoveredRow(index);
        setCursor(hit.type != InlineTrackEditor::None ? Qt::PointingHandCursor : Qt::ArrowCursor);
        viewport()->update();
    }
    ExpandedTreeView::mouseMoveEvent(event);
}

void ExpansionTrackView::mousePressEvent(QMouseEvent* event)
{
    if(!m_previewMode && event->button() == Qt::LeftButton) {
        const QModelIndex index = indexAt(event->pos());
        const auto hit          = editorAt(index, event->pos());
        if(hit.type != InlineTrackEditor::None) {
            m_delegate->setHover(index, hit, event->pos());
            Q_EMIT editorClicked(index, hit, event->pos());
        }
    }
    ExpandedTreeView::mousePressEvent(event);
}

void ExpansionTrackView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if(!m_previewMode && event->button() == Qt::LeftButton
       && editorAt(indexAt(event->pos()), event->pos()).type != InlineTrackEditor::None) {
        event->accept();
        return;
    }
    ExpandedTreeView::mouseDoubleClickEvent(event);
}

bool ExpansionTrackView::viewportEvent(QEvent* event)
{
    if(event->type() == QEvent::Leave) {
        clearEditorHover();
    }
    return ExpandedTreeView::viewportEvent(event);
}

InlineTrackEditorHit ExpansionTrackView::editorAt(const QModelIndex& index, const QPoint& pos) const
{
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    option.rect = visualRect(index);
    return m_delegate->inlineEditorAt(option, index, pos);
}

void ExpansionTrackView::activateTrack(const QModelIndex& index)
{
    if(!m_previewMode && index.isValid()) {
        Q_EMIT trackActivated(index);
    }
}

void ExpansionTrackView::clearEditorHover()
{
    m_delegate->clearHover();
    unsetCursor();
    viewport()->update();
}
} // namespace Fooyin::Filters

#include "moc_filterexpansiontrackview.cpp"
