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

#include "radioguideview.h"

#include <QPaintEvent>
#include <QPainter>

namespace Fooyin::RadioBrowser {
void RadioGuideView::setStatusText(const QString& text)
{
    if(std::exchange(m_statusText, text) != text) {
        viewport()->update();
    }
}

void RadioGuideView::clearStatusText()
{
    setStatusText({});
}

void RadioGuideView::paintEvent(QPaintEvent* event)
{
    QTreeView::paintEvent(event);

    if(m_statusText.isEmpty()) {
        return;
    }

    QPainter painter{viewport()};

    QRect textRect = painter.fontMetrics().boundingRect(m_statusText);
    textRect.moveCenter(viewport()->rect().center());
    textRect.adjust(-12, -8, 12, 8);

    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    QColor background = palette().color(QPalette::Base);
    background.setAlpha(220);
    painter.setBrush(background);

    painter.drawRoundedRect(textRect, 4, 4);

    painter.setPen(palette().color(QPalette::Text));
    painter.drawText(textRect, Qt::AlignCenter, m_statusText);
}
} // namespace Fooyin::RadioBrowser

#include "moc_radioguideview.cpp"
