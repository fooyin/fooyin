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

#include "inlinetrackeditors.h"

#include <gui/guisettings.h>
#include <gui/guiutils.h>
#include <gui/iconloader.h>
#include <gui/scripting/richtextutils.h>
#include <utils/settings/settingsmanager.h>
#include <utils/stareditor.h>

#include <QPainter>
#include <QStyleOptionViewItem>

using namespace Qt::StringLiterals;

namespace Fooyin {
QSize InlineTrackEditors::size(InlineTrackEditor type) const
{
    if(type == InlineTrackEditor::Love) {
        return love.sizeHint();
    }
    if(type == InlineTrackEditor::Rating) {
        return rating.sizeHint();
    }
    return {};
}

void InlineTrackEditors::setTrack(const Track& track)
{
    love.setLoved(track.isLoved());
    rating.setRating(track.rating());
}

void InlineTrackEditorHover::update(const QModelIndex& nextIndex, const InlineTrackEditorHit& hit, const QPoint& pos,
                                    const InlineTrackEditors& editors)
{
    if(hit.type == InlineTrackEditor::None) {
        clear();
        return;
    }

    if(index != nextIndex || type != hit.type) {
        loved = !editors.love.loved();
    }

    index = nextIndex;
    type  = hit.type;

    if(type == InlineTrackEditor::Rating) {
        rating = StarEditor::ratingAtPosition(pos, hit.rect, editors.rating);
    }
}

void InlineTrackEditorHover::apply(InlineTrackEditors& editors) const
{
    if(type == InlineTrackEditor::Love) {
        editors.love.setLoved(loved);
    }
    else if(type == InlineTrackEditor::Rating) {
        editors.rating.setRating(rating);
    }
}

void InlineTrackEditorHover::clear()
{
    index = QPersistentModelIndex{};
    type  = InlineTrackEditor::None;
}

InlineTrackEditors inlineTrackEditors(const SettingsManager& settings, const Track& track)
{
    StarRating rating{track.rating(),
                      5,
                      settings.value<Settings::Gui::StarRatingSize>(),
                      Gui::ratingStarColours(settings),
                      Gui::unratedStarColour(settings),
                      Gui::ratingEditorSymbols(settings)};
    rating.setSymbolFont(settings.value<Settings::Gui::RatingSymbolFont>().value<QFont>());
    return {.love            = HeartValue{track.isLoved(), settings.value<Settings::Gui::LoveHeartSize>(),
                                          Gui::loveHeartColour(settings), Gui::unlovedHeartColour(settings)},
            .rating          = std::move(rating),
            .hideEmptyLove   = settings.value<Settings::Gui::LoveEditorOnlyOnHover>(),
            .hideEmptyRating = settings.value<Settings::Gui::RatingEditorOnlyOnHover>()};
}

InlineTrackEditor inlineTrackEditorType(const RichTextBlock& block)
{
    if(block.type != RichTextBlock::Type::Text || block.text.size() != 1) {
        return InlineTrackEditor::None;
    }
    if(block.text.front() == InlineLoveEditorMarker) {
        return InlineTrackEditor::Love;
    }
    if(block.text.front() == InlineRatingEditorMarker) {
        return InlineTrackEditor::Rating;
    }
    return InlineTrackEditor::None;
}

bool hasInlineTrackEditors(const RichText& text)
{
    return std::ranges::any_of(text.blocks, [](const RichTextBlock& block) {
        return inlineTrackEditorType(block) != InlineTrackEditor::None;
    });
}

RichText evaluateInlineTrackScript(ScriptParser& parser, ScriptFormatter& formatter, const ParsedScript& script,
                                   const Track& track, const ScriptContext& context,
                                   const ScriptEvaluationOptions& options)
{
    if(!script.input.contains(u"%love_editor%"_s, Qt::CaseInsensitive)
       && !script.input.contains(u"%rating_editor%"_s, Qt::CaseInsensitive)) {
        return formatter.evaluate(parser.evaluate(script, track, context, options));
    }

    std::vector<ScriptVariableValue> values;
    values.reserve(context.values.size() + 2);

    values.emplace_back(u"love_editor"_s, ScriptResult{.value = QString{InlineLoveEditorMarker}, .cond = true});
    values.emplace_back(u"rating_editor"_s, ScriptResult{.value = QString{InlineRatingEditorMarker}, .cond = true});
    values.insert(values.end(), context.values.begin(), context.values.end());

    auto displayContext{context};
    displayContext.values = values;

    RichText text;
    const auto formatted = formatter.evaluate(parser.evaluate(script, track, displayContext, options));
    for(const auto& block : formatted.blocks) {
        qsizetype start{0};
        for(qsizetype i{0}; i < block.text.size(); ++i) {
            const auto character = block.text.at(i);
            if(character != InlineLoveEditorMarker && character != InlineRatingEditorMarker) {
                continue;
            }

            if(i > start) {
                auto prefix{block};
                prefix.text = block.text.sliced(start, i - start);
                text.blocks.push_back(std::move(prefix));
            }

            auto editor{block};
            editor.text = QString{character};
            text.blocks.push_back(std::move(editor));
            start = i + 1;
        }

        if(start < block.text.size() || block.text.isEmpty()) {
            auto suffix{block};
            suffix.text = block.text.sliced(start);
            text.blocks.push_back(std::move(suffix));
        }
    }

    return text;
}

int inlineTrackEditorTextHeight(const RichText& text, int loveSize, int ratingSize)
{
    int height{0};

    for(const auto& line : splitRichTextLines(text)) {
        int lineHeight = richTextHeight(line);

        for(const auto& block : line.blocks) {
            const auto type = inlineTrackEditorType(block);
            if(type == InlineTrackEditor::Love) {
                lineHeight = std::max(lineHeight, loveSize);
            }
            else if(type == InlineTrackEditor::Rating) {
                lineHeight = std::max(lineHeight, ratingSize);
            }
        }
        height += lineHeight;
    }

    return height;
}

QString inlineTrackEditorPlainText(const RichText& text)
{
    QString result;
    for(const auto& block : text.blocks) {
        if(inlineTrackEditorType(block) == InlineTrackEditor::None) {
            result += block.text;
        }
    }
    return result;
}

InlineTrackEditorHit inlineTrackEditorAt(const std::vector<InlineTrackEditorHit>& editors, const QPoint& pos)
{
    const auto it = std::ranges::find_if(editors, [&pos](const auto& editor) { return editor.rect.contains(pos); });
    return it == editors.end() ? InlineTrackEditorHit{} : *it;
}

QVariant inlineTrackEditorValue(const InlineTrackEditorHit& hit, const QPoint& pos, const InlineTrackEditors& editors)
{
    if(hit.type == InlineTrackEditor::Love) {
        auto love{editors.love};
        love.setLoved(!love.loved());
        return QVariant::fromValue(love);
    }

    if(hit.type != InlineTrackEditor::Rating) {
        return {};
    }

    auto rating{editors.rating};
    rating.setRating(StarEditor::ratingAtPosition(pos, hit.rect, rating));
    return QVariant::fromValue(rating);
}

struct PreparedTextBlock
{
    QString text;
    QFont font;
    QColor colour;
    int width{0};
    bool playingIcon{false};
    InlineTrackEditor editor{InlineTrackEditor::None};
};

struct PreparedTextLine
{
    std::vector<PreparedTextBlock> blocks;
    TextBaselineMetrics baseline;
    int width{0};
};

DrawTrackTextResult drawTrackTextBlocks(QPainter* painter, const QStyleOptionViewItem& option, QRect rect,
                                        const std::vector<RichTextBlock>& blocks, Qt::Alignment alignment,
                                        const QIcon& playingIcon, const InlineTrackEditors* editors)
{
    DrawTrackTextResult result;

    if(painter) {
        painter->save();
        painter->setClipRect(rect, Qt::IntersectClip);
    }

    const auto selected         = option.state & QStyle::State_Selected;
    const QColor defaultColour  = option.palette.color(QPalette::Text);
    const QColor selectedColour = option.palette.color(Gui::itemViewSelectionTextRole(option));
    const QColor linkColour     = option.palette.color(QPalette::Link);

    const RichText richText{.blocks = blocks};
    const auto logicalLines = splitRichTextLines(richText);

    std::vector<PreparedTextLine> preparedLines;
    preparedLines.reserve(logicalLines.size());

    int totalHeight{0};
    for(const auto& logicalLine : logicalLines) {
        PreparedTextLine line;
        line.baseline = textBaselineMetrics(option.font);

        int remainingWidth = rect.width();
        int editorHeight{0};

        for(const auto& block : logicalLine.blocks) {
            const bool isIcon = block.type == RichTextBlock::Type::PlayingIcon;
            const auto editor = editors ? inlineTrackEditorType(block) : InlineTrackEditor::None;
            if((block.text.isEmpty() && !isIcon) || remainingWidth <= 0 || (isIcon && playingIcon.isNull())) {
                continue;
            }

            const QFont font = resolvedRichTextFont(block.format, option.font);
            const QFontMetrics fm{font};

            if(editor != InlineTrackEditor::None) {
                const QSize size = editors->size(editor);
                if(size.width() > remainingWidth) {
                    break;
                }

                line.blocks.push_back(
                    {.text = {}, .font = font, .colour = {}, .width = size.width(), .editor = editor});
                line.baseline.expand(fm);
                editorHeight = std::max(editorHeight, size.height());
                line.width += size.width();
                remainingWidth -= size.width();
                continue;
            }

            if(isIcon) {
                const int width = std::min(fm.height(), remainingWidth);
                line.blocks.push_back({.text = {}, .font = font, .colour = {}, .width = width, .playingIcon = true});
                line.baseline.expand(fm);
                line.width += width;
                remainingWidth -= width;
                continue;
            }

            const QString text = fm.elidedText(block.text, Qt::ElideRight, remainingWidth);
            if(text.isEmpty()) {
                continue;
            }

            QColor blockColour = resolvedRichTextColour(block.format, defaultColour, linkColour);
            if(selected) {
                blockColour = selectedColour;
            }

            const int width = fm.horizontalAdvance(text);
            line.blocks.push_back({.text = text, .font = font, .colour = blockColour, .width = width});
            line.baseline.expand(fm);
            line.width += width;
            remainingWidth -= width;

            if(text != block.text) {
                break;
            }
        }

        line.baseline.ascent = std::max(line.baseline.ascent, editorHeight - line.baseline.descent);
        result.totalWidth    = std::max(result.totalWidth, line.width);
        totalHeight += line.baseline.height();
        preparedLines.push_back(std::move(line));
    }

    int y = rect.y();
    if(alignment & Qt::AlignBottom) {
        y = rect.bottom() - totalHeight + 1;
    }
    else if(alignment & Qt::AlignVCenter) {
        y += std::max(0, (rect.height() - totalHeight) / 2);
    }

    int boundX = rect.x();
    if(alignment & Qt::AlignRight) {
        boundX = rect.right() - result.totalWidth + 1;
    }
    else if(alignment & Qt::AlignHCenter) {
        boundX += std::max(0, (rect.width() - result.totalWidth) / 2);
    }
    result.bound = {boundX, y, result.totalWidth, totalHeight};

    for(const auto& line : preparedLines) {
        int x = rect.x();
        if(alignment & Qt::AlignRight) {
            x = rect.right() - line.width + 1;
        }
        else if(alignment & Qt::AlignHCenter) {
            x += std::max(0, (rect.width() - line.width) / 2);
        }

        for(const auto& block : line.blocks) {
            if(block.editor != InlineTrackEditor::None) {
                const QSize size = editors->size(block.editor);
                const QRect editorRect{x, y + ((line.baseline.height() - size.height()) / 2), block.width,
                                       size.height()};
                result.editors.push_back({.type = block.editor, .rect = editorRect.intersected(rect)});

                if(painter) {
                    if(block.editor == InlineTrackEditor::Love
                       && (!editors->hideEmptyLove || editors->love.loved() || editors->active)) {
                        editors->love.paint(painter, editorRect, option.palette, HeartValue::EditMode::ReadOnly,
                                            Qt::AlignCenter, selected);
                    }
                    else if(block.editor == InlineTrackEditor::Rating
                            && (!editors->hideEmptyRating || editors->rating.rating() >= 0.1F || editors->active)) {
                        painter->setFont(block.font);
                        editors->rating.paint(painter, editorRect, option.palette, StarRating::EditMode::ReadOnly,
                                              Qt::AlignCenter, selected);
                    }
                }
            }
            else if(block.playingIcon && painter) {
                const QRect iconRect{x, y + ((line.baseline.height() - block.width) / 2), block.width, block.width};
                Gui::drawItemViewIcon(painter, option, playingIcon, iconRect, Qt::AlignCenter);
            }
            else if(painter) {
                painter->setFont(block.font);
                painter->setPen(block.colour);
                painter->drawText(QPoint{x, y + line.baseline.ascent}, block.text);
            }

            x += block.width;
        }

        y += line.baseline.height();
    }

    if(painter) {
        painter->restore();
    }
    return result;
}
} // namespace Fooyin
