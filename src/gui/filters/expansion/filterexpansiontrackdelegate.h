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

#include "scripting/inlinetrackeditors.h"

#include <QStyledItemDelegate>

namespace Fooyin {
class ExpansionTrackDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    void setHover(const QModelIndex& index, const InlineTrackEditorHit& hit, const QPoint& pos);
    void clearHover();
    void setHoveredRow(const QModelIndex& index);
    [[nodiscard]] InlineTrackEditorHit inlineEditorAt(const QStyleOptionViewItem& option, const QModelIndex& index,
                                                      const QPoint& pos) const;

    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    [[nodiscard]] InlineTrackEditors inlineEditors(const QStyleOptionViewItem& option, const QModelIndex& index) const;

    InlineTrackEditorHover m_hover;
    QPersistentModelIndex m_hoveredRow;
};
} // namespace Fooyin
