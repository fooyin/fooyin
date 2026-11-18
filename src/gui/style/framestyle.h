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

class QStyleOptionFrame;
class QStyleOptionGroupBox;
class QStyleOptionRubberBand;

namespace Fooyin {
struct GroupBoxAppearance;

namespace Style::Frame {
void draw(const QStyleOptionFrame& option, QPainter& painter);
void drawFocus(const QStyleOption& option, QPainter& painter);
void drawRubberBand(const QStyleOptionRubberBand& option, QPainter& painter);
void drawSeparator(const QStyleOptionFrame& option, QPainter& painter);
void drawGroupFrame(const QStyleOptionFrame& option, QPainter& painter, const GroupBoxAppearance& appearance);
void drawGroupBox(const QStyle& style, const QStyleOptionGroupBox& option, QPainter& painter, const QWidget* widget,
                  const GroupBoxAppearance& appearance);
QRect groupBoxSubControlRect(const QStyle& style, const QStyleOptionGroupBox& option, QStyle::SubControl subControl,
                             const QWidget* widget, const GroupBoxAppearance& appearance);
QSize groupBoxSize(const QStyle& style, const QStyleOptionGroupBox& option, const QSize& contentsSize,
                   const QWidget* widget, const GroupBoxAppearance& appearance);
} // namespace Style::Frame
} // namespace Fooyin