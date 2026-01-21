/*
 * Fooyin
 * Copyright © 2025, Luke Taylor <LukeT1@proton.me>
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

#include "lyricsdelegate.h"

#include "lyricsmodel.h"

#include <gui/scripting/richtext.h>

#include <QApplication>
#include <QPainter>

#include <ranges>

namespace {
void calculateWordRects(const QModelIndex& index, const QRect& boundingRect,
                        std::vector<std::pair<QRect, int>>& wordRects, int& totalHeight)
{
    const auto lineSpacing = index.data(Fooyin::Lyrics::LyricsModel::LineSpacingRole).toInt();
    const auto alignment   = index.data(Qt::TextAlignmentRole).toInt();

    const auto richText = index.data(Fooyin::Lyrics::LyricsModel::RichTextRole).value<Fooyin::RichText>();
    if(richText.empty()) {
        totalHeight = lineSpacing;
        return;
    }

    const int rightEdge = boundingRect.width();
    int currentY{0};
    int lineHeight{0};
    std::vector<std::pair<QRect, int>> currentLineRects;
    int currentLineWidth{0};

    const auto flushLine = [&](bool addSpacing) {
        int lineStartX{0};
        switch(alignment) {
            case(Qt::AlignRight):
                lineStartX = rightEdge - currentLineWidth;
                break;
            case(Qt::AlignCenter):
                lineStartX = (rightEdge - currentLineWidth) / 2;
                break;
            case(Qt::AlignLeft):
            default:
                lineStartX = 0;
                break;
        }

        int x = lineStartX + boundingRect.left();
        for(auto& [rect, idx] : currentLineRects) {
            rect.moveLeft(x);
            rect.moveTop(currentY);
            wordRects.emplace_back(rect, idx);
            x += rect.width();
        }

        currentY += lineHeight + (addSpacing ? lineSpacing : 0);
        currentLineRects.clear();
        currentLineWidth = 0;
        lineHeight       = 0;
    };

    for(size_t i{0}; i < richText.blocks.size(); ++i) {
        const auto& block = richText.blocks[i];
        const QFontMetrics fm{block.format.font};
        const int wordWidth  = fm.horizontalAdvance(block.text);
        const int wordHeight = fm.height();

        if(currentLineWidth + wordWidth > rightEdge && !currentLineRects.empty()) {
            flushLine(true);
        }

        currentLineRects.emplace_back(QRect{0, 0, wordWidth, wordHeight}, static_cast<int>(i));
        currentLineWidth += wordWidth;
        lineHeight = std::max(lineHeight, wordHeight);
    }

    if(!currentLineRects.empty()) {
        flushLine(false);
    }

    totalHeight = currentY;
}
} // namespace

namespace Fooyin::Lyrics {
void LyricsDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    painter->save();

    const auto bg = index.data(Qt::BackgroundRole).value<QBrush>();
    if(bg != Qt::NoBrush) {
        painter->fillRect(option.rect, bg);
    }

    if(index.data(LyricsModel::IsPaddingRole).toBool()) {
        painter->restore();
        return;
    }

    const auto richText = index.data(LyricsModel::RichTextRole).value<RichText>();
    if(richText.empty()) {
        painter->restore();
        return;
    }

    const auto margins = index.data(LyricsModel::MarginsRole).value<QMargins>();

    // Apply left/right padding
    const QRect contentRect = option.rect.adjusted(margins.left(), 0, -margins.right(), 0);

    std::vector<std::pair<QRect, int>> wordRects;
    int totalHeight{0};

    calculateWordRects(index, contentRect, wordRects, totalHeight);

    // Offset word rects
    for(auto& rect : wordRects | std::views::keys) {
        rect.translate(0, option.rect.top());
    }

    for(size_t i{0}; i < wordRects.size() && i < richText.blocks.size(); ++i) {
        const auto& [rect, wordIdx] = wordRects[i];
        const auto& [text, format]  = richText.blocks[wordIdx];

        painter->setFont(format.font);
        painter->setPen(format.colour);

        painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, text);
    }

    painter->restore();
}

QSize LyricsDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if(index.data(LyricsModel::IsPaddingRole).toBool()) {
        const QSize paddingSize = index.data(Qt::SizeHintRole).toSize();
        return {option.rect.width(), paddingSize.height()};
    }

    const auto richText = index.data(LyricsModel::RichTextRole).value<RichText>();

    int viewWidth = option.rect.width();
    if(viewWidth <= 0) {
        if(option.widget) {
            viewWidth = option.widget->width();
        }
        if(viewWidth <= 0) {
            viewWidth = 400; // Fallback
        }
    }

    const auto margins     = index.data(LyricsModel::MarginsRole).value<QMargins>();
    const auto lineSpacing = index.data(LyricsModel::LineSpacingRole).toInt();

    const int availableWidth = viewWidth - margins.left() - margins.right();

    if(richText.empty()) {
        // Empty line
        const QFontMetrics fm{option.font};
        return {viewWidth, fm.height() + lineSpacing};
    }

    const QRect boundingRect{0, 0, availableWidth, 10000};

    std::vector<std::pair<QRect, int>> wordRects;
    int totalHeight{0};
    calculateWordRects(index, boundingRect, wordRects, totalHeight);

    // Ensure minimum height
    if(totalHeight <= 0) {
        const QFontMetrics fm{option.font};
        totalHeight = fm.height();
    }

    return {viewWidth, totalHeight + lineSpacing};
}

int LyricsDelegate::wordIndexAt(const QModelIndex& index, const QPoint& pos, const QRect& idxRect) const
{
    QStyleOptionViewItem opt;
    initStyleOption(&opt, index);
    opt.rect = idxRect;

    if(index.data(LyricsModel::IsPaddingRole).toBool()) {
        return -1;
    }

    const auto margins = index.data(LyricsModel::MarginsRole).value<QMargins>();

    const QRect contentRect = opt.rect.adjusted(margins.left(), 0, -margins.right(), 0);

    std::vector<std::pair<QRect, int>> wordRects;
    int totalHeight{0};

    calculateWordRects(index, contentRect, wordRects, totalHeight);

    for(auto& rect : wordRects | std::views::keys) {
        rect.translate(0, opt.rect.top());
    }

    for(const auto& [rect, idx] : wordRects) {
        if(rect.contains(pos)) {
            return idx;
        }
    }

    return -1;
}
} // namespace Fooyin::Lyrics
