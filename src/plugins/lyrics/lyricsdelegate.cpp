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

#include <QGlyphRun>
#include <QPainter>
#include <QPainterPath>
#include <QTextOption>

#include <algorithm>
#include <cmath>

constexpr size_t MaxLayoutCacheEntries = 128;

namespace Fooyin::Lyrics {
namespace {
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

void calculateLayout(const QModelIndex& index, const QFont& baseFont, const QColor& baseColour,
                     LyricsDelegate::CachedLayout& layout)
{
    const auto richText = index.data(LyricsModel::RichTextRole).value<RichText>();
    // Keep word ranges for seeking, but shape and reorder the complete line together
    layout.textLayout = std::make_unique<QTextLayout>(richText.joinedText(), baseFont);
    layout.textLayout->setCacheEnabled(true);

    QTextOption textOption;
    textOption.setTextDirection(Qt::LayoutDirectionAuto);
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    textOption.setAlignment(static_cast<Qt::Alignment>(index.data(Qt::TextAlignmentRole).toInt()));
    layout.textLayout->setTextOption(textOption);

    QList<QTextLayout::FormatRange> formats;
    for(int position{0}; const auto& block : richText.blocks) {
        const auto length = static_cast<int>(block.text.size());
        layout.words.emplace_back(position, length);

        QTextCharFormat format;
        format.setFont(resolvedRichTextFont(block.format, baseFont));
        format.setForeground(resolvedRichTextColour(block.format, baseColour));
        if(!formats.empty() && formats.back().format == format) {
            formats.back().length += length;
        }
        else {
            formats.emplace_back(position, length, format);
        }

        position += length;
    }
    layout.textLayout->setFormats(formats);

    const int lineSpacing = index.data(LyricsModel::LineSpacingRole).toInt();
    qreal height{0.0};
    layout.textLayout->beginLayout();
    while(true) {
        auto line = layout.textLayout->createLine();
        if(!line.isValid()) {
            break;
        }
        if(height > 0.0) {
            height += lineSpacing;
        }
        line.setLineWidth(layout.width);
        line.setPosition({0.0, height});
        height += line.height();
    }
    layout.textLayout->endLayout();
    layout.totalHeight = static_cast<int>(std::ceil(height));
}

void calculateProgressRects(const LyricsDelegate::CachedLayout& layout)
{
    if(!layout.chunks.empty()) {
        return;
    }

    const QString text = layout.textLayout->text();
    for(int wordIndex{0}; std::cmp_less(wordIndex, layout.words.size()); ++wordIndex) {
        const auto& word       = layout.words[wordIndex];
        const bool rightToLeft = QStringView{text}.sliced(word.start, word.length).isRightToLeft();

        for(int lineIndex{0}; lineIndex < layout.textLayout->lineCount(); ++lineIndex) {
            const auto line = layout.textLayout->lineAt(lineIndex);
            const int start = std::max(word.start, line.textStart());
            const int end   = std::min(word.start + word.length, line.textStart() + line.textLength());
            if(start >= end) {
                continue;
            }

            const auto firstChunk = static_cast<int>(layout.chunks.size());

            const auto runs = line.glyphRuns(start, end - start);
            for(const auto& run : runs) {
                QRectF rect = run.boundingRect();
                rect.setTop(line.y());
                rect.setHeight(line.height());
                if(rect.isEmpty()) {
                    continue;
                }

                // Font fallback can return several runs covering the same selection
                const auto overlapping = std::find_if(
                    layout.chunks.begin() + firstChunk, layout.chunks.end(), [&rect, &run](const auto& chunk) {
                        return chunk.rightToLeft == run.isRightToLeft() && chunk.rect.intersects(rect);
                    });
                if(overlapping != layout.chunks.end()) {
                    overlapping->rect = overlapping->rect.united(rect);
                }
                else {
                    layout.chunks.emplace_back(rect, wordIndex, run.isRightToLeft());
                }
            }

            std::sort(layout.chunks.begin() + firstChunk, layout.chunks.end(),
                      [rightToLeft](const auto& lhs, const auto& rhs) {
                          return rightToLeft ? lhs.rect.right() > rhs.rect.right() : lhs.rect.left() < rhs.rect.left();
                      });
        }
    }
}

void drawProgressOverlay(QPainter* painter, const QPointF& origin, const LyricsDelegate::CachedLayout& layout,
                         const LyricsDelegate::TextRange& range, int wordIndex, qreal progress,
                         const QColor& progressColour)
{
    if(progress <= 0.0 || !progressColour.isValid()) {
        return;
    }

    calculateProgressRects(layout);

    qreal totalWidth{0.0};
    for(const auto& chunk : layout.chunks) {
        if(wordIndex < 0 || chunk.blockIndex == wordIndex) {
            totalWidth += chunk.rect.width();
        }
    }

    qreal remainingWidth = progress * totalWidth;

    QPainterPath clip;
    clip.setFillRule(Qt::WindingFill);

    for(const auto& chunk : layout.chunks) {
        if(remainingWidth <= 0.0) {
            break;
        }
        if(wordIndex >= 0 && chunk.blockIndex != wordIndex) {
            continue;
        }

        const qreal fillWidth = std::min(chunk.rect.width(), remainingWidth);
        QRectF rect{chunk.rect};
        if(chunk.rightToLeft) {
            rect.setLeft(rect.right() - fillWidth);
        }
        else {
            rect.setWidth(fillWidth);
        }
        clip.addRect(rect.translated(origin));
        remainingWidth -= fillWidth;
    }

    QTextCharFormat format;
    format.setForeground(progressColour);
    painter->save();
    painter->setClipPath(clip, Qt::IntersectClip);
    layout.textLayout->draw(painter, origin, {{.start = range.start, .length = range.length, .format = format}});
    painter->restore();
}
} // namespace

void LyricsDelegate::clearLayoutCache()
{
    m_layoutCache.clear();
}

const LyricsDelegate::CachedLayout& LyricsDelegate::layoutForIndex(const QModelIndex& index, int width,
                                                                   const QFont& font, const QColor& baseColour) const
{
    width = std::max(width, 1);
    ++m_layoutCacheUseCounter;

    if(!index.isValid()) {
        m_layoutCache.clear();
    }

    const int row     = index.row();
    const auto cached = std::ranges::find_if(m_layoutCache, [&](const auto& layout) {
        return layout.row == row && layout.width == width && layout.font == font && layout.baseColour == baseColour;
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

    auto& layout      = m_layoutCache.emplace_back();
    layout.row        = row;
    layout.font       = font;
    layout.baseColour = baseColour;
    layout.width      = width;
    layout.lastUsed   = m_layoutCacheUseCounter;

    calculateLayout(index, font, baseColour, layout);
    return layout;
}

void LyricsDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if(index.data(LyricsModel::IsPaddingRole).toBool()) {
        return;
    }

    const auto margins      = index.data(LyricsModel::MarginsRole).value<QMargins>();
    const QRect contentRect = option.rect.adjusted(margins.left(), 0, -margins.right(), 0);
    const auto& layout = layoutForIndex(index, contentRect.width(), option.font, option.palette.color(QPalette::Text));
    if(layout.textLayout->lineCount() == 0) {
        return;
    }

    qreal progress{0.0};
    QColor progressColour;
    QColor progressBaseColour;
    int progressWord{-1};
    TextRange progressRange{.start = 0, .length = static_cast<int>(layout.textLayout->text().size())};

    const auto lyricsType = static_cast<Lyrics::Type>(index.data(LyricsModel::LyricsTypeRole).toInt());
    if(lyricsType == Lyrics::Type::Synced || lyricsType == Lyrics::Type::SyncedWords) {
        const auto timestamp   = index.data(LyricsModel::TimestampRole).value<uint64_t>();
        const auto duration    = index.data(LyricsModel::DurationRole).value<uint64_t>();
        const auto currentTime = index.data(LyricsModel::CurrentTimeRole).value<uint64_t>();

        if(currentTime >= timestamp && currentTime < timestamp + duration) {
            if(lyricsType == Lyrics::Type::Synced) {
                progress           = progressForTime(currentTime, timestamp, duration);
                progressColour     = index.data(LyricsModel::ProgressColourRole).value<QColor>();
                progressBaseColour = index.data(LyricsModel::ProgressBaseColourRole).value<QColor>();
            }
            else {
                const auto words = index.data(LyricsModel::WordsRole).value<std::vector<ParsedWord>>();
                const auto word  = std::ranges::find_if(
                    words, [currentTime](const ParsedWord& parsedWord) { return parsedWord.isCurrent(currentTime); });
                if(word != words.cend()) {
                    progressWord       = static_cast<int>(std::distance(words.cbegin(), word));
                    progressRange      = layout.words.at(progressWord);
                    progress           = progressForTime(currentTime, word->timestamp, word->duration);
                    progressColour     = index.data(LyricsModel::ProgressColourRole).value<QColor>();
                    progressBaseColour = index.data(LyricsModel::ProgressBaseColourRole).value<QColor>();
                }
            }
        }
    }

    QList<QTextLayout::FormatRange> selections;
    if(progressBaseColour.isValid()) {
        QTextCharFormat format;
        format.setForeground(progressBaseColour);
        selections.emplace_back(progressRange.start, progressRange.length, format);
    }

    painter->save();
    const QPointF origin = contentRect.topLeft();
    layout.textLayout->draw(painter, origin, selections);
    drawProgressOverlay(painter, origin, layout, progressRange, progressWord, progress, progressColour);
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

    int totalHeight
        = layoutForIndex(index, availableWidth, option.font, option.palette.color(QPalette::Text)).totalHeight;

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

    const auto& layout     = layoutForIndex(index, contentRect.width(), opt.font, opt.palette.color(QPalette::Text));
    const QPointF localPos = pos - contentRect.topLeft();
    for(int lineIndex{0}; lineIndex < layout.textLayout->lineCount(); ++lineIndex) {
        const auto line = layout.textLayout->lineAt(lineIndex);
        if(!line.naturalTextRect().contains(localPos) || line.textLength() == 0) {
            continue;
        }

        const int position = std::clamp(line.xToCursor(localPos.x(), QTextLine::CursorOnCharacter), line.textStart(),
                                        line.textStart() + line.textLength() - 1);
        const auto word    = std::ranges::find_if(layout.words, [position](const TextRange& range) {
            return position >= range.start && position < range.start + range.length;
        });
        if(word != layout.words.cend()) {
            return static_cast<int>(std::distance(layout.words.cbegin(), word));
        }
    }

    return -1;
}
} // namespace Fooyin::Lyrics
