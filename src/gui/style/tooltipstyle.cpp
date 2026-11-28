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

#include "tooltipstyle.h"

#include "theme/tooltipappearance.h"

#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
#include <QVariant>
#include <QWidget>

#include <cmath>

constexpr auto BackgroundAttributeProperty = "_fooyin_tooltip_background";
constexpr auto ForegroundRoleProperty      = "_fooyin_tooltip_foreground_role";
constexpr auto ForegroundPaletteProperty   = "_fooyin_tooltip_foreground_palette";

namespace Fooyin::Style::ToolTip {
void drawPanel(const QStyleOption& option, QPainter& painter, const ToolTipAppearance& appearance,
               const QWidget* widget)
{
    const double inset = appearance.borderWidth / 2.0;
    const QRectF rect  = QRectF{option.rect}.adjusted(inset, inset, -inset, -inset);
    if(rect.isEmpty()) {
        return;
    }

    // QLabel needs a remapped text palette; resolve panel roles against the original palette
    const auto saved   = widget ? widget->property(ForegroundPaletteProperty) : QVariant{};
    const auto palette = saved.isValid() ? saved.value<QPalette>() : option.palette;

    QPainterPath path;
    path.addRoundedRect(rect, appearance.radius, appearance.radius);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);

    const double opacity = painter.opacity();
    painter.setOpacity(opacity * appearance.backgroundOpacity / 100.0);
    painter.fillPath(path, resolveBrush(appearance.backgroundColour, palette, QPalette::Inactive));
    painter.setOpacity(opacity);

    if(appearance.borderWidth > 0 && appearance.borderOpacity > 0) {
        painter.setOpacity(opacity * appearance.borderOpacity / 100.0);
        painter.strokePath(
            path, QPen{resolveBrush(appearance.borderColour, palette, QPalette::Inactive), appearance.borderWidth});
    }

    painter.restore();
}

int frameWidth(const ToolTipAppearance& appearance)
{
    // QTipLabel adds one pixel to this metric for its uniform text margin
    return std::max(appearance.padding, static_cast<int>(std::ceil(appearance.borderWidth))) - 1;
}

QPalette textPalette(const QPalette& palette, const ToolTipAppearance& appearance)
{
    auto resolved{palette};
    resolved.setBrush(QPalette::All, QPalette::ToolTipText,
                      resolveBrush(appearance.foregroundColour, palette, QPalette::Inactive));
    return resolved;
}

void polish(QWidget* widget, const ToolTipAppearance& appearance)
{
    if(!widget->inherits("QTipLabel")) {
        return;
    }

    if(!widget->testAttribute(Qt::WA_TranslucentBackground)) {
        // Alpha must be enabled before Qt creates the tooltip's native window
        widget->setProperty(BackgroundAttributeProperty, widget->testAttribute(Qt::WA_NoSystemBackground));
        widget->setAttribute(Qt::WA_TranslucentBackground, true);
    }

    if(!widget->property(ForegroundPaletteProperty).isValid()) {
        widget->setProperty(ForegroundPaletteProperty, widget->palette());
    }
    widget->setPalette(textPalette(widget->property(ForegroundPaletteProperty).value<QPalette>(), appearance));

    if(widget->foregroundRole() != QPalette::ToolTipText) {
        widget->setProperty(ForegroundRoleProperty, static_cast<int>(widget->foregroundRole()));
        // QLabel uses its foreground role for both plain and rich text
        widget->setForegroundRole(QPalette::ToolTipText);
    }
}

void unpolish(QWidget* widget)
{
    if(const auto palette = widget->property(ForegroundPaletteProperty); palette.isValid()) {
        widget->setPalette(palette.value<QPalette>());
        widget->setProperty(ForegroundPaletteProperty, {});
    }
    if(const auto foreground = widget->property(ForegroundRoleProperty); foreground.isValid()) {
        widget->setForegroundRole(static_cast<QPalette::ColorRole>(foreground.toInt()));
        widget->setProperty(ForegroundRoleProperty, {});
    }
    if(const auto background = widget->property(BackgroundAttributeProperty); background.isValid()) {
        widget->setAttribute(Qt::WA_TranslucentBackground, false);
        widget->setAttribute(Qt::WA_NoSystemBackground, background.toBool());
        widget->setProperty(BackgroundAttributeProperty, {});
    }
}
} // namespace Fooyin::Style::ToolTip
