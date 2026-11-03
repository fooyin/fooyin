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

#include <core/scripting/scriptparser.h>
#include <gui/scripting/scriptformatter.h>
#include <utils/hearteditor.h>
#include <utils/starrating.h>

#include <QIcon>
#include <QPersistentModelIndex>

class QPainter;
class QStyleOptionViewItem;

namespace Fooyin {
class SettingsManager;

constexpr auto InlineLoveEditorMarker   = u'\uE001';
constexpr auto InlineRatingEditorMarker = u'\uE002';

enum class InlineTrackEditor : uint8_t
{
    None = 0,
    Love,
    Rating,
};

struct InlineTrackEditorHit
{
    InlineTrackEditor type{InlineTrackEditor::None};
    QRect rect;
};

struct InlineTrackEditors
{
    HeartValue love;
    StarRating rating;
    bool hideEmptyLove{false};
    bool hideEmptyRating{false};
    bool active{false};

    [[nodiscard]] QSize size(InlineTrackEditor type) const;
    void setTrack(const Track& track);
};

struct InlineTrackEditorHover
{
    QPersistentModelIndex index;
    InlineTrackEditor type{InlineTrackEditor::None};
    bool loved{false};
    float rating{0.0F};

    void update(const QModelIndex& nextIndex, const InlineTrackEditorHit& hit, const QPoint& pos,
                const InlineTrackEditors& editors);
    void apply(InlineTrackEditors& editors) const;
    void clear();
};

struct DrawTrackTextResult
{
    QRect bound;
    int totalWidth{0};
    std::vector<InlineTrackEditorHit> editors;
};

InlineTrackEditors inlineTrackEditors(const SettingsManager& settings, const Track& track = {});
InlineTrackEditor inlineTrackEditorType(const RichTextBlock& block);
bool hasInlineTrackEditors(const RichText& text);
RichText evaluateInlineTrackScript(ScriptParser& parser, ScriptFormatter& formatter, const ParsedScript& script,
                                   const Track& track, const ScriptContext& context,
                                   const ScriptEvaluationOptions& options);
int inlineTrackEditorTextHeight(const RichText& text, int loveSize, int ratingSize);
QString inlineTrackEditorPlainText(const RichText& text);
InlineTrackEditorHit inlineTrackEditorAt(const std::vector<InlineTrackEditorHit>& editors, const QPoint& pos);
QVariant inlineTrackEditorValue(const InlineTrackEditorHit& hit, const QPoint& pos, const InlineTrackEditors& editors);
DrawTrackTextResult drawTrackTextBlocks(QPainter* painter, const QStyleOptionViewItem& option, QRect rect,
                                        const std::vector<RichTextBlock>& blocks, Qt::Alignment alignment,
                                        const QIcon& playingIcon = {}, const InlineTrackEditors* editors = nullptr);
} // namespace Fooyin

Q_DECLARE_METATYPE(Fooyin::InlineTrackEditors)
