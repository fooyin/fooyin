/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include <gui/contextmenuutils.h>
#include <gui/guiutils.h>
#include <gui/widgets/widgetbackground.h>

#include <QHeaderView>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QScopedValueRollback>
#include <QWheelEvent>

namespace Fooyin {
LibraryTreeView::LibraryTreeView(QWidget* parent)
    : QTreeView{parent}
    , m_background{new WidgetBackground(viewport(), this)}
    , m_backgroundPalette{palette()}
    , m_updatingBackgroundPalette{false}
    , m_isLoading{false}
    , m_expandsOnSingleClick{false}
{
    viewport()->setAutoFillBackground(false);

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

void LibraryTreeView::setExpandsOnSingleClick(bool enabled)
{
    m_expandsOnSingleClick = enabled;
}

void LibraryTreeView::setBackgroundConfig(const BackgroundConfig& config)
{
    const QScopedValueRollback guard{m_updatingBackgroundPalette, true};
    m_background->setConfig(config);
    updateBackgroundPalette();
}

void LibraryTreeView::setBackgroundPixmap(const QPixmap& pixmap)
{
    const QScopedValueRollback guard{m_updatingBackgroundPalette, true};
    m_background->setPixmap(pixmap);
    updateBackgroundPalette();
}

void LibraryTreeView::setViewPalette(const QPalette& palette)
{
    m_backgroundPalette = palette;
    updateBackgroundPalette();
}

void LibraryTreeView::initViewItemOption(QStyleOptionViewItem* option) const
{
    QTreeView::initViewItemOption(option);

    if(m_background->hasImage()) {
        WidgetBackground::makeRowPaletteTransparent(option->palette);
    }
}

void LibraryTreeView::changeEvent(QEvent* event)
{
    if(m_updatingBackgroundPalette && event->type() == QEvent::PaletteChange) {
        QTreeView::changeEvent(event);
        return;
    }

    switch(event->type()) {
        case QEvent::FontChange:
        case QEvent::PaletteChange:
        case QEvent::StyleChange:
            Q_EMIT displayAboutToChange();
            break;
        default:
            break;
    }

    QTreeView::changeEvent(event);

    switch(event->type()) {
        case QEvent::FontChange:
        case QEvent::PaletteChange:
        case QEvent::StyleChange:
            Q_EMIT displayChanged();
            break;
        default:
            break;
    }
}

void LibraryTreeView::mousePressEvent(QMouseEvent* event)
{
    const auto modifierGuard = ContextMenuUtils::normaliseContextMenuModifiers(event);

    if(event->button() > Qt::MiddleButton) {
        event->ignore();
        return;
    }

    const QModelIndex index = indexAt(event->position().toPoint());

    m_singleClickToggleIndex = QPersistentModelIndex{};

    if(!index.isValid()) {
        clearSelection();
    }

    if(event->button() == Qt::MiddleButton) {
        QTreeView::mousePressEvent(event);
        Q_EMIT middleClicked(index);
        return;
    }

    if(m_expandsOnSingleClick && event->button() == Qt::LeftButton && index.isValid() && model()->hasChildren(index)
       && event->modifiers() == Qt::NoModifier) {
        const QRect indexRect = visualRect(index);
        if(event->position().toPoint().x() >= indexRect.left()) {
            m_singleClickToggleIndex = index;
        }
    }

    QTreeView::mousePressEvent(event);
}

void LibraryTreeView::mouseReleaseEvent(QMouseEvent* event)
{
    QTreeView::mouseReleaseEvent(event);

    if(m_expandsOnSingleClick && event->button() == Qt::LeftButton && m_singleClickToggleIndex.isValid()
       && m_singleClickToggleIndex == indexAt(event->position().toPoint())) {
        setExpanded(m_singleClickToggleIndex, !isExpanded(m_singleClickToggleIndex));
    }

    m_singleClickToggleIndex = QPersistentModelIndex{};
}

void LibraryTreeView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if(event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }

    m_singleClickToggleIndex = QPersistentModelIndex{};

    if(m_expandsOnSingleClick) {
        const QModelIndex index = indexAt(event->position().toPoint());
        if(index.isValid()) {
            if(expandsOnDoubleClick() && model()->hasChildren(index)) {
                setExpanded(index, !isExpanded(index));
            }
            Q_EMIT doubleClicked(index);
        }
        event->accept();
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
    int newSize         = iconSize().width() + (increment * 2);
    newSize             = std::clamp(newSize, 16, 1024);
    setIconSize({newSize, newSize});

    event->accept();
}

void LibraryTreeView::paintEvent(QPaintEvent* event)
{
    {
        QPainter painter{viewport()};
        painter.fillRect(viewport()->rect(), m_backgroundPalette.brush(QPalette::Base));
        m_background->draw(painter);
    }

    if(m_isLoading) {
        QPainter painter{viewport()};

        const QString text = tr("Loading Library…");
        QRect textRect     = painter.fontMetrics().boundingRect(text);
        textRect.moveCenter(viewport()->rect().center());

        painter.drawText(textRect, Qt::AlignCenter, text);
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

void LibraryTreeView::updateBackgroundPalette()
{
    const bool hasImage = m_background->hasImage();

    QPalette viewPalette{m_backgroundPalette};
    if(hasImage) {
        // QTreeView fills its animation buffers with the view's base brush
        viewPalette.setBrush(QPalette::Base, Qt::NoBrush);
    }
    Gui::refreshItemViewPalette(this, viewPalette);
}
} // namespace Fooyin

#include "moc_librarytreeview.cpp"
