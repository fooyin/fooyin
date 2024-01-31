/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include <QTimer>
#include <QTreeView>

namespace Fooyin {
class PlaylistView : public QTreeView
{
    Q_OBJECT

public:
    explicit PlaylistView(QWidget* parent = nullptr);

    void setupView();

protected:
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const override;
    void drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void paintEvent(QPaintEvent* event) override;

    [[nodiscard]] QAbstractItemView::DropIndicatorPosition position(const QPoint& pos, const QRect& rect,
                                                                    const QModelIndex& index) const;
    [[nodiscard]] bool isIndexDropEnabled(const QModelIndex& index) const;

    [[nodiscard]] bool shouldAutoScroll(const QPoint& pos) const;
    void startAutoScroll();
    void stopAutoScroll();
    void doAutoScroll();

    bool dropOn(QDropEvent* event, int& dropRow, int& dropCol, QModelIndex& dropIndex);

private:
    QRect dropIndicatorRect;
    QAbstractItemView::DropIndicatorPosition dropIndicatorPos;
    QTimer m_autoScrollTimer;
    int m_autoScrollCount;
};
} // namespace Fooyin
