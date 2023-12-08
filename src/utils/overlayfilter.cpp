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

#include <utils/overlayfilter.h>

#include <QApplication>
#include <QPaintEvent>
#include <QPainter>

namespace Fooyin {
OverlayFilter::OverlayFilter(QWidget* parent)
    : QWidget{parent}
{
    setAttribute(Qt::WA_NoSystemBackground);

    resetColour();
    hide();
}

void OverlayFilter::setColour(const QColor& colour)
{
    m_colour = colour;
}

void OverlayFilter::resetColour()
{
    static QColor colour = QApplication::palette().color(QPalette::Highlight);
    colour.setAlpha(80);
    m_colour = colour;
}

void OverlayFilter::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter{this};

    painter.fillRect(rect(), m_colour);
}
} // namespace Fooyin

#include "utils/moc_overlayfilter.cpp"
