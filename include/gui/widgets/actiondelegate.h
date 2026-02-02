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

#include "fygui_export.h"

#include <QStyledItemDelegate>

#include <vector>

namespace Fooyin {
struct ActionButton
{
    int id;
    QString text;
};

/*!
 * A delegate that paints right-aligned action buttons in item view rows.
 *
 * Subclasses override buttons() to return which buttons to show for each index.
 * Handles hover highlighting and click detection.
 *
 * Emits buttonClicked(index, buttonId) when a button is clicked.
 */
class FYGUI_EXPORT ActionDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit ActionDelegate(QAbstractItemView* view, QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

signals:
    void buttonClicked(const QModelIndex& index, int buttonId);

protected:
    [[nodiscard]] virtual std::vector<ActionButton> buttons(const QModelIndex& index) const = 0;

    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                     const QModelIndex& index) override;

private:
    QAbstractItemView* m_view;

    mutable QModelIndex m_hoveredIndex;
    mutable int m_hoveredButton{-1};
};
} // namespace Fooyin
