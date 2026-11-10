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

#include "richtextlayout.h"

#include <gui/guiutils.h>
#include <gui/iconloader.h>

#include <QFontMetrics>
#include <QPainter>
#include <QStringList>
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

ElidedHeaderText elideText(const RichText& text, const QFont& baseFont, int availableWidth,
                           Qt::TextElideMode mode = Qt::ElideRight)
{
    ElidedHeaderText result;
    const bool fromRight = (mode == Qt::ElideLeft);

    for(int i{0}; std::cmp_less(i, text.blocks.size()); ++i) {
        auto block          = text.blocks[fromRight ? text.blocks.size() - i - 1 : i];
        const int remaining = availableWidth - result.width;
        if(remaining <= 0) {
            break;
        }

        const QFontMetrics fm{resolvedRichTextFont(block.format, baseFont)};
        if(block.type == RichTextBlock::Type::PlayingIcon) {
            const int width = std::min(fm.height(), remaining);
            result.width += width;
            result.text.blocks.push_back(std::move(block));
            continue;
        }
        const auto original{block.text};

        block.text = fm.elidedText(original, mode, remaining);
        result.width += fm.horizontalAdvance(block.text);
        result.text.blocks.push_back(std::move(block));

        if(result.text.blocks.back().text != original) {
            break;
        }
    }

    if(fromRight) {
        std::ranges::reverse(result.text.blocks);
    }

    return result;
}

void drawText(QPainter* painter, const QStyleOptionViewItem& option, const RichText& text, int x, int baseline,
              const QIcon& playingIcon)
{
    const bool selected = option.state & QStyle::State_Selected;

    for(const auto& block : text.blocks) {
        const QFont font = resolvedRichTextFont(block.format, option.font);
        painter->setFont(font);
        painter->setPen(selected ? option.palette.color(Gui::itemViewSelectionTextRole(option))
                                 : resolvedRichTextColour(block.format, option.palette.color(QPalette::Text),
                                                          option.palette.color(QPalette::Link)));
        const QFontMetrics metrics{font};
        if(block.type == RichTextBlock::Type::PlayingIcon) {
            const int extent = metrics.height();
            Gui::drawItemViewIcon(painter, option, playingIcon, {x, baseline - metrics.ascent(), extent, extent});
            x += extent;
        }
        else {
            painter->drawText(QPoint{x, baseline}, block.text);
            x += metrics.horizontalAdvance(block.text);
        }
    }
}
} // namespace

RichTextLayout prepareRichTextLayout(const RichText& text, const QFont& baseFont, bool measureWidth,
                                     const InlineTrackEditors* editors)
{
    RichTextLayout layout;

    const auto lines = splitRichTextLines(text);
    for(const auto& richLine : lines) {
        RichTextLine line;
        line.baseline      = textBaselineMetrics(baseFont);
        line.leftBaseline  = line.baseline;
        line.rightBaseline = line.baseline;

        int leftEditorHeight{0};
        int rightEditorHeight{0};

        for(const auto& block : richLine.blocks) {
            if(block.type == RichTextBlock::Type::Line) {
                line.rule = block.format;
                continue;
            }

            const QFontMetrics fm{resolvedRichTextFont(block.format, baseFont)};
            line.baseline.expand(fm);

            const auto editor      = editors ? inlineTrackEditorType(block) : InlineTrackEditor::None;
            const bool hasEditor   = editor != InlineTrackEditor::None;
            const QSize editorSize = hasEditor ? editors->size(editor) : QSize{};
            const int width        = hasEditor                                      ? editorSize.width()
                                   : block.type == RichTextBlock::Type::PlayingIcon ? fm.height()
                                                                                    : fm.horizontalAdvance(block.text);

            if(block.format.alignment == RichAlignment::Right) {
                line.rightBaseline.expand(fm);
                line.right.blocks.push_back(block);
                if(measureWidth) {
                    line.rightWidth += width;
                }
                rightEditorHeight = std::max(rightEditorHeight, editorSize.height());
            }
            else {
                line.leftBaseline.expand(fm);
                line.left.blocks.push_back(block);
                if(measureWidth) {
                    line.leftWidth += width;
                }
                leftEditorHeight = std::max(leftEditorHeight, editorSize.height());
            }
        }

        line.leftBaseline.ascent  = std::max(line.leftBaseline.ascent, leftEditorHeight - line.leftBaseline.descent);
        line.rightBaseline.ascent = std::max(line.rightBaseline.ascent, rightEditorHeight - line.rightBaseline.descent);
        line.baseline.ascent
            = std::max(line.baseline.ascent, std::max(leftEditorHeight, rightEditorHeight) - line.baseline.descent);

        const bool hasText = !line.left.empty() || !line.right.empty();
        line.height        = !hasText && line.rule ? RuleHeight : line.baseline.height();

        const int gap = !line.left.empty() && !line.right.empty() ? TextGap : 0;
        layout.width  = std::max(layout.width, line.leftWidth + line.rightWidth + gap);

        layout.height += line.height;
        layout.lines.push_back(std::move(line));
    }

    return layout;
}

