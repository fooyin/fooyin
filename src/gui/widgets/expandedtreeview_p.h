/*
 * Fooyin
 * Copyright © 2025, Luke Taylor <LukeT1@proton.me>
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

#include <gui/widgets/expandedtreeview.h>

#include <QBasicTimer>

class QHeaderView;

namespace Fooyin {
class BaseView;

struct ExpandedTreeViewItem
{
    QModelIndex index;
    int parentItem{-1};
    int level{0};
    bool hasChildren{false};
    bool hasMoreSiblings{false};
    int childCount{0};
    int columnCount{0};
    int height{0};  // Row height
    int padding{0}; // Padding at bottom of row

    // Icon mode
    int x{-1};
    int y{-1};
    int width{-1};

    constexpr bool operator==(const ExpandedTreeViewItem& other) const
    {
        return (index != other.index);
    }

    constexpr bool operator!=(const ExpandedTreeViewItem& other) const
    {
        return !(*this == other);
    }

    [[nodiscard]] constexpr bool isValid() const
    {
        return rect().isValid();
    }

    [[nodiscard]] constexpr QRect rect() const
    {
        return {x, y, width, height};
    }
};

class ExpandedTreeViewPrivate : public QObject
{
    Q_OBJECT

public:
    explicit ExpandedTreeViewPrivate(ExpandedTreeView* self);
    ~ExpandedTreeViewPrivate() override;

    void setHeader(QHeaderView* header);

    int itemCount() const;
    QModelIndex modelIndex(int i, int column = 0) const;
    void select(const QModelIndex& topIndex, const QModelIndex& bottomIndex,
                QItemSelectionModel::SelectionFlags command) const;
    void resizeColumnToContents(int column) const;
    void columnCountChanged(int oldCount, int newCount) const;
    void columnResized(int logical, int oldSize, int newSize);

    void doDelayedItemsLayout(int delay = 0) const;
    void interruptDelayedItemsLayout() const;
    void layoutItems() const;

    ExpandedTreeViewItem indexToListViewItem(const QModelIndex& index) const;

    int viewIndex(const QModelIndex& index) const;
    void insertViewItems(int pos, int count, const ExpandedTreeViewItem& viewItem);
    bool hasVisibleChildren(const QModelIndex& parent) const;
    bool isIndexEnabled(const QModelIndex& index) const;
    bool isItemDisabled(int i) const;
    bool itemHasChildren(int i) const;
    void invalidateHeightCache(int item) const;
    int itemForHomeKey() const;
    int itemForEndKey() const;
    void setHoverIndex(const QPersistentModelIndex& index);

    bool isIndexDropEnabled(const QModelIndex& index) const;
    QModelIndexList selectedDraggableIndexes(bool fullRow = false) const;
    bool shouldAutoScroll() const;
    void startAutoScroll();
    void stopAutoScroll();
    void doAutoScroll();
    bool dropOn(QDropEvent* event, int& dropRow, int& dropCol, QModelIndex& dropIndex);
    std::vector<std::pair<int, int>> columnRanges(const QModelIndex& topIndex, const QModelIndex& bottomIndex) const;
    std::vector<QRect> rectsToPaint(const QModelIndex& index, const QStyleOptionViewItem& option, int y) const;

    QPoint offset() const;
    bool isIndexValid(const QModelIndex& index) const;

    ExpandedTreeView* m_self;

    QHeaderView* m_header;
    QAbstractItemModel* m_model{nullptr};
    std::unique_ptr<BaseView> m_view;

    using ViewMode       = ExpandedTreeView::ViewMode;
    using CaptionDisplay = ExpandedTreeView::CaptionDisplay;

    ViewMode m_viewMode{ViewMode::Tree};
    CaptionDisplay m_captionDisplay{CaptionDisplay::Bottom};
    bool m_uniformRowHeights{false};
    bool m_selectBeforeDrag{false};
    bool m_selectIgnoreParents{false};

    mutable bool m_delayedPendingLayout{false};
    bool m_updatingGeometry{false};
    int m_hidingScrollbar{0};
    bool m_layingOutItems{false};

    mutable std::vector<ExpandedTreeViewItem> m_viewItems;
    mutable int m_lastViewedItem{0};
    int m_defaultItemHeight{20};
    int m_indent{0};
    int m_uniformHeightRole{-1};
    std::unordered_map<int, int> m_uniformRoleHeights;
    std::set<int> m_spans;

    mutable QBasicTimer m_delayedLayout;
    QPoint m_pressedPos;
    QPersistentModelIndex m_hoverIndex;
    QPoint m_dragPos;
    QRect m_dropIndicatorRect;
    ExpandedTreeView::DropIndicatorPosition m_dropIndicatorPos{ExpandedTreeView::OnViewport};

    QBasicTimer m_autoScrollTimer;
    int m_autoScrollCount{0};
    QPoint m_scrollDelayOffset;

    int m_columnResizeTimerId{0};
};
} // namespace Fooyin
