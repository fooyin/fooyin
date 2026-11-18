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

class QStyleOption;
class QPalette;

namespace Fooyin {
struct ToolTipAppearance;

namespace Style::ToolTip {
void drawPanel(const QStyleOption& option, QPainter& painter, const ToolTipAppearance& appearance,
               const QWidget* widget);
int frameWidth(const ToolTipAppearance& appearance);
QPalette textPalette(const QPalette& palette, const ToolTipAppearance& appearance);
void polish(QWidget* widget, const ToolTipAppearance& appearance);
void unpolish(QWidget* widget);
} // namespace Style::ToolTip
} // namespace Fooyin
