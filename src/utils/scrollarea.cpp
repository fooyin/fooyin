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

#include "scrollarea.h"

namespace Fooyin {
ScrollArea::ScrollArea(QWidget* parent)
    : QScrollArea{parent}
{
    setFrameShape(QFrame::NoFrame);
    setFrameShadow(QFrame::Plain);
    setWidgetResizable(true);
    setAutoFillBackground(true);
}

void ScrollArea::resizeEvent(QResizeEvent* event)
{
    QWidget* child = widget();
    if(child) {
        const QSize chilSizeHint = child->minimumSizeHint();
        const int width          = frameWidth() * 2;
        QSize innerSize          = event->size() - QSize{width, width};

        if(chilSizeHint.height() > innerSize.height()) {
            // Child widget is bigger
            innerSize.setWidth(innerSize.width() - scrollBarWidth());
            innerSize.setHeight(chilSizeHint.height());
        }
        // Resize to fit scroll area
        child->resize(innerSize);
    }
    QScrollArea::resizeEvent(event);
}

QSize ScrollArea::minimumSizeHint() const
{
    QWidget* child = widget();
    if(child) {
        const int width = frameWidth() * 2;
        QSize minSize   = child->minimumSizeHint();

        minSize += QSize{width, width};
        minSize += QSize{scrollBarWidth(), 0};

        minSize.setWidth(std::min(minSize.width(), 250));
        minSize.setHeight(std::min(minSize.height(), 250));

        return minSize;
    }
    return {0, 0};
}

bool ScrollArea::event(QEvent* event)
{
    if(event->type() == QEvent::LayoutRequest) {
        updateGeometry();
    }
    return QScrollArea::event(event);
}

int ScrollArea::scrollBarWidth() const
{
    auto* scrollArea = const_cast<ScrollArea*>(this); // NOLINT
    QWidgetList list = scrollArea->scrollBarWidgets(Qt::AlignRight);
    if(list.isEmpty()) {
        return 0;
    }
    return list.first()->sizeHint().width();
}
} // namespace Fooyin

#include "moc_scrollarea.cpp"
