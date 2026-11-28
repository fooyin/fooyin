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

class QStyleOptionHeader;

namespace Fooyin {
struct HeaderAppearance;

namespace Style::Header {
void drawSection(const QStyleOptionHeader& option, QPainter& painter, const QWidget* widget,
                 const HeaderAppearance& appearance);
void drawEmptyArea(const QStyleOption& option, QPainter& painter, const QWidget* widget,
                   const HeaderAppearance& appearance);
void drawLabel(const QCommonStyle& style, const QStyleOptionHeader& option, QPainter& painter, const QWidget* widget,
               const HeaderAppearance& appearance);
void drawSortIndicator(const QStyleOptionHeader& option, QPainter& painter, const HeaderAppearance& appearance);

std::optional<int> pixelMetric(QStyle::PixelMetric metric, const QStyleOption* option,
                               const HeaderAppearance& appearance);
QSize sizeFromContents(const QStyleOptionHeader& option, const QSize& contentsSize, const HeaderAppearance& appearance);
QRect subElementRect(const QStyleOptionHeader& option, QStyle::SubElement element, const HeaderAppearance& appearance);

void unpolish(QWidget* widget);
} // namespace Style::Header
} // namespace Fooyin
