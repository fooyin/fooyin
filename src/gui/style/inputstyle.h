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

class QStyleOptionComboBox;
class QStyleOptionFrame;
class QStyleOptionSpinBox;

namespace Fooyin {
struct InputAppearance;

namespace Style::Input {
QSize lineEditSize(const QStyleOptionFrame& option, const QSize& contentsSize, const InputAppearance& appearance,
                   const QWidget* widget);
QRect lineEditContentsRect(const QStyleOptionFrame& option, const InputAppearance& appearance, const QWidget* widget);
void drawLineEdit(const QStyleOptionFrame& option, QPainter& painter, bool fill, const InputAppearance& appearance,
                  const QWidget* widget);

QSize comboBoxSize(const QStyleOptionComboBox& option, const QSize& contentsSize, const InputAppearance& appearance);
QRect comboBoxSubControlRect(const QStyleOptionComboBox& option, QStyle::SubControl subControl,
                             const InputAppearance& appearance);
void drawComboBox(const QStyle& style, const QStyleOptionComboBox& option, QPainter& painter, const QWidget* widget,
                  const InputAppearance& appearance);
void drawComboBoxLabel(const QStyle& style, const QStyleOptionComboBox& option, QPainter& painter,
                       const QWidget* widget);

QSize spinBoxSize(const QStyleOptionSpinBox& option, const QSize& contentsSize, const InputAppearance& appearance);
QRect spinBoxSubControlRect(const QStyleOptionSpinBox& option, QStyle::SubControl subControl,
                            const InputAppearance& appearance);
void drawSpinBox(const QStyle& style, const QStyleOptionSpinBox& option, QPainter& painter, const QWidget* widget,
                 const InputAppearance& appearance);

std::optional<int> pixelMetric(QStyle::PixelMetric metric, const QWidget* widget, const InputAppearance& lineEdit,
                               const InputAppearance& comboBox, const InputAppearance& spinBox);
} // namespace Style::Input
} // namespace Fooyin