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

#include "theme/pushbuttonappearance.h"
#include "theme/toolbuttonappearance.h"

#include <QCommonStyle>

#include <optional>

class QStyleOptionButton;
class QStyleOptionToolButton;

namespace Fooyin::Style::Button {
void drawPushButton(const QStyle& style, const QStyleOptionButton& option, QPainter& painter, const QWidget* widget,
                    const PushButtonAppearance& appearance);
void drawPushButtonPanel(const QStyle& style, const QStyleOptionButton& option, QPainter& painter,
                         const QWidget* widget, const PushButtonAppearance& appearance);
void drawPushButtonLabel(const QCommonStyle& style, const QStyleOptionButton& option, QPainter& painter,
                         const QWidget* widget, const PushButtonAppearance& appearance);
void drawToolButton(const QStyle& style, const QStyleOptionToolButton& option, QPainter& painter, const QWidget* widget,
                    const ToolButtonAppearance& appearance);
void drawToolButtonLabel(const QCommonStyle& style, const QStyleOptionToolButton& option, QPainter& painter,
                         const QWidget* widget, const ToolButtonAppearance& appearance);
std::optional<int> pixelMetric(QStyle::PixelMetric metric, const QStyleOption* option,
                               const ToolButtonAppearance& appearance);
QSize pushButtonSize(const QStyleOptionButton& option, const QSize& contentsSize,
                     const PushButtonAppearance& appearance);
QSize toolButtonSize(const QSize& contentsSize, const ToolButtonAppearance& appearance);
QRect pushButtonContentsRect(const QStyleOptionButton& option, const PushButtonAppearance& appearance);
QRect toolButtonSubControlRect(const QStyle& style, const QStyleOptionToolButton& option, QStyle::SubControl subControl,
                               const QWidget* widget);
} // namespace Fooyin::Style::Button
