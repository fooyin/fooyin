/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include "fygui_export.h"

#include "filterfwd.h"
#include "filteritem.h"

#include <gui/widgets/expandedtreeview.h>

#include <QCache>
#include <QMargins>
#include <QStyledItemDelegate>

namespace Fooyin::Filters {
class FYGUI_EXPORT FilterDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit FilterDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    void setArtworkCornerRadius(int radius);
    void setArtworkMargins(const QMargins& margins);
    void setArtworkScaling(ArtworkScaling scaling);
    void setArtworkFrames(const ArtworkFrame& hover, const ArtworkFrame& selection);
    void setCaptionHighlighting(bool hover, bool selection);
    void setCoverFlowCacheSize(int sizeMb);

private:
    struct ArtworkFrameGeometry
    {
        QRectF rect;
        qreal radius{0.0};
    };

    [[nodiscard]] QRect artworkRect(const QStyleOptionViewItem& option) const;
    [[nodiscard]] QRect iconTextRect(const QStyleOptionViewItem& option) const;
    [[nodiscard]] int iconTextWidth(const QStyleOptionViewItem& option) const;

    [[nodiscard]] static IconCaptionLineList iconRichLines(const QModelIndex& index,
                                                           const QStyleOptionViewItem& option);
    [[nodiscard]] static RichText fallbackRichText(const QStyleOptionViewItem& option, const QModelIndex& index);
    [[nodiscard]] QSize richTextSize(const QStyleOptionViewItem& option, const QModelIndex& index) const;
    [[nodiscard]] QSize iconItemSize(const QStyleOptionViewItem& option, const QModelIndex& index) const;

    void initLayoutOnlyOption(QStyleOptionViewItem* option, const QModelIndex& index) const;
    static void setupFilterOption(QStyleOptionViewItem* option, const QModelIndex& index);

    [[nodiscard]] const ArtworkFrame* artworkFrame(const QStyleOptionViewItem& option) const;
    [[nodiscard]] ArtworkFrameGeometry artworkFrameGeometry(const QStyleOptionViewItem& option, const QRectF& artwork,
                                                            int cornerRadius) const;

    void drawArtworkFrame(QPainter* painter, const QStyleOptionViewItem& option,
                          const ArtworkFrameGeometry& geometry) const;
    static void drawArtworkLayer(QPainter* painter, const ArtworkFrameGeometry& geometry, const QPointF& position,
                                 const QPixmap& layer);
    void drawIconArtwork(QPainter* painter, const QStyleOptionViewItem& option, const QRect& rect,
                         const QPixmap& artwork) const;
    void drawOverlayItem(QPainter* painter, const QStyleOptionViewItem& option, const QPixmap& artwork,
                         const IconCaptionLineList& lines, QStyleOptionViewItem captionOption) const;
    void drawRichTextLines(QPainter* painter, const QStyleOptionViewItem& option, QRect rect,
                           const IconCaptionLineList& lines, const QRectF& artwork) const;
    static void drawTextBlocks(QPainter* painter, const QStyleOptionViewItem& option, QRect rect,
                               const std::vector<RichTextBlock>& blocks);

    void paintCoverFlow(QPainter* painter, const ExpandedTreeView::CoverFlowStyleOption& option,
                        const QModelIndex& index) const;
    void paintCoverFlowCaption(QPainter* painter, QStyleOptionViewItem option, const CoverFlowSettings& settings,
                               const QModelIndex& index) const;

    [[nodiscard]] QPixmap coverFlowPixmap(const QStyleOptionViewItem& option, const QModelIndex& index) const;
    void cacheCoverFlowPixmap(const QString& key, const QPixmap& pixmap) const;

    int m_artworkCornerRadius;
    ArtworkScaling m_artworkScaling;
    QMargins m_artworkMargins;
    ArtworkFrame m_artworkHoverFrame;
    ArtworkFrame m_artworkSelectionFrame;
    bool m_highlightHoveredCaptions;
    bool m_highlightSelectedCaptions;
    mutable QCache<QString, QPixmap> m_coverFlowPixmaps;
};
} // namespace Fooyin::Filters
