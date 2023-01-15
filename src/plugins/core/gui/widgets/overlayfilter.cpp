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

#include "overlayfilter.h"

#include <QPaintEvent>
#include <QPainter>

namespace Core::Widgets {
OverlayFilter::OverlayFilter(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    hide();
}

OverlayFilter::~OverlayFilter() = default;

void OverlayFilter::paintEvent(QPaintEvent* e)
{
    Q_UNUSED(e)
    QPainter painter(this);

    QColor colour = palette().color(QPalette::Highlight);
    colour.setAlpha(30);

    painter.fillRect(rect(), colour);
}
} // namespace Core::Widgets
