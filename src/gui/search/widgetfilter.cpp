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

#include "widgetfilter.h"

#include <gui/fywidget.h>
#include <utils/widgets/overlaywidget.h>

#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QPushButton>

namespace Fooyin {
WidgetFilter::WidgetFilter(QObject* parent)
    : QObject{parent}
{ }

void WidgetFilter::start()
{
    qApp->installEventFilter(this);
}

void WidgetFilter::stop()
{
    qApp->removeEventFilter(this);
}

bool WidgetFilter::eventFilter(QObject* watched, QEvent* event)
{
    auto handleMouseEvent = [this, event]() {
        const auto* mouseEvent = static_cast<QMouseEvent*>(event);

        if(!mouseEvent) {
            return false;
        }

        if(event->type() == QEvent::MouseButtonPress && mouseEvent->button() != Qt::LeftButton) {
            emit filterFinished();
            return false;
        }

        const QPoint pos = mouseEvent->globalPosition().toPoint();
        auto* widget     = qApp->widgetAt(pos);

        return (widget && (qobject_cast<OverlayWidget*>(widget) || qobject_cast<QPushButton*>(widget)));
    };

    switch(event->type()) {
        case(QEvent::MouseMove):
        case(QEvent::MouseButtonPress): {
            if(handleMouseEvent()) {
                event->ignore();
                return QObject::eventFilter(watched, event);
            }
            event->accept();
            return true;
        }
        case(QEvent::KeyPress): {
            emit filterFinished();
            event->accept();
            return true;
        }
        case(QEvent::Wheel): {
            event->accept();
            return true;
        }
        default:
            event->ignore();
            return QObject::eventFilter(watched, event);
    }
}
} // namespace Fooyin

#include "moc_widgetfilter.cpp"
