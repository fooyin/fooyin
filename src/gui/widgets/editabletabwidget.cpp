/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <gui/widgets/editabletabwidget.h>

#include <gui/widgets/editabletabbar.h>

#include <QMouseEvent>

namespace Fooyin {
EditableTabWidget::EditableTabWidget(QWidget* parent)
    : QTabWidget{parent}
    , m_tabBar{new EditableTabBar(this)}
{
    setTabBar(m_tabBar);

    QObject::connect(m_tabBar, &EditableTabBar::tabBarClicked, this, &EditableTabWidget::tabBarClicked);
    QObject::connect(m_tabBar, &EditableTabBar::middleClicked, this, &EditableTabWidget::middleClicked);
}

EditableTabBar* EditableTabWidget::editableTabBar() const
{
    return m_tabBar;
}

void EditableTabWidget::mousePressEvent(QMouseEvent* event)
{
    const QPoint pos = event->position().toPoint();
    const int index  = m_tabBar->tabAt(pos);

    if(event->button() & Qt::MiddleButton) {
        emit middleClicked(index);
    }
    else {
        emit tabBarClicked(index);
    }

    QTabWidget::mousePressEvent(event);
}
} // namespace Fooyin

#include "gui/widgets/moc_editabletabwidget.cpp"
