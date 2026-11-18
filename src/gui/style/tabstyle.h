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

#include <QCommonStyle>

#include <optional>

class QStyleOptionTab;
class QStyleOptionTabWidgetFrame;
class QTabBar;

namespace Fooyin {
struct TabAppearance;

namespace Style::Tab {
void drawStripBackground(QWidget* widget, const TabAppearance& appearance);
void drawCloseIndicator(const QStyleOption& option, QPainter& painter, const TabAppearance& appearance);
void drawShape(const QStyleOptionTab& option, QPainter& painter, const QWidget* widget,
               const TabAppearance& appearance);
void drawPane(const QStyleOptionTabWidgetFrame& option, QPainter& painter, const TabAppearance& appearance,
              const QWidget* widget);
void drawLabel(const QCommonStyle& style, const QStyleOptionTab& option, QPainter& painter, const QWidget* widget,
               const TabAppearance& appearance);

QSize sizeFromContents(const QStyleOptionTab& option, const QSize& contentsSize, const TabAppearance& appearance);
QRect paddedTabRect(const QStyleOptionTab& option, const TabAppearance& appearance);
QSize paneSize(const QStyleOptionTabWidgetFrame& option, const QSize& contentsSize, const TabAppearance& appearance);
QRect paneContentsRect(const QCommonStyle& style, const QStyleOptionTabWidgetFrame& option, const QWidget* widget,
                       const TabAppearance& appearance);
std::optional<int> pixelMetric(QStyle::PixelMetric metric, const TabAppearance& appearance);
std::optional<int> styleHint(QStyle::StyleHint hint, const TabAppearance& appearance);

void polish(QWidget* widget);
void unpolish(QWidget* widget);
void updatePane(QTabBar* bar);
} // namespace Style::Tab
} // namespace Fooyin
