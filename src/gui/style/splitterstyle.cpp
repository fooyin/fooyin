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

#include "splitterstyle.h"

#include "styleutils.h"
#include "theme/splitterappearance.h"

#include <QPainter>
#include <QStyleOption>
#include <QWidget>

namespace Fooyin::Style::Splitter {
QRectF gripRect(const QStyleOption& option, const SplitterAppearance& appearance)
{
    const bool horizontal = option.state & QStyle::State_Horizontal;
    const QRectF rect{option.rect};
    const double length    = std::min(appearance.grip.length,
                                      (horizontal ? rect.height() : rect.width()) - (2.0 * appearance.grip.margin));
    const double thickness = std::min(appearance.grip.thickness, horizontal ? rect.width() : rect.height());
    if(length <= 0.0 || thickness <= 0.0) {
        return {};
    }

    const auto centre = rect.center();
    return horizontal ? QRectF{centre.x() - (thickness / 2.0), centre.y() - (length / 2.0), thickness, length}
                      : QRectF{centre.x() - (length / 2.0), centre.y() - (thickness / 2.0), length, thickness};
}

void draw(const QStyleOption& option, QPainter& painter, const QWidget* widget, const SplitterAppearance& appearance)
{
    if(option.rect.isEmpty()) {
        return;
    }

    auto group = colourGroup(option);

    const bool enabled = option.state & QStyle::State_Enabled;
    if(enabled && widget) {
        group = widget->isActiveWindow() ? QPalette::Active : QPalette::Inactive;
    }

    const QRectF rect{option.rect};
    const QRectF grip = gripRect(option, appearance);

    const bool pressed = enabled && (option.state & QStyle::State_Sunken);
    const bool hovered = enabled && (option.state & QStyle::State_MouseOver);
    const auto& state  = stateAppearance(appearance, enabled, hovered, pressed);

    painter.save();
    painter.setClipRect(option.rect, Qt::IntersectClip);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    const double opacity = painter.opacity();

    if(state.backgroundOpacity > 0) {
        painter.setOpacity(opacity * state.backgroundOpacity / 100.0);
        painter.setBrush(resolveBrush(state.backgroundColour, option.palette, group));
        painter.drawRoundedRect(rect, appearance.backgroundRadius, appearance.backgroundRadius);
    }
    if(grip.isEmpty()) {
        painter.restore();
        return;
    }

    painter.setOpacity(opacity * state.gripOpacity / 100.0);
    painter.setBrush(resolveBrush(state.gripColour, option.palette, group));
    const double radius = std::min(grip.width(), grip.height()) * appearance.grip.cornerRadius / 200.0;
    painter.drawRoundedRect(grip, radius, radius);
    painter.restore();
}
} // namespace Fooyin::Style::Splitter
