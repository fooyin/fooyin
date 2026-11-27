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

#pragma once

#include "styleanimation.h"

#include <gui/coloursource.h>

#include <QIcon>
#include <QStyleOption>

class QBrush;
class QPainterPath;

namespace Fooyin::Style {
template <typename Appearance>
constexpr const auto& stateAppearance(const Appearance& appearance, bool enabled, bool hovered, bool pressed)
{
    if(!enabled) {
        return appearance.disabled;
    }
    if(pressed) {
        return appearance.pressed;
    }
    if(hovered) {
        return appearance.hovered;
    }
    return appearance.normal;
}

QBrush animatedBrush(const QStyleOption& option, const QWidget* widget, TransitionChannel channel,
                     const QBrush& target);
QBrush opacityBrush(const QBrush& brush, double opacity);
QPalette::ColorGroup colourGroup(const QStyleOption& option);
QColor panelColour(const QStyleOption& option);
QColor outlineColour(const QStyleOption& option);
QColor tintedColour(const QStyleOption& option, const ColourSource& source, const ColourSource& tintSource,
                    int tintOpacity, int opacity);
QIcon tintedIcon(const QIcon& icon, const QSize& size, double devicePixelRatio, QIcon::Mode mode, QIcon::State state,
                 const QColor& colour);
void drawChevron(const QStyleOption& option, const QRect& rect, QPainter& painter, bool up, QPalette::ColorRole role);
void drawChevron(const QStyleOption& option, const QRect& rect, QPainter& painter, bool up, const QBrush& brush);
void drawArrow(const QStyleOption& option, QPainter& painter, Qt::ArrowType arrow);
void drawRelief(const QStyleOption& option, QPainter& painter, const QPainterPath& path, double width, double opacity);
} // namespace Fooyin::Style
