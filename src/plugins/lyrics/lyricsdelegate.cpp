/*
 * Fooyin
 * Copyright © 2025, Luke Taylor <luket@pm.me>
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

#include "lyrics.h"
#include "lyricsmodel.h"

#include <gui/scripting/richtext.h>
#include <gui/scripting/richtextutils.h>

#include <QApplication>
#include <QPainter>
#include <QTextBoundaryFinder>

#include <algorithm>
#include <numeric>

constexpr size_t MaxLayoutCacheEntries = 128;

namespace Fooyin::Lyrics {
namespace {
int firstGraphemeLength(const QString& text)
{
    QTextBoundaryFinder boundary{QTextBoundaryFinder::BoundaryType::Grapheme, text};
    boundary.toStart();

    const int next = boundary.toNextBoundary();
    return next > 0 ? next : 1;
}

int fittingLength(const QString& text, int maxWidth, const QFontMetrics& fm)
{
    if(text.isEmpty() || maxWidth <= 0) {
        return 0;
    }

    QTextBoundaryFinder boundary{QTextBoundaryFinder::BoundaryType::Grapheme, text};
    boundary.toStart();

    int bestLength{0};
    for(int next = boundary.toNextBoundary(); next >= 0; next = boundary.toNextBoundary()) {
        if(fm.horizontalAdvance(text.left(next)) > maxWidth) {
            break;
        }
        bestLength = next;
    }

    return bestLength;
}

int preferredSplitLength(const QString& text, int maxWidth, const QFontMetrics& fm)
{
    const int splitLength = fittingLength(text, maxWidth, fm);
    if(splitLength <= 0) {
        return firstGraphemeLength(text);
    }

    for(int i{splitLength - 1}; i >= 0; --i) {
        if(text.at(i).isSpace()) {
            return i + 1;
        }
    }

    return splitLength;
}

int textHeight(const QString& text, const QFontMetrics& fm)
{
    if(text.isEmpty()) {
        return fm.height();
    }
    return std::max(fm.height(), fm.boundingRect(text).height());
}

qreal progressForTime(uint64_t currentTime, uint64_t startTime, uint64_t duration)
{
    if(duration == 0 || currentTime <= startTime) {
        return 0.0;
    }

    if(currentTime >= startTime + duration) {
        return 1.0;
    }

    return static_cast<qreal>(currentTime - startTime) / static_cast<qreal>(duration);
}

void calculateWordRects(const QModelIndex& index, int width, const QFont& baseFont,
                        std::vector<LyricsDelegate::LaidOutChunk>& wordRects, int& totalHeight)
{
    const auto lineSpacing = index.data(LyricsModel::LineSpacingRole).toInt();
    const auto alignment   = index.data(Qt::TextAlignmentRole).toInt();

    const auto richText = index.data(LyricsModel::RichTextRole).value<RichText>();
    if(richText.empty()) {
        totalHeight = lineSpacing;
        return;
    }

    const int rightEdge = std::max(width, 1);
    int currentY{0};
    int lineHeight{0};
    std::vector<LyricsDelegate::LaidOutChunk> currentLineRects;
    int currentLineWidth{0};

    const auto flushLine = [&](bool addSpacing) {
        int lineStartX{0};
        switch(alignment) {
            case Qt::AlignRight:
                lineStartX = rightEdge - currentLineWidth;
                break;
            case Qt::AlignCenter:
                lineStartX = (rightEdge - currentLineWidth) / 2;
                break;
            case Qt::AlignLeft:
            default:
                lineStartX = 0;
                break;
        }

        int x = lineStartX;
        for(auto& chunk : currentLineRects) {
            chunk.rect.moveLeft(x);
            chunk.rect.moveTop(currentY);
            wordRects.emplace_back(chunk);
            x += chunk.rect.width();
        }

        currentY += lineHeight + (addSpacing ? lineSpacing : 0);
        currentLineRects.clear();
        currentLineWidth = 0;
        lineHeight       = 0;
    };

    for(size_t i{0}; i < richText.blocks.size(); ++i) {
        const auto& block = richText.blocks[i];
        const QFontMetrics fm{Fooyin::resolvedRichTextFont(block.format, baseFont)};
        QString remainingText{block.text};

        while(!remainingText.isEmpty()) {
            const int remainingWidth  = fm.horizontalAdvance(remainingText);
            const int remainingHeight = textHeight(remainingText, fm);

            if(currentLineWidth >= rightEdge && !currentLineRects.empty()) {
                flushLine(true);
            }

            if(!currentLineRects.empty() && currentLineWidth + remainingWidth > rightEdge) {
                flushLine(true);
                continue;
            }

            if(remainingWidth <= rightEdge - currentLineWidth) {
                currentLineRects.emplace_back(QRect{0, 0, remainingWidth, remainingHeight}, static_cast<int>(i),
                                              remainingText);
                currentLineWidth += remainingWidth;
                lineHeight = std::max(lineHeight, remainingHeight);
                break;
            }

            const int availableWidth  = rightEdge - currentLineWidth;
            const int segmentLength   = preferredSplitLength(remainingText, availableWidth, fm);
            const QString segmentText = remainingText.left(segmentLength);
            const int segmentWidth    = fm.horizontalAdvance(segmentText);
            const int segmentHeight   = textHeight(segmentText, fm);

            if(currentLineWidth + segmentWidth > rightEdge && !currentLineRects.empty()) {
                flushLine(true);
                continue;
            }

            currentLineRects.emplace_back(QRect{0, 0, segmentWidth, segmentHeight}, static_cast<int>(i), segmentText);
            currentLineWidth += segmentWidth;
            lineHeight = std::max(lineHeight, segmentHeight);

            remainingText.remove(0, segmentLength);

            if(!remainingText.isEmpty()) {
                flushLine(true);
            }
        }
    }

    if(!currentLineRects.empty()) {
        flushLine(false);
    }

    totalHeight = currentY;
}

void drawProgressOverlay(QPainter* painter, const QStyleOptionViewItem& option, const RichText& richText,
                         const std::vector<LyricsDelegate::LaidOutChunk>& chunks, qreal progress,
                         const QColor& progressColour)
{
    if(chunks.empty() || progress <= 0.0 || !progressColour.isValid()) {
        return;
    }

    const int totalWidth = std::accumulate(chunks.cbegin(), chunks.cend(), 0,
                                           [](int total, const auto& chunk) { return total + chunk.rect.width(); });

    int remainingWidth = std::clamp(static_cast<int>(std::ceil(progress * totalWidth)), 0, totalWidth);
    if(remainingWidth <= 0) {
        return;
    }

    for(const auto& chunk : chunks) {
        if(remainingWidth <= 0) {
            break;
        }

        const int fillWidth = std::min(chunk.rect.width(), remainingWidth);
        if(fillWidth <= 0 || chunk.blockIndex < 0 || std::cmp_greater_equal(chunk.blockIndex, richText.blocks.size())) {
            continue;
        }

        painter->save();

        painter->setClipRect(QRect{chunk.rect.left(), chunk.rect.top(), fillWidth, chunk.rect.height()});
        painter->setFont(resolvedRichTextFont(richText.blocks[chunk.blockIndex].format, option.font));
        painter->setPen(progressColour);
        painter->drawText(chunk.rect, Qt::AlignLeft | Qt::AlignVCenter, chunk.text);

        painter->restore();

        remainingWidth -= fillWidth;
    }
}

bool containsChunk(const std::vector<LyricsDelegate::LaidOutChunk>& chunks, const LyricsDelegate::LaidOutChunk& chunk)
{
    return std::ranges::any_of(chunks, [&chunk](const LyricsDelegate::LaidOutChunk& candidate) {
        return candidate.blockIndex == chunk.blockIndex && candidate.rect == chunk.rect && candidate.text == chunk.text;
    });
}
} // namespace

void LyricsDelegate::clearLayoutCache()
{
    m_layoutCache.clear();
}

const LyricsDelegate::CachedLayout& LyricsDelegate::layoutForIndex(const QModelIndex& index, int width,
                                                                   const QFont& font) const
{
    width = std::max(width, 1);
    ++m_layoutCacheUseCounter;

    if(!index.isValid()) {
        m_layoutCache.clear();
    }

    const int row     = index.row();
    const auto cached = std::ranges::find_if(m_layoutCache, [&](const auto& layout) {
        return layout.row == row && layout.width == width && layout.font == font;
    });
    if(cached != m_layoutCache.end()) {
        cached->lastUsed = m_layoutCacheUseCounter;
        return *cached;
    }

    if(m_layoutCache.size() >= MaxLayoutCacheEntries) {
        const auto leastRecentlyUsed = std::ranges::min_element(m_layoutCache, {}, &CachedLayout::lastUsed);
        if(leastRecentlyUsed != m_layoutCache.end()) {
            m_layoutCache.erase(leastRecentlyUsed);
        }
    }

    auto& layout    = m_layoutCache.emplace_back();
    layout.row      = row;
    layout.font     = font;
    layout.width    = width;
    layout.lastUsed = m_layoutCacheUseCounter;

    calculateWordRects(index, width, font, layout.chunks, layout.totalHeight);
    return layout;
}

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

    std::vector<LaidOutChunk> wordRects{layoutForIndex(index, contentRect.width(), option.font).chunks};

    // Offset word rects
    for(auto& chunk : wordRects) {
        chunk.rect.translate(contentRect.left(), option.rect.top());
    }

    qreal progress{0.0};
    QColor progressColour;
    QColor progressBaseColour;
    std::vector<LaidOutChunk> progressChunks;
    const auto lyricsType = static_cast<Lyrics::Type>(index.data(LyricsModel::LyricsTypeRole).toInt());
    if(lyricsType == Lyrics::Type::Synced) {
        const auto timestamp   = index.data(LyricsModel::TimestampRole).value<uint64_t>();
        const auto duration    = index.data(LyricsModel::DurationRole).value<uint64_t>();
        const auto currentTime = index.data(LyricsModel::CurrentTimeRole).value<uint64_t>();

        if(currentTime >= timestamp && currentTime < timestamp + duration) {
            progress           = progressForTime(currentTime, timestamp, duration);
            progressColour     = index.data(LyricsModel::ProgressColourRole).value<QColor>();
            progressBaseColour = index.data(LyricsModel::ProgressBaseColourRole).value<QColor>();
            progressChunks     = wordRects;
        }
    }
    else if(lyricsType == Lyrics::Type::SyncedWords) {
        const auto currentTime = index.data(LyricsModel::CurrentTimeRole).value<uint64_t>();
        const auto timestamp   = index.data(LyricsModel::TimestampRole).value<uint64_t>();
        const auto duration    = index.data(LyricsModel::DurationRole).value<uint64_t>();

        if(currentTime >= timestamp && currentTime < timestamp + duration) {
            const auto words = index.data(LyricsModel::WordsRole).value<std::vector<ParsedWord>>();

            const auto word = std::ranges::find_if(words, [currentTime](const ParsedWord& parsedWord) {
                return currentTime >= parsedWord.timestamp && currentTime < parsedWord.endTimestamp();
            });

            if(word != words.cend()) {
                const int wordIndex = static_cast<int>(std::distance(words.cbegin(), word));
                std::ranges::copy_if(wordRects, std::back_inserter(progressChunks),
                                     [wordIndex](const LaidOutChunk& chunk) { return chunk.blockIndex == wordIndex; });

                progress           = progressForTime(currentTime, word->timestamp, word->duration);
                progressColour     = index.data(LyricsModel::ProgressColourRole).value<QColor>();
                progressBaseColour = index.data(LyricsModel::ProgressBaseColourRole).value<QColor>();
            }
        }
    }

    const bool allChunksUseProgressBase = progressChunks.size() == wordRects.size();
    for(const auto& chunk : wordRects) {
        const auto& format = richText.blocks[chunk.blockIndex].format;

        painter->setFont(resolvedRichTextFont(format, option.font));
        painter->setPen((allChunksUseProgressBase || containsChunk(progressChunks, chunk))
                                && progressBaseColour.isValid()
                            ? progressBaseColour
                            : resolvedRichTextColour(format, option.palette.color(QPalette::Text)));

        painter->drawText(chunk.rect, Qt::AlignLeft | Qt::AlignVCenter, chunk.text);
    }

    drawProgressOverlay(painter, option, richText, progressChunks, progress, progressColour);

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

    int totalHeight = layoutForIndex(index, availableWidth, option.font).totalHeight;

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

    std::vector<LaidOutChunk> wordRects{layoutForIndex(index, contentRect.width(), opt.font).chunks};

    for(auto& chunk : wordRects) {
        chunk.rect.translate(contentRect.left(), opt.rect.top());
    }

    for(const auto& chunk : wordRects) {
        if(chunk.rect.contains(pos)) {
            return chunk.blockIndex;
        }
    }

    return -1;
}
} // namespace Fooyin::Lyrics
