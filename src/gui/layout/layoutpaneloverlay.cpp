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

#include "layoutpaneloverlay.h"

#include <gui/guiconstants.h>
#include <gui/iconloader.h>

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>

namespace Fooyin {
LayoutPanelOverlay::LayoutPanelOverlay(QWidget* parent, LayoutPanelOverlay* outline)
    : QWidget{parent}
    , m_outline{outline}
    , m_pressed{false}
    , m_hovered{false}
{
    setCursor(Qt::OpenHandCursor);
    setAttribute(Qt::WA_NoSystemBackground);
}

void LayoutPanelOverlay::setHovered(bool hovered)
{
    m_hovered = hovered;
    update();
}

void LayoutPanelOverlay::mousePressEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton) {
        m_pressPosition = event->position().toPoint();
        m_pressed       = true;
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void LayoutPanelOverlay::mouseMoveEvent(QMouseEvent* event)
{
    if(m_pressed && event->buttons().testFlag(Qt::LeftButton)
       && (event->position().toPoint() - m_pressPosition).manhattanLength() >= QApplication::startDragDistance()) {
        m_pressed = false;
        Q_EMIT dragRequested();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void LayoutPanelOverlay::mouseReleaseEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton) {
        const bool clicked = std::exchange(m_pressed, false);
        event->accept();
        if(clicked) {
            Q_EMIT selectionRequested();
        }
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void LayoutPanelOverlay::enterEvent(QEnterEvent* event)
{
    if(m_outline) {
        m_outline->setHovered(true);
    }
    else {
        setHovered(true);
    }
    QWidget::enterEvent(event);
}

void LayoutPanelOverlay::leaveEvent(QEvent* event)
{
    if(m_outline) {
        m_outline->setHovered(false);
    }
    else {
        setHovered(false);
    }
    QWidget::leaveEvent(event);
}

void LayoutPanelOverlay::paintEvent(QPaintEvent* /*event*/)
{
    if(m_hovered) {
        QPainter painter{this};
        QPen pen{palette().color(QPalette::Highlight)};
        pen.setCosmetic(true);
        painter.setPen(pen);

        const auto transform   = painter.deviceTransform();
        const auto bounds      = QRectF{rect()};
        const auto topLeft     = transform.map(bounds.topLeft()).toPoint();
        const auto bottomRight = transform.map(bounds.bottomRight()).toPoint();
        const auto frame       = QRectF{topLeft, bottomRight}.adjusted(0.5, 0.5, -0.5, -0.5);
        painter.drawRect(transform.inverted().mapRect(frame));
    }
}

LayoutStackGrip::LayoutStackGrip(QWidget* parent, LayoutPanelOverlay* outline)
    : LayoutPanelOverlay{parent, outline}
    , m_orientation{Qt::Vertical}
{
    setFixedSize(LayoutStackGrip::sizeHint());
}

QSize LayoutStackGrip::sizeHint() const
{
    return {32, 24};
}

void LayoutStackGrip::setBackgroundColour(const QColor& colour)
{
    m_bgColour = colour;
    update();
}

void LayoutStackGrip::setOrientation(Qt::Orientation orientation)
{
    if(std::exchange(m_orientation, orientation) != orientation) {
        update();
    }
}

void LayoutStackGrip::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter{this};

    if(m_bgColour.isValid()) {
        painter.fillRect(rect(), m_bgColour);
        painter.fillRect(rect(), m_bgColour);

        auto background = palette().color(QPalette::Window);
        background.setAlpha(255);

        painter.fillRect(rect().adjusted(5, 5, -5, -5), background);
    }

    const int size = std::max(32, std::min(width(), height()));
    Gui::iconFromTheme(m_orientation == Qt::Horizontal ? Constants::Icons::GripHorizontal
                                                       : Constants::Icons::GripVertical)
        .paint(&painter, QRect{(width() - size) / 2, (height() - size) / 2, size, size});
}
} // namespace Fooyin
