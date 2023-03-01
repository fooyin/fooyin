/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

namespace Fy::Gui::Widgets {
class ItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit ItemDelegate(QObject* parent = nullptr);

    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    static void paintHeader(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index);
    static void paintEntry(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index);
};
} // namespace Fy::Gui::Widgets
