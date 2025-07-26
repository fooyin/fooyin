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

#include <gui/windowcontroller.h>

#include <QEvent>
#include <QMainWindow>

namespace Fooyin {
WindowController::WindowController(QMainWindow* window)
    : QObject{window}
    , m_mainWindow{window}
{
    m_mainWindow->installEventFilter(this);
}

bool WindowController::isFullScreen() const
{
    return m_mainWindow->isFullScreen();
}

void WindowController::setFullScreen(bool fullscreen)
{
    if(fullscreen) {
        m_mainWindow->setWindowState(m_mainWindow->windowState() | Qt::WindowMaximized);
    }
    else {
        m_mainWindow->setWindowState(m_mainWindow->windowState() &= ~Qt::WindowMaximized);
    }
}

void WindowController::raise()
{
    m_mainWindow->raise();
}

bool WindowController::eventFilter(QObject* watched, QEvent* event)
{
    if(event->type() == QEvent::WindowStateChange) {
        emit isFullScreenChanged(m_mainWindow->windowState() & Qt::WindowMaximized);
    }

    if(event->type() == QEvent::ShowToParent) {
        emit windowShown();
    }
    else if(event->type() == QEvent::Hide) {
        emit windowHidden();
    }

    return QObject::eventFilter(watched, event);
}
} // namespace Fooyin

#include "gui/moc_windowcontroller.cpp"
