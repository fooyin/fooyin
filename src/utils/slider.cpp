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

#include <utils/slider.h>

#include <QMouseEvent>
#include <QProxyStyle>
#include <QStyle>

namespace Fooyin {
Slider::Slider(Qt::Orientation type, QWidget* parent)
    : QSlider{type, parent}
{
    setMouseTracking(true);
}

void Slider::mousePressEvent(QMouseEvent* e)
{
    Qt::MouseButton button = e->button();
    if(button == Qt::LeftButton) {
        const int absolute = style()->styleHint(QStyle::SH_Slider_AbsoluteSetButtons);
        if(Qt::LeftButton & absolute) {
            button = Qt::LeftButton;
        }
        else if(Qt::MiddleButton & absolute) {
            button = Qt::MiddleButton;
        }
        else if(Qt::RightButton & absolute) {
            button = Qt::RightButton;
        }
    }

    QMouseEvent event(e->type(), e->position(), e->globalPosition(), button, button, e->modifiers());
    QSlider::mousePressEvent(&event);
}
} // namespace Fooyin

#include "utils/moc_slider.cpp"