QString richTextLayoutPlainText(const RichTextLayout& layout)
{
    QStringList lines;
    for(const auto& line : layout.lines) {
        auto text        = inlineTrackEditorPlainText(line.left);
        const auto right = inlineTrackEditorPlainText(line.right);
        if(!text.isEmpty() && !right.isEmpty() && !text.back().isSpace() && !right.front().isSpace()) {
            text += u' ';
        }
        text += right;
        lines.push_back(std::move(text));
    }
    return lines.join(u'\n');
}

bool hasInlineTrackEditors(const RichTextLayout& layout)
{
    return std::ranges::any_of(layout.lines, [](const RichTextLine& line) {
        return hasInlineTrackEditors(line.left) || hasInlineTrackEditors(line.right);
    });
}

std::vector<InlineTrackEditorHit> drawRichTextLayout(QPainter* painter, const QStyleOptionViewItem& option,
                                                     const QRect& rect, const RichTextLayout& layout,
                                                     const QIcon& playingIcon, const InlineTrackEditors* editors)
{
    if(painter) {
        painter->save();
        painter->setClipRect(rect, Qt::IntersectClip);
    }

    std::vector<InlineTrackEditorHit> hits;

    int y = rect.top() + std::max(0, (rect.height() - layout.height) / 2);
    for(const auto& line : layout.lines) {
        const int rightWidth = line.left.empty() ? rect.width() : rect.width() / 2;
        const int gap        = !line.left.empty() && !line.right.empty() ? TextGap : 0;

        int leftWidth{0};
        int drawnRightWidth{0};
        if(editors) {
            const QRect rightRect{rect.right() - rightWidth + 1, y, rightWidth, line.height};
            auto right = drawTrackTextBlocks(painter, option, rightRect, line.right.blocks,
                                             Qt::AlignVCenter | Qt::AlignRight, playingIcon, editors);
            const QRect leftRect{rect.left(), y, std::max(0, rect.width() - right.totalWidth - gap), line.height};
            auto left       = drawTrackTextBlocks(painter, option, leftRect, line.left.blocks,
                                                  Qt::AlignVCenter | Qt::AlignLeft, playingIcon, editors);
            leftWidth       = left.totalWidth;
            drawnRightWidth = right.totalWidth;
            hits.insert(hits.end(), left.editors.begin(), left.editors.end());
            hits.insert(hits.end(), right.editors.begin(), right.editors.end());
        }
        else {
            const auto right        = elideText(line.right, option.font, rightWidth, Qt::ElideLeft);
            const auto left         = elideText(line.left, option.font, std::max(0, rect.width() - right.width - gap));
            const int leftBaseline  = y + ((line.height - line.leftBaseline.height()) / 2) + line.leftBaseline.ascent;
            const int rightBaseline = y + ((line.height - line.rightBaseline.height()) / 2) + line.rightBaseline.ascent;
            if(painter) {
                drawText(painter, option, left.text, rect.left(), leftBaseline, playingIcon);
                drawText(painter, option, right.text, rect.right() - right.width + 1, rightBaseline, playingIcon);
            }
            leftWidth       = left.width;
            drawnRightWidth = right.width;
        }

        if(line.rule && painter) {
            QColor colour = option.palette.color(QPalette::Text);
            colour.setAlpha(40);
            colour = resolvedRichTextColour(*line.rule, colour, option.palette.color(QPalette::Link));
            painter->setPen(QPen{colour, 1});

            const int start = rect.left() + leftWidth + (leftWidth > 0 ? TextGap / 2 : 0);
            const int end   = rect.right() - drawnRightWidth - (drawnRightWidth > 0 ? TextGap / 2 : 0);
            if(end > start) {
                painter->drawLine(start, y + (line.height / 2), end, y + (line.height / 2));
            }
        }

        y += line.height;
    }

    if(painter) {
        painter->restore();
    }

    for(auto& hit : hits) {
        hit.rect = hit.rect.intersected(rect);
    }
    std::erase_if(hits, [](const InlineTrackEditorHit& hit) { return hit.rect.isEmpty(); });
    return hits;
}
} // namespace Fooyin
