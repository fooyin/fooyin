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

#include <QAbstractItemView>

namespace Fooyin {
class AutoHeaderView;

class PlaylistView : public QAbstractItemView
{
    Q_OBJECT

public:
    explicit PlaylistView(QWidget* parent = nullptr);
    ~PlaylistView() override;

    void setModel(QAbstractItemModel* model) override;

    [[nodiscard]] AutoHeaderView* header() const;
    [[nodiscard]] bool isSpanning(int column) const;

    void setWaitForLoad(bool enabled);
    void setSpan(int column, bool span);

    [[nodiscard]] QRect visualRect(const QModelIndex& index) const override;
    void scrollTo(const QModelIndex& index, ScrollHint hint = EnsureVisible) override;
    [[nodiscard]] QModelIndex indexAt(const QPoint& point) const override;
    [[nodiscard]] QModelIndex indexAbove(const QModelIndex& index) const;
    [[nodiscard]] QModelIndex indexBelow(const QModelIndex& index) const;

    void doItemsLayout() override;
    void reset() override;
    void updateGeometries() override;

    void dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles = {}) override;
    void selectAll() override;

protected:
    bool viewportEvent(QEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;
    void rowsInserted(const QModelIndex& parent, int start, int end) override;
    void rowsRemoved(const QModelIndex& parent, int first, int last);
    void startDrag(Qt::DropActions supportedActions) override;

    QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) override;
    [[nodiscard]] int horizontalOffset() const override;
    [[nodiscard]] int verticalOffset() const override;
    [[nodiscard]] bool isIndexHidden(const QModelIndex& index) const override;
    void setSelection(const QRect& rect, QItemSelectionModel::SelectionFlags command) override;
    void currentChanged(const QModelIndex& current, const QModelIndex& previous) override;
    [[nodiscard]] QRegion visualRegionForSelection(const QItemSelection& selection) const override;

private:
    class Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
