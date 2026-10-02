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

#include "splittergraboverlay.h"

#include "fysplitter.h"

#include <QCoreApplication>
#include <QCursor>
#include <QEvent>
#include <QMouseEvent>

constexpr auto HoverGrabArea = 6;

namespace Fooyin {
SplitterGrabOverlay::SplitterGrabOverlay(FySplitterHandle* handle)
    : QWidget{nullptr}
    , m_handle{handle}
{
    setAttribute(Qt::WA_NoSystemBackground);
    setMouseTracking(true);
    hide();
    handle->installEventFilter(this);
}

SplitterGrabOverlay::~SplitterGrabOverlay()
{
    m_handle = nullptr;
}

void SplitterGrabOverlay::showForHandle()
{
    if(isVisible() || !m_handle->isEnabled() || mouseGrabber()) {
        return;
    }

    if(parentWidget() != m_handle->window()) {
        if(parentWidget()) {
            parentWidget()->removeEventFilter(this);
        }
        setParent(m_handle->window());
        parentWidget()->installEventFilter(this);
    }

    QRect area{parentWidget()->mapFromGlobal(m_handle->mapToGlobal(QPoint{})), m_handle->size()};
    if(m_handle->orientation() == Qt::Horizontal) {
        area.adjust(-HoverGrabArea, 0, HoverGrabArea, 0);
    }
    else {
        area.adjust(0, -HoverGrabArea, 0, HoverGrabArea);
    }

    setGeometry(area);
    setCursor(m_handle->cursor());
    raise();
    show();
    m_timer.start(150, this);
}

bool SplitterGrabOverlay::eventFilter(QObject* watched, QEvent* event)
{
    if(watched == parentWidget() && event->type() == QEvent::WindowDeactivate) {
        hide();
    }

    if(watched == m_handle) {
        switch(event->type()) {
            case QEvent::Hide:
            case QEvent::EnabledChange:
                hide();
                break;
            case QEvent::Move:
            case QEvent::Resize:
                if(mouseGrabber() != this) {
                    hide();
                }
                break;
            default:
                break;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void SplitterGrabOverlay::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_timer.timerId()) {
        if(mouseGrabber() != this && !rect().contains(mapFromGlobal(QCursor::pos()))) {
            hide();
        }
    }
    else {
        QWidget::timerEvent(event);
    }
}

bool SplitterGrabOverlay::event(QEvent* event)
{
    switch(event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseMove:
        case QEvent::MouseButtonRelease: {
            const auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if(event->type() == QEvent::MouseButtonPress && mouseEvent->button() == Qt::LeftButton) {
                grabMouse();
            }

            const auto globalPosition = mouseEvent->globalPosition();
            QMouseEvent forwarded{event->type(),
                                  m_handle->mapFromGlobal(globalPosition),
                                  m_handle->window()->mapFromGlobal(globalPosition),
                                  globalPosition,
                                  mouseEvent->button(),
                                  mouseEvent->buttons(),
                                  mouseEvent->modifiers(),
                                  mouseEvent->pointingDevice()};
            QCoreApplication::sendEvent(m_handle, &forwarded);

            if(event->type() == QEvent::MouseButtonRelease && mouseEvent->button() == Qt::LeftButton) {
                releaseMouse();
                hide();
            }

            event->accept();
            return true;
        }
        case QEvent::Leave:
            if(mouseGrabber() != this && !rect().contains(mapFromGlobal(QCursor::pos()))) {
                hide();
            }
            break;
        case QEvent::Hide: {
            m_timer.stop();

            if(mouseGrabber() == this) {
                releaseMouse();
            }

            const auto globalPosition = QCursor::pos();
            const auto position       = m_handle->mapFromGlobal(globalPosition);
            QHoverEvent leaveEvent{QEvent::HoverLeave, position, globalPosition, position};
            QCoreApplication::sendEvent(m_handle, &leaveEvent);
            break;
        }
        default:
            break;
    }

    return QWidget::event(event);
}
} // namespace Fooyin
