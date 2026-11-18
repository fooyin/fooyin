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

class QPainter;
class QStyleOption;
class QStyleOptionViewItem;
class QWidget;

namespace Fooyin {
struct ItemViewAppearance;

namespace Style::ItemView {
void drawBranch(const QStyleOption& option, QPainter& painter, const QWidget* widget,
                const ItemViewAppearance& appearance);
void drawPanel(const QStyleOptionViewItem& option, QPainter& painter, const QWidget* widget, bool row,
               const ItemViewAppearance& appearance);
void drawFocus(const QStyleOption& option, QPainter& painter, const ItemViewAppearance& appearance);
void polish(QWidget* widget);
void unpolish(QWidget* widget);
} // namespace Style::ItemView
} // namespace Fooyin