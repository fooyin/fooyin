/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "hovermenu.h"

HoverMenu::HoverMenu(QWidget* parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::CustomizeWindowHint | Qt::ToolTip);

    connect(&m_timer, &QTimer::timeout, this, &HoverMenu::closeMenu);
}

HoverMenu::~HoverMenu() = default;

void HoverMenu::leaveEvent(QEvent* event)
{
    accept();
    QWidget::leaveEvent(event);
}

void HoverMenu::showEvent(QShowEvent* event)
{
    // Close after 1 second
    m_timer.start(1000);
    QDialog::showEvent(event);
}

void HoverMenu::closeMenu()
{
    if(this->underMouse() || parentWidget()->underMouse()) {
        // Close as soon as mouse leaves
        return m_timer.start();
    }
    m_timer.stop();
    accept();
}
