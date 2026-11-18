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

#include "sliderstyle.h"

#include "styleutils.h"
#include "theme/sliderappearance.h"

#include <QPainter>
#include <QSlider>
#include <QStyleOptionSlider>

namespace Fooyin::Style::Slider {
void draw(const QStyle& style, const SliderAppearance& appearance, const QStyleOptionSlider& option, QPainter& painter,
          const QWidget* widget)
{
    const auto group             = colourGroup(option);
    const QRectF handle          = style.subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, widget);
    const bool enabled           = option.state & QStyle::State_Enabled;
    const auto& grooveAppearance = stateAppearance(appearance.groove, enabled, option.state & QStyle::State_MouseOver,
                                                   option.state & QStyle::State_Sunken);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    const bool horizontal = option.orientation == Qt::Horizontal;
    QRectF groove         = style.subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderGroove, widget);
    if(option.subControls & QStyle::SC_SliderGroove) {
        if(horizontal) {
            groove.adjust(handle.width() / 2.0, 0.0, -handle.width() / 2.0, 0.0);
            const double inset
                = (groove.height() - std::min<double>(grooveAppearance.thickness, handle.height())) / 2.0;
            groove.adjust(0.0, inset, 0.0, -inset);
        }
        else {
            groove.adjust(0.0, handle.height() / 2.0, 0.0, -handle.height() / 2.0);
            const double inset = (groove.width() - std::min<double>(grooveAppearance.thickness, handle.width())) / 2.0;
            groove.adjust(inset, 0.0, -inset, 0.0);
        }

        if(!groove.isEmpty()) {
            painter.setBrush(resolveBrush(grooveAppearance.backgroundColour, option.palette, group, groove,
                                          option.orientation, option.upsideDown));
            painter.drawRoundedRect(groove, grooveAppearance.radius, grooveAppearance.radius);

            QRectF filled{groove};
            if(horizontal) {
                if(option.upsideDown) {
                    filled.setLeft(handle.center().x());
                }
                else {
                    filled.setRight(handle.center().x());
                }
            }
            else if(option.upsideDown) {
                filled.setTop(handle.center().y());
            }
            else {
                filled.setBottom(handle.center().y());
            }

            if(!filled.isEmpty()) {
                painter.setBrush(resolveBrush(grooveAppearance.filledColour, option.palette, group, groove,
                                              option.orientation, option.upsideDown));
                painter.drawRoundedRect(filled, grooveAppearance.radius, grooveAppearance.radius);
            }
        }
    }

    const bool active  = option.activeSubControls & QStyle::SC_SliderHandle;
    const bool hovered = enabled && active && (option.state & QStyle::State_MouseOver);
    const bool pressed = enabled && active && (option.state & QStyle::State_Sunken);
    const bool focused = enabled && (option.state & QStyle::State_HasFocus)
                      && (option.state & QStyle::State_KeyboardFocusChange) && appearance.focus.borderWidth > 0.0;

    if(option.subControls & QStyle::SC_SliderHandle) {
        const auto& handleAppearance = stateAppearance(appearance, enabled, hovered, pressed);

        const auto drawHandle = [&](const QRectF& rect) {
            if(handleAppearance.cornerRadius == 100) {
                painter.drawEllipse(rect);
            }
            else {
                const double radius = std::min(rect.width(), rect.height()) * handleAppearance.cornerRadius / 200.0;
                painter.drawRoundedRect(rect, radius, radius);
            }
        };

        const double inset = handleAppearance.inset;
        const double borderWidth
            = focused ? std::min(appearance.focus.borderWidth, inset * 2.0) : handleAppearance.borderWidth;
        const auto& borderColour = focused ? appearance.focus.borderColour : handleAppearance.borderColour;
        if(borderWidth > 0.0) {
            painter.setPen(QPen{resolveBrush(borderColour, option.palette, group), borderWidth});
        }
        else {
            painter.setPen(Qt::NoPen);
        }
        painter.setBrush(resolveBrush(handleAppearance.backgroundColour, option.palette, group,
                                      handle.adjusted(inset, inset, -inset, -inset), option.orientation,
                                      option.upsideDown));
        drawHandle(handle.adjusted(inset, inset, -inset, -inset));
    }

    painter.restore();
}

std::optional<int> pixelMetric(QStyle::PixelMetric metric, const SliderAppearance& appearance)
{
    switch(metric) {
        case QStyle::PM_SliderLength:
        case QStyle::PM_SliderThickness:
            return appearance.handleExtent;
        case QStyle::PM_SliderTickmarkOffset:
            return appearance.tickmarkOffset;
        default:
            return {};
    }
}

QRect subControlRect(const QStyle& style, const SliderAppearance& /*appearance*/, const QStyleOptionSlider& option,
                     QStyle::SubControl subControl, const QWidget* widget)
{
    const QRect rect{option.rect};

    const bool horizontal = option.orientation == Qt::Horizontal;
    const int length      = horizontal ? rect.width() : rect.height();
    const int thickness   = horizontal ? rect.height() : rect.width();
    const int extent
        = std::max(0, std::min({style.pixelMetric(QStyle::PM_SliderLength, &option, widget), length, thickness}));
    const int tickOffset = style.pixelMetric(QStyle::PM_SliderTickmarkOffset, &option, widget);

    int offset = (thickness - extent) / 2;
    if(option.tickPosition & QSlider::TicksAbove) {
        offset += tickOffset;
    }
    if(option.tickPosition & QSlider::TicksBelow) {
        offset -= tickOffset;
    }
    offset = std::clamp(offset, 0, std::max(0, thickness - extent));

    if(subControl == QStyle::SC_SliderHandle) {
        const int position = QStyle::sliderPositionFromValue(option.minimum, option.maximum, option.sliderPosition,
                                                             std::max(0, length - extent), option.upsideDown);
        return horizontal ? QRect{rect.x() + position, rect.y() + offset, extent, extent}
                          : QRect{rect.x() + offset, rect.y() + position, extent, extent};
    }

    return horizontal ? QRect{rect.x(), rect.y() + offset, length, extent}
                      : QRect{rect.x() + offset, rect.y(), extent, length};
}
} // namespace Fooyin::Style::Slider
