/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <utils/extentabletableview.h>

#include <QMouseEvent>
#include <QPainter>
#include <QTextEdit>

namespace Fy::Utils {
constexpr auto ButtonText = "+ add new";

ExtendableTableView::ExtendableTableView(QWidget* parent)
    : QTableView{parent}
    , m_mouseOverButton{false}
{
    setMouseTracking(true);
}

void ExtendableTableView::mouseMoveEvent(QMouseEvent* event)
{
    const bool isOverButton = m_buttonRect.contains(event->pos());

    if(isOverButton != m_mouseOverButton) {
        m_mouseOverButton = isOverButton;
        viewport()->update();
    }

    QTableView::mouseMoveEvent(event);
}

void ExtendableTableView::mousePressEvent(QMouseEvent* event)
{
    if(m_buttonRect.contains(event->pos())) {
        emit newRowClicked();
    }

    QTableView::mousePressEvent(event);
}

void ExtendableTableView::paintEvent(QPaintEvent* event)
{
    QTableView::paintEvent(event);

    QPainter painter{viewport()};

    const int lastRow    = model()->rowCount() - 1;
    const QRect lastRect = visualRect(model()->index(lastRow, 0));

    const int buttonHeight = 15;
    const int leftMargin   = 50;
    const int topMargin    = 8;

    m_buttonRect = {lastRect.left() + leftMargin, lastRect.bottom() + topMargin, lastRect.width(), buttonHeight};

    if(m_mouseOverButton) {
        painter.fillRect(m_buttonRect, palette().button());
        painter.drawRect(m_buttonRect);
    }
    painter.drawText(m_buttonRect, Qt::AlignCenter, ButtonText);
}
} // namespace Fy::Utils
