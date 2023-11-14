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

#include <utils/hovermenu.h>

#include <chrono>

namespace {
void closeMenu(QTimer& timer, QWidget* self)
{
    if(self->underMouse() || self->parentWidget()->underMouse()) {
        // Close as soon as mouse leaves
        timer.start();
        return;
    }
    timer.stop();
    self->hide();
}
} // namespace

namespace Fy::Utils {
HoverMenu::HoverMenu(QWidget* parent)
    : QWidget{parent}
{
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);

    QObject::connect(&m_timer, &QTimer::timeout, this, [this]() { closeMenu(m_timer, this); });
}

void HoverMenu::start(std::chrono::milliseconds ms)
{
    m_timer.setInterval(ms);
    m_timer.start(ms);
}

void HoverMenu::leaveEvent(QEvent* event)
{
    hide();
    QWidget::leaveEvent(event);
}
} // namespace Fy::Utils

#include "utils/moc_hovermenu.cpp"
