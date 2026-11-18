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

class QStyleOption;
class QStyleOptionMenuItem;

namespace Fooyin {
struct MenuAppearance;

namespace Style::Menu {
void drawPanel(const QStyleOption& option, QPainter& painter, const QWidget* widget, const MenuAppearance& appearance);
void drawMenuBarBackground(const QStyleOption& option, QPainter& painter, const QWidget* widget,
                           const MenuAppearance& appearance);
void drawItem(const QStyle& style, const QStyleOptionMenuItem& option, QPainter& painter, const QWidget* widget,
              const MenuAppearance& appearance);
void drawMenuBarItem(const QStyle& style, const QStyleOptionMenuItem& option, QPainter& painter, const QWidget* widget,
                     const MenuAppearance& appearance);
void drawAuxiliary(QStyle::ControlElement element, const QStyleOption& option, QPainter& painter, const QWidget* widget,
                   const MenuAppearance& appearance);

std::optional<int> pixelMetric(QStyle::PixelMetric metric, const MenuAppearance& appearance);
std::optional<int> styleHint(QStyle::StyleHint hint);
QSize itemSize(const QStyle& style, const QStyleOptionMenuItem& option, const QSize& contentsSize,
               const QWidget* widget, const MenuAppearance& appearance);
QSize menuBarItemSize(const QStyleOptionMenuItem& option, const QSize& contentsSize, const MenuAppearance& appearance);

void polish(QWidget* widget);
void unpolish(QWidget* widget);
} // namespace Style::Menu
} // namespace Fooyin