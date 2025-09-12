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

#include "infoview.h"

#include "infoitem.h"

#include <QHeaderView>
#include <QPainter>

namespace Fooyin {
InfoView::InfoView(QWidget* parent)
    : ExpandedTreeView{parent}
    , m_elideText{true}
{
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setTextElideMode(Qt::ElideRight);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setAlternatingRowColors(true);
    setIndentation(5);
    setUniformHeightRole(InfoItem::Role::Type);
}

void InfoView::resizeView()
{
    const int lastColumn = model()->columnCount() - 1;

    if(!m_elideText) {
        const int geometryWidth = geometry().width();

        header()->setSectionResizeMode(lastColumn, QHeaderView::Fixed);
        header()->resizeSection(lastColumn, static_cast<int>(static_cast<double>(geometryWidth) * 1.25));
        header()->setStretchLastSection(false);
    }
    else {
        header()->setSectionResizeMode(lastColumn, QHeaderView::Stretch);
        header()->adjustSize();
        header()->setStretchLastSection(true);
    }
}

void InfoView::setElideText(bool enabled)
{
    if(std::exchange(m_elideText, enabled) != enabled) {
        resizeView();
    }
}

void InfoView::resizeEvent(QResizeEvent* event)
{
    ExpandedTreeView::resizeEvent(event);
    resizeView();
}

void InfoView::paintEvent(QPaintEvent* event)
{
    if(model() && model()->rowCount({}) == 0) {
        QPainter painter{viewport()};
        const QString text = tr("No Selection");
        QRect textRect     = painter.fontMetrics().boundingRect(text);
        textRect.moveCenter(viewport()->rect().center());
        painter.drawText(textRect, Qt::AlignCenter, text);
    }

    ExpandedTreeView::paintEvent(event);
}
} // namespace Fooyin

#include "moc_infoview.cpp"
