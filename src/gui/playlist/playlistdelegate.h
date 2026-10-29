/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <luket@pm.me>
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

namespace Fooyin {
class PlaylistDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    struct ReflectionOptions
    {
        bool enabled{false};
        int height{33};
        int opacity{35};
        int gap{3};
    };

    using QStyledItemDelegate::QStyledItemDelegate;

    [[nodiscard]] QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                        const QModelIndex& index) const override;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;

    void setArtworkCornerRadius(int radius);
    void setArtworkReflection(const ReflectionOptions& options);

    [[nodiscard]] int artworkSpanHeight(int width, const QModelIndex& index) const;

    static QRect subheaderContentRect(const QStyleOptionViewItem& option, const QModelIndex& index, int offset = 0);

private:
    int m_artworkCornerRadius{0};
    ReflectionOptions m_reflectionOptions;
};
} // namespace Fooyin
