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

#pragma once

#include <QStyledItemDelegate>

#include <QFont>
#include <QRectF>
#include <QTextLayout>

#include <memory>
#include <vector>

namespace Fooyin {
class SettingsManager;

namespace Lyrics {
class LyricsDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    [[nodiscard]] int wordIndexAt(const QModelIndex& index, const QPoint& pos, const QRect& idxRect) const;

    void clearLayoutCache();

    struct TextRange
    {
        int start{0};
        int length{0};
    };

    struct LaidOutChunk
    {
        QRectF rect;
        int blockIndex{0};
        bool rightToLeft{false};
    };

    struct CachedLayout
    {
        int row{-1};
        QFont font;
        QColor baseColour;
        int width{0};
        std::unique_ptr<QTextLayout> textLayout;
        std::vector<TextRange> words;
        mutable std::vector<LaidOutChunk> chunks;
        int totalHeight{0};
        uint64_t lastUsed{0};
    };

private:
    [[nodiscard]] const CachedLayout& layoutForIndex(const QModelIndex& index, int width, const QFont& font,
                                                     const QColor& baseColour) const;

    mutable std::vector<CachedLayout> m_layoutCache;
    mutable uint64_t m_layoutCacheUseCounter{0};
};
} // namespace Lyrics
} // namespace Fooyin
