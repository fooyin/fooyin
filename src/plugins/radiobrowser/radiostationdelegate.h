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

#include <QMargins>
#include <QStyledItemDelegate>

namespace Fooyin::RadioBrowser {

class RadioStationDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    using QStyledItemDelegate::QStyledItemDelegate;

    [[nodiscard]] bool uniformStationIcons() const;
    void setUniformStationIcons(bool enabled);
    void setIconCornerRadius(int radius);
    void setIconMargins(const QMargins& margins);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    bool m_uniformStationIcons{true};
    int m_iconCornerRadius{20};
    QMargins m_iconMargins{8, 8, 8, 8};
};
} // namespace Fooyin::RadioBrowser
