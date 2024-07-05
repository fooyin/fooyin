/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "fyutils_export.h"

#include <QAbstractItemView>

#include <set>

class QHeaderView;

namespace Fooyin {
class FYUTILS_EXPORT ExpandedTreeView : public QAbstractItemView
{
    Q_OBJECT

public:
    enum class ViewMode : uint8_t
    {
        Tree = 0,
        Icon
    };
    Q_ENUM(ViewMode)

    enum class CaptionDisplay : uint8_t
    {
        None = 0,
        Bottom,
        Right
    };
    Q_ENUM(CaptionDisplay)

    explicit ExpandedTreeView(QWidget* parent = nullptr);
    ~ExpandedTreeView() override;

    [[nodiscard]] QHeaderView* header() const;
    void setHeader(QHeaderView* header);

    void setModel(QAbstractItemModel* model) override;

    [[nodiscard]] bool isSpanning(int column) const;
    void setSpan(int column, bool span);

    [[nodiscard]] ViewMode viewMode() const;
    void setViewMode(ViewMode mode);

    [[nodiscard]] CaptionDisplay captionDisplay() const;
    void setCaptionDisplay(CaptionDisplay display);

    [[nodiscard]] bool uniformRowHeights() const;
    void setUniformRowHeights(bool enabled);

    [[nodiscard]] int uniformHeightRole() const;
    void setUniformHeightRole(int role);

    [[nodiscard]] bool selectBeforeDrag() const;
    void setSelectBeforeDrag(bool enabled);

    void changeIconSize(const QSize& size);

    [[nodiscard]] QRect visualRect(const QModelIndex& index) const override;
    [[nodiscard]] int sizeHintForColumn(int column) const override;
    void scrollTo(const QModelIndex& index, ScrollHint hint) override;
    [[nodiscard]] QModelIndex indexAt(const QPoint& point) const override;

    [[nodiscard]] QModelIndex findIndexAt(const QPoint& point, bool includeSpans, bool includePadding = false) const;
    [[nodiscard]] QModelIndex indexAbove(const QModelIndex& index) const;
    [[nodiscard]] QModelIndex indexBelow(const QModelIndex& index) const;

    void doItemsLayout() override;
    void reset() override;
    void updateGeometries() override;

    void dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles) override;
    void selectAll() override;

signals:
    void viewModeChanged(ViewMode mode);
    void middleClicked(const QModelIndex& index);

protected:
    bool viewportEvent(QEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;
    void rowsInserted(const QModelIndex& parent, int start, int end) override;
    void rowsAboutToBeRemoved(const QModelIndex& parent, int start, int end) override;
    void rowsRemoved(const QModelIndex& parent, int first, int last);
    void startDrag(Qt::DropActions supportedActions) override;
    void verticalScrollbarValueChanged(int value) override;
    virtual DropIndicatorPosition dropPosition(const QPoint& pos, const QRect& rect, const QModelIndex& index);

    [[nodiscard]] int itemCount() const;
    [[nodiscard]] std::set<int> spans() const;

    void drawFocus(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index, int y) const;

    QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) override;
    [[nodiscard]] int horizontalOffset() const override;
    [[nodiscard]] int verticalOffset() const override;
    [[nodiscard]] bool isIndexHidden(const QModelIndex& index) const override;
    void setSelection(const QRect& rect, QItemSelectionModel::SelectionFlags command) override;
    void currentChanged(const QModelIndex& current, const QModelIndex& previous) override;
    [[nodiscard]] QRegion visualRegionForSelection(const QItemSelection& selection) const override;
    [[nodiscard]] QModelIndexList selectedIndexes() const override;

private:
    friend class BaseView;
    friend class TreeView;
    friend class IconView;

    class Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
