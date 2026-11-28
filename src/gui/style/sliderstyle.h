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

#include <QStyle>

#include <optional>

class QStyleOptionSlider;

namespace Fooyin {
struct SliderAppearance;

namespace Style::Slider {
void draw(const QStyle& style, const SliderAppearance& appearance, const QStyleOptionSlider& option, QPainter& painter,
          const QWidget* widget);
std::optional<int> pixelMetric(QStyle::PixelMetric metric, const SliderAppearance& appearance);
QRect subControlRect(const QStyle& style, const SliderAppearance& appearance, const QStyleOptionSlider& option,
                     QStyle::SubControl subControl, const QWidget* widget);
} // namespace Style::Slider
} // namespace Fooyin
