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

#include "swatchbutton.h"

#include <QPainter>
#include <QStyle>
#include <QStyleOptionFocusRect>
#include <QStyleOptionToolButton>

namespace Fooyin {
void paintBrushSwatch(QPainter& painter, const BrushSource& brush, const QPalette& palette, QPalette::ColorGroup group,
                      const QRectF& bounds)
{
    painter.save();
    painter.setClipRect(bounds);

    for(int y{0}; y < bounds.height(); y += 8) {
        for(int x{0}; x < bounds.width(); x += 8) {
            painter.fillRect(QRectF{bounds.left() + x, bounds.top() + y, 8, 8},
                             ((x + y) / 8) % 2 ? QColor{160, 160, 160} : QColor{220, 220, 220});
        }
    }

    painter.fillRect(bounds, resolveBrush(brush, palette, group, bounds));
    painter.setPen(palette.color(group, QPalette::Mid));
    painter.drawRect(bounds.adjusted(0.5, 0.5, -0.5, -0.5));
    painter.restore();
}

QIcon brushSourceIcon(const BrushSource& brush, const QPalette& palette, QPalette::ColorGroup group, qreal ratio,
                      const QSize& size)
{
    QPixmap pixmap{size * ratio};
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);

    QPainter painter{&pixmap};
    paintBrushSwatch(painter, brush, palette, group, QRectF{QPointF{}, size});

    QIcon icon;
    for(const auto mode : {QIcon::Normal, QIcon::Disabled, QIcon::Active, QIcon::Selected}) {
        icon.addPixmap(pixmap, mode, QIcon::Off);
        icon.addPixmap(pixmap, mode, QIcon::On);
    }
    return icon;
}

SwatchButton::SwatchButton(QWidget* parent)
    : QAbstractButton{parent}
{
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

QSize SwatchButton::sizeHint() const
{
    return {120, fontMetrics().height() + 8};
}

QSize SwatchButton::minimumSizeHint() const
{
    return {20, fontMetrics().height() + 8};
}

void SwatchButton::paintBackground(QPainter& painter) const
{
    QStyleOptionToolButton option;
    option.initFrom(this);

    option.state |= QStyle::State_AutoRaise;
    if(isDown()) {
        option.state |= QStyle::State_Sunken;
    }
    style()->drawPrimitive(QStyle::PE_PanelButtonTool, &option, &painter, this);
}

void SwatchButton::paintFocus(QPainter& painter) const
{
    if(hasFocus()) {
        QStyleOptionFocusRect focus;
        focus.initFrom(this);

        focus.rect = rect().adjusted(1, 1, -1, -1);
        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, &painter, this);
    }
}

void SwatchButton::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter{this};
    paintBackground(painter);

    const QRect swatch = rect().adjusted(4, 2, -4, -2);
    icon().paint(&painter, swatch);

    if(!isEnabled()) {
        QColor overlay = palette().color(QPalette::Disabled, QPalette::Window);
        overlay.setAlpha(160);
        painter.fillRect(swatch, overlay);
        painter.setPen(palette().color(QPalette::Disabled, QPalette::Mid));
        painter.drawRect(swatch.adjusted(0, 0, -1, -1));
    }

    paintFocus(painter);
}
} // namespace Fooyin
