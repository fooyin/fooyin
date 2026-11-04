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

#include <gui/scripting/richtextutils.h>

class QPainter;
class QRect;
class QSize;
class QStyleOptionViewItem;

namespace Fooyin::Utils {
struct PreparedTextBlock
{
    QString text;
    QFont font;
    QColor colour;
    int width{0};
};

struct PreparedTextLine
{
    std::vector<PreparedTextBlock> blocks;
    int totalWidth{0};
    TextBaselineMetrics baseline;
    int height{0};
};
using PreparedTextLines = std::vector<PreparedTextLine>;

PreparedTextLines prepareTextLines(const QStyleOptionViewItem& option, int maxWidth, const RichText& richText);
QSize richTextNaturalSize(const QStyleOptionViewItem& option, const RichText& richText);
void drawPreparedTextLines(QPainter* painter, const QRect& rect, const PreparedTextLines& lines,
                           Qt::Alignment alignment);
} // namespace Fooyin::Utils
