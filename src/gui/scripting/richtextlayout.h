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

#include <QIcon>

#include <optional>

class QPainter;
class QStyleOptionViewItem;

namespace Fooyin {
struct RichTextLine
{
    RichText left;
    RichText right;
    TextBaselineMetrics baseline;
    TextBaselineMetrics leftBaseline;
    TextBaselineMetrics rightBaseline;
    std::optional<RichFormatting> rule;
    int leftWidth{0};
    int rightWidth{0};
    int height{0};
};

struct RichTextLayout
{
    std::vector<RichTextLine> lines;
    int width{0};
    int height{0};
};

RichTextLayout prepareRichTextLayout(const RichText& text, const QFont& baseFont = {}, bool measureWidth = true);
QString richTextLayoutPlainText(const RichTextLayout& layout);
void drawRichTextLayout(QPainter* painter, const QStyleOptionViewItem& option, const QRect& rect,
                        const RichTextLayout& layout, const QIcon& playingIcon = {});
} // namespace Fooyin

Q_DECLARE_METATYPE(Fooyin::RichTextLayout)
