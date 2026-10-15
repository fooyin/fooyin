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

#include "playlistheaderlayout.h"

#include <QFontMetrics>
#include <QPainter>
#include <QStyleOptionViewItem>

constexpr auto TextGap    = 20;
constexpr auto RuleHeight = 4;

namespace Fooyin {
namespace {
struct ElidedHeaderText
{
    RichText text;
    int width{0};
};

ElidedHeaderText elideText(const RichText& text, const QFont& baseFont, int availableWidth)
{
    ElidedHeaderText result;

    for(auto block : text.blocks) {
        const int remaining = availableWidth - result.width;
        if(remaining <= 0) {
            break;
        }

        const QFontMetrics fm{resolvedRichTextFont(block.format, baseFont)};
        const auto original{block.text};

        block.text = fm.elidedText(original, Qt::ElideRight, remaining);
        result.width += fm.horizontalAdvance(block.text);
        result.text.blocks.push_back(std::move(block));

        if(result.text.blocks.back().text != original) {
            break;
        }
    }

    return result;
}

void drawText(QPainter* painter, const QStyleOptionViewItem& option, const RichText& text, int x, int baseline)
{
    const bool selected = option.state & QStyle::State_Selected;

    for(const auto& block : text.blocks) {
        const QFont font = resolvedRichTextFont(block.format, option.font);
        painter->setFont(font);
        painter->setPen(selected ? option.palette.color(QPalette::HighlightedText)
                                 : resolvedRichTextColour(block.format, option.palette.color(QPalette::Text),
                                                          option.palette.color(QPalette::Link)));
        painter->drawText(QPoint{x, baseline}, block.text);
        x += QFontMetrics{font}.horizontalAdvance(block.text);
    }
}
} // namespace

PlaylistHeaderLayout preparePlaylistHeader(const RichText& text, const QFont& baseFont, bool measureWidth)
{
    PlaylistHeaderLayout layout;

    const auto lines = splitRichTextLines(text);
    for(const auto& richLine : lines) {
        PlaylistHeaderLine line;
        line.baseline      = textBaselineMetrics(baseFont);
        line.leftBaseline  = line.baseline;
        line.rightBaseline = line.baseline;

        for(const auto& block : richLine.blocks) {
            if(block.type == RichTextBlock::Type::Line) {
                line.rule = block.format;
                continue;
            }

            const QFontMetrics fm{resolvedRichTextFont(block.format, baseFont)};
            line.baseline.expand(fm);

            if(block.format.alignment == RichAlignment::Right) {
                line.rightBaseline.expand(fm);
                line.right.blocks.push_back(block);
                if(measureWidth) {
                    line.rightWidth += fm.horizontalAdvance(block.text);
                }
            }
            else {
                line.leftBaseline.expand(fm);
                line.left.blocks.push_back(block);
                if(measureWidth) {
                    line.leftWidth += fm.horizontalAdvance(block.text);
                }
            }
        }

        const bool hasText = !line.left.empty() || !line.right.empty();
        line.height        = !hasText && line.rule ? RuleHeight : line.baseline.height();

        const int gap = !line.left.empty() && !line.right.empty() ? TextGap : 0;
        layout.width  = std::max(layout.width, line.leftWidth + line.rightWidth + gap);

        layout.height += line.height;
        layout.lines.push_back(std::move(line));
    }

    return layout;
}

void drawPlaylistHeader(QPainter* painter, const QStyleOptionViewItem& option, const QRect& rect,
                        const PlaylistHeaderLayout& layout)
{
    painter->save();
    painter->setClipRect(rect, Qt::IntersectClip);

    int y = rect.top() + std::max(0, (rect.height() - layout.height) / 2);
    for(const auto& line : layout.lines) {
        const int rightWidth    = line.left.empty() ? rect.width() : rect.width() / 2;
        const auto right        = elideText(line.right, option.font, rightWidth);
        const int gap           = !line.left.empty() && !line.right.empty() ? TextGap : 0;
        const auto left         = elideText(line.left, option.font, std::max(0, rect.width() - right.width - gap));
        const int leftBaseline  = y + ((line.height - line.leftBaseline.height()) / 2) + line.leftBaseline.ascent;
        const int rightBaseline = y + ((line.height - line.rightBaseline.height()) / 2) + line.rightBaseline.ascent;

        drawText(painter, option, left.text, rect.left(), leftBaseline);
        drawText(painter, option, right.text, rect.right() - right.width + 1, rightBaseline);

        if(line.rule) {
            QColor colour = option.palette.color(QPalette::Text);
            colour.setAlpha(40);
            colour = resolvedRichTextColour(*line.rule, colour, option.palette.color(QPalette::Link));
            painter->setPen(QPen{colour, 1});

            const int start = rect.left() + left.width + (left.width > 0 ? TextGap / 2 : 0);
            const int end   = rect.right() - right.width - (right.width > 0 ? TextGap / 2 : 0);
            if(end > start) {
                painter->drawLine(start, y + (line.height / 2), end, y + (line.height / 2));
            }
        }

        y += line.height;
    }

    painter->restore();
}
} // namespace Fooyin
