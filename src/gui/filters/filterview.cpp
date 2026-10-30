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

#include "filterview.h"

#include <gui/widgets/widgetbackground.h>

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QTimerEvent>

namespace Fooyin::Filters {
FilterView::FilterView(QWidget* parent)
    : ExpandedTreeView{parent}
    , m_background{new WidgetBackground(viewport(), this)}
{
    viewport()->setAutoFillBackground(false);

    QObject::connect(this, &ExpandedTreeView::clicked, this, [this](const QModelIndex& index) {
        if(m_collapseIndex.isValid() && index.siblingAtColumn(0) == m_collapseIndex) {
            m_collapseTimer.start(QApplication::doubleClickInterval(), Qt::PreciseTimer, this);
        }
    });
}

void FilterView::cancelPendingCollapse()
{
    m_collapseTimer.stop();
    m_collapseIndex = QModelIndex{};
}

void FilterView::setBackgroundConfig(const BackgroundConfig& config)
{
    m_background->setConfig(config);
}

void FilterView::setBackgroundPixmap(const QPixmap& pixmap)
{
    m_background->setPixmap(pixmap);
}

void FilterView::mousePressEvent(QMouseEvent* event)
{
    cancelPendingCollapse();

    if(event->button() == Qt::LeftButton && event->modifiers() == Qt::NoModifier && expandedIndex().isValid()
       && indexAt(event->position().toPoint()).siblingAtColumn(0) == expandedIndex()
       && selectionModel()->selectedRows().size() == 1) {
        m_collapseIndex = expandedIndex();
    }

    ExpandedTreeView::mousePressEvent(event);
}

void FilterView::mouseReleaseEvent(QMouseEvent* event)
{
    if(event->modifiers() != Qt::NoModifier) {
        cancelPendingCollapse();
    }
    ExpandedTreeView::mouseReleaseEvent(event);
}

void FilterView::mouseDoubleClickEvent(QMouseEvent* event)
{
    cancelPendingCollapse();
    ExpandedTreeView::mouseDoubleClickEvent(event);
}

void FilterView::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_collapseTimer.timerId()) {
        const bool collapse = m_collapseIndex.isValid() && m_collapseIndex == expandedIndex();
        cancelPendingCollapse();
        if(collapse) {
            Q_EMIT collapseRequested();
        }
        return;
    }

    ExpandedTreeView::timerEvent(event);
}

void FilterView::paintEvent(QPaintEvent* event)
{
    {
        QPainter painter{viewport()};
        painter.fillRect(viewport()->rect(), palette().brush(QPalette::Base));
        m_background->draw(painter);
    }
    ExpandedTreeView::paintEvent(event);
}

void FilterView::changeEvent(QEvent* event)
{
    ExpandedTreeView::changeEvent(event);

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
} // namespace Fooyin::Filters

#include "moc_filterview.cpp"
