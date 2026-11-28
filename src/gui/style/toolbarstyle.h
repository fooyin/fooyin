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

#include "theme/toolbarappearance.h"

#include <QStyle>

#include <optional>

class QStyleOption;
class QStyleOptionToolBar;
class QStyleOptionToolButton;

namespace Fooyin::Style::Toolbar {
void drawPanel(const QStyleOptionToolBar& option, QPainter& painter, const ToolBarAppearance& appearance);
void drawFrame(const QStyleOption& option, QPainter& painter, const ToolBarAppearance& appearance);
void drawHandle(const QStyleOption& option, QPainter& painter, const ToolBarAppearance& appearance);
void drawSeparator(const QStyleOption& option, QPainter& painter, const ToolBarAppearance& appearance);
void drawExtension(const QStyleOptionToolButton& option, QPainter& painter, const QWidget* widget,
                   const ToolBarAppearance& appearance);
std::optional<int> pixelMetric(QStyle::PixelMetric metric, const ToolBarAppearance& appearance);
} // namespace Fooyin::Style::Toolbar
