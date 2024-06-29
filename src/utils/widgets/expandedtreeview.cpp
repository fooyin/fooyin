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

#include "utils/utils.h"
#include <utils/widgets/expandedtreeview.h>

#include <QDrag>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QScrollBar>
#include <QStack>
#include <QStylePainter>
#include <QTimer>
#include <QWheelEvent>

#include <set>

using namespace std::chrono_literals;

constexpr auto MinItemSpacing = 10;
constexpr auto IconRowSpacing = 10;

namespace {
void selectChildren(QAbstractItemModel* model, const QModelIndex& parentIndex, QItemSelection& selection)
{
    if(model->hasChildren(parentIndex)) {
        const int rowCount = model->rowCount(parentIndex);

        const QModelIndex firstChild = model->index(0, 0, parentIndex);
        const QModelIndex lastChild  = model->index(rowCount - 1, 0, parentIndex);
        selection.append({firstChild, lastChild});

        for(int row{0}; row < rowCount; ++row) {
            selectChildren(model, model->index(row, 0, parentIndex), selection);
        }
    }
}

QItemSelection selectRecursively(QAbstractItemModel* model, const QItemSelection& selection)
{
    QItemSelection newSelection;
    newSelection.reserve(selection.size());

    for(const auto& range : selection) {
        for(int row = range.top(); row <= range.bottom(); ++row) {
            const QModelIndex index = model->index(row, 0, range.parent());
            if(model->hasChildren(index)) {
                selectChildren(model, index, newSelection);
            }
        }
        newSelection.append(range);
    }

    return newSelection;
}
} // namespace

namespace Fooyin {
struct ExpandedTreeViewItem
{
    QModelIndex index;
    int parentItem{-1};
    bool hasChildren{false};
    bool hasMoreSiblings{false};
    int childCount{0};
    int columnCount{0};
    int height{0};  // Row height
    int padding{0}; // Padding at bottom of row

    // Icon mode
    int baseHeight{-1};    // Height of first item row (including icon)
    int captionHeight{-1}; // Height of each additional row below
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

    [[nodiscard]] constexpr QRect baseRect() const
    {
        return {x, y, width, baseHeight};
    }

    [[nodiscard]] constexpr QRect columnRect(int column) const
    {
        return {x, y + baseHeight + (captionHeight * (column - 1)), width, captionHeight};
    }
};

struct ItemViewPaintPair
{
    QRect rect;
    QModelIndex index;
};
using ItemViewPaintPairs = std::vector<ItemViewPaintPair>;

enum class RectRule
{
    FullRow,
    SingleSection
};

class BaseView;

class ExpandedTreeView::Private : public QObject
{
    Q_OBJECT

public:
    explicit Private(ExpandedTreeView* self);

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

    std::vector<QRect> rectsToPaint(const QStyleOptionViewItem& option, int y) const;

    bool isIndexValid(const QModelIndex& index) const;

    ExpandedTreeView* m_self;

    QHeaderView* m_header;
    QAbstractItemModel* m_model{nullptr};
    std::unique_ptr<BaseView> m_view;

    ViewMode m_viewMode{ViewMode::Tree};
    CaptionDisplay m_captionDisplay{CaptionDisplay::Bottom};
    bool m_uniformRowHeights{false};

    mutable bool m_delayedPendingLayout{false};
    bool m_updatingGeometry{false};
    int m_hidingScrollbar{0};
    bool m_layingOutItems{false};

    mutable std::vector<ExpandedTreeViewItem> m_viewItems;
    mutable int m_lastViewedItem{0};
    int m_defaultItemHeight{20};

    std::set<int> m_spans;

    mutable QBasicTimer m_delayedLayout;

    QPersistentModelIndex m_hoverIndex;
    QPoint m_dragPos;
    QRect m_dropIndicatorRect;
    DropIndicatorPosition m_dropIndicatorPos{OnViewport};

    QBasicTimer m_autoScrollTimer;
    int m_autoScrollCount{0};
    QPoint m_scrollDelayOffset;

    int m_columnResizeTimerId{0};
};

class BaseView
{
public:
    explicit BaseView(ExpandedTreeView* view, ExpandedTreeView::Private* viewP)
        : m_view{view}
        , m_p{viewP}
    { }

    virtual ~BaseView() = default;

    virtual void invalidate() = 0;

    virtual void doItemLayout()                                           = 0;
    virtual void drawView(QPainter* painter, const QRegion& region) const = 0;

    [[nodiscard]] virtual QRect visualRect(const QModelIndex& index, RectRule rule, bool includePadding) const = 0;
    [[nodiscard]] virtual ExpandedTreeViewItem indexToViewItem(const QModelIndex& index) const                 = 0;
    [[nodiscard]] virtual QModelIndex findIndexAt(const QPoint& point, bool incSpans, bool incPadding) const   = 0;
    virtual void dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)                       = 0;
    [[nodiscard]] virtual int sizeHintForColumn(int column) const                                              = 0;

    [[nodiscard]] virtual int firstVisibleItem(int* offset) const                            = 0;
    [[nodiscard]] virtual int lastVisibleItem(int firstVisual, int offset) const             = 0;
    [[nodiscard]] virtual QPoint coordinateForItem(int item) const                           = 0;
    [[nodiscard]] virtual int itemAtCoordinate(QPoint coordinate, bool includePadding) const = 0;
    [[nodiscard]] virtual int itemAbove(int item) const                                      = 0;
    [[nodiscard]] virtual int itemBelow(int item) const                                      = 0;
    [[nodiscard]] virtual int itemLeft(int item) const                                       = 0;
    [[nodiscard]] virtual int itemRight(int item) const                                      = 0;
    [[nodiscard]] virtual int pageUp(int item) const                                         = 0;
    [[nodiscard]] virtual int pageDown(int item) const                                       = 0;

    virtual void renderToPixmap(QPainter* painter, const ItemViewPaintPairs& paintPairs, QRect& rect) const = 0;

    virtual void updateScrollBars() = 0;
    virtual void updateColumns() { }

    void layout(int i, bool afterIsUninitialised = false);
    [[nodiscard]] QRect mapToViewport(const QRect& rect) const;
    [[nodiscard]] int indexWidthHint(const QModelIndex& index, int hint, const QStyleOptionViewItem& option) const;
    [[nodiscard]] std::pair<int, int> startAndEndColumns(const QRect& rect) const;
    void calcLogicalIndexes(std::vector<int>& logicalIndexes,
                            std::vector<QStyleOptionViewItem::ViewItemPosition>& itemPositions, int left,
                            int right) const;
    [[nodiscard]] ItemViewPaintPairs draggablePaintPairs(const QModelIndexList& indexes, QRect& rect) const;

    [[nodiscard]] bool isIndexValid(const QModelIndex& index) const
    {
        return m_p->isIndexValid(index);
    }

    [[nodiscard]] QScrollBar* verticalScrollBar() const
    {
        return m_view->verticalScrollBar();
    }

    [[nodiscard]] QScrollBar* horizontalScrollBar() const
    {
        return m_view->horizontalScrollBar();
    }

    [[nodiscard]] Qt::ScrollBarPolicy verticalScrollBarPolicy() const
    {
        return m_view->verticalScrollBarPolicy();
    }

    [[nodiscard]] Qt::ScrollBarPolicy horizontalScrollBarPolicy() const
    {
        return m_view->horizontalScrollBarPolicy();
    }

    [[nodiscard]] int itemCount() const
    {
        return m_p->itemCount();
    }

    [[nodiscard]] std::vector<ExpandedTreeViewItem>& viewItems() const
    {
        return m_p->m_viewItems;
    }

    [[nodiscard]] ExpandedTreeViewItem& viewItem(int i) const
    {
        return m_p->m_viewItems.at(i);
    }

    [[nodiscard]] bool empty() const
    {
        return m_p->m_viewItems.empty();
    }

    [[nodiscard]] QWidget* viewport() const
    {
        return m_view->viewport();
    }

    [[nodiscard]] QSize viewportSize() const
    {
        return m_view->contentsRect().size();
    }

    [[nodiscard]] QAbstractItemModel* model() const
    {
        return m_p->m_model;
    }

    [[nodiscard]] QHeaderView* header() const
    {
        return m_p->m_header;
    }

    [[nodiscard]] QAbstractItemDelegate* delegate(const QModelIndex& idx) const
    {
        return m_view->itemDelegateForIndex(idx);
    }

    ExpandedTreeView* m_view;
    ExpandedTreeView::Private* m_p;

    mutable std::pair<int, int> m_leftAndRight;
    QSize m_contentsSize;
    mutable int m_uniformRowHeight{0};
};

void BaseView::layout(int i, bool afterIsUninitialised)
{
    const QModelIndex parent = i < 0 ? m_view->rootIndex() : m_p->modelIndex(i);

    if(i >= 0 && !parent.isValid()) {
        return;
    }

    int count{0};
    if(model()->hasChildren(parent)) {
        if(model()->canFetchMore(parent)) {
            model()->fetchMore(parent);
            const int itemHeight = m_p->m_defaultItemHeight <= 0 ? m_view->sizeHintForRow(0) : m_p->m_defaultItemHeight;
            const int viewCount  = itemHeight ? m_view->viewport()->height() / itemHeight : 0;

            int lastCount{-1};
            while((count = model()->rowCount(parent)) < viewCount && count != lastCount
                  && model()->canFetchMore(parent)) {
                model()->fetchMore(parent);
                lastCount = count;
            }
        }
        else {
            count = model()->rowCount(parent);
        }
    }

    if(i == -1) {
        m_p->m_viewItems.resize(count);
        afterIsUninitialised = true;
    }
    else if(viewItem(i).childCount != count) {
        if(!afterIsUninitialised) {
            m_p->insertViewItems(i + 1, count, ExpandedTreeViewItem{});
        }
        else if(count > 0) {
            m_p->m_viewItems.resize(itemCount() + count);
        }
    }

    const int first{i + 1};
    int children{0};
    ExpandedTreeViewItem* item{nullptr};
    QModelIndex currentIndex;

    int columnCount{0};
    const int total = model()->columnCount();
    for(int column{0}; column < total; ++column) {
        if(!header()->isSectionHidden(column)) {
            ++columnCount;
        }
    }

    for(int index{first}; index < first + count; ++index) {
        currentIndex = model()->index(index - first, 0, parent);

        const int last = index + children;

        if(item) {
            item->hasMoreSiblings = true;
        }

        item                  = &viewItem(last);
        item->index           = currentIndex;
        item->parentItem      = i;
        item->height          = 0;
        item->childCount      = 0;
        item->hasMoreSiblings = false;
        item->hasChildren     = model()->hasChildren(currentIndex);
        item->columnCount     = columnCount;

        if(item->hasChildren) {
            layout(last, afterIsUninitialised);
            item = &viewItem(last);
            children += item->childCount;
        }
    }

    while(i > -1) {
        viewItem(i).childCount += count;
        i = viewItem(i).parentItem;
    }
}

QRect BaseView::mapToViewport(const QRect& rect) const
{
    if(!rect.isValid()) {
        return rect;
    }

    const QRect result{rect};
    const int dx = -m_view->horizontalOffset();
    const int dy = -m_view->verticalOffset();

    return result.adjusted(dx, dy, dx, dy);
}

int BaseView::indexWidthHint(const QModelIndex& index, int hint, const QStyleOptionViewItem& option) const
{
    const int xHint = delegate(index)->sizeHint(option, index).width();
    return std::max(hint, xHint);
}

std::pair<int, int> BaseView::startAndEndColumns(const QRect& rect) const
{
    const int start = std::min(header()->visualIndexAt(rect.left()), 0);
    int end         = header()->visualIndexAt(rect.right());

    if(end == -1) {
        end = header()->count() - 1;
    }

    return {std::min(start, end), std::max(start, end)};
}

void BaseView::calcLogicalIndexes(std::vector<int>& logicalIndexes,
                                  std::vector<QStyleOptionViewItem::ViewItemPosition>& itemPositions, int left,
                                  int right) const
{
    const int columnCount = header()->count();

    int logicalIndexBeforeLeft{-1};
    int logicalIndexAfterRight{-1};

    for(int visualIndex{left - 1}; visualIndex >= 0; --visualIndex) {
        const int logical = header()->logicalIndex(visualIndex);
        if(!header()->isSectionHidden(logical)) {
            logicalIndexBeforeLeft = logical;
            break;
        }
    }

    for(int visualIndex{left}; visualIndex < columnCount; ++visualIndex) {
        const int logical = header()->logicalIndex(visualIndex);
        if(!header()->isSectionHidden(logical)) {
            if(visualIndex > right) {
                logicalIndexAfterRight = logical;
                break;
            }
            logicalIndexes.emplace_back(logical);
        }
    }

    const auto count = static_cast<int>(logicalIndexes.size());
    itemPositions.resize(count);

    for(int visual{0}; visual < count; ++visual) {
        const int logical     = logicalIndexes.at(visual);
        const int nextLogical = (visual + 1) >= count ? logicalIndexAfterRight : logicalIndexes.at(visual + 1);
        const int prevLogical = (visual - 1) < 0 ? logicalIndexBeforeLeft : logicalIndexes.at(visual - 1);

        QStyleOptionViewItem::ViewItemPosition pos;

        if(columnCount == 1 || (nextLogical == 0 && prevLogical == -1) || (logical == 0 && nextLogical == -1)
           || m_view->isSpanning(logical)) {
            pos = QStyleOptionViewItem::OnlyOne;
        }
        else if(nextLogical != 0 && prevLogical == -1) {
            pos = QStyleOptionViewItem::Beginning;
        }
        else if(nextLogical == 0 || nextLogical == -1) {
            pos = QStyleOptionViewItem::End;
        }
        else {
            pos = QStyleOptionViewItem::Middle;
        }

        itemPositions[visual] = pos;
    }
}

ItemViewPaintPairs BaseView::draggablePaintPairs(const QModelIndexList& indexes, QRect& rect) const
{
    ItemViewPaintPairs ret;

    const QRect viewportRect = viewport()->rect();

    for(const auto& index : indexes) {
        if(!index.isValid() || m_view->isSpanning(index.column())) {
            continue;
        }

        const QRect currentRect = m_view->visualRect(index);
        if(currentRect.intersects(viewportRect)) {
            ret.emplace_back(currentRect, index);
            rect |= currentRect;
        }
    }

    const QRect clipped = rect & viewportRect;
    rect.setLeft(clipped.left());
    rect.setRight(clipped.right());

    return ret;
}

class TreeView : public BaseView
{
public:
    using BaseView::BaseView;

    void invalidate() override;

    void doItemLayout() override;
    void drawView(QPainter* painter, const QRegion& region) const override;

    [[nodiscard]] QRect visualRect(const QModelIndex& index, RectRule rule, bool includePadding) const override;
    [[nodiscard]] ExpandedTreeViewItem indexToViewItem(const QModelIndex& index) const override;
    [[nodiscard]] QModelIndex findIndexAt(const QPoint& point, bool includeSpans, bool includePadding) const override;
    void dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight) override;
    [[nodiscard]] int sizeHintForColumn(int column) const override;

    [[nodiscard]] int firstVisibleItem(int* offset) const override;
    [[nodiscard]] int lastVisibleItem(int firstVisual, int offset) const override;
    [[nodiscard]] QPoint coordinateForItem(int item) const override;
    [[nodiscard]] int itemAtCoordinate(QPoint coordinate, bool includePadding) const override;
    [[nodiscard]] int itemAbove(int item) const override;
    [[nodiscard]] int itemBelow(int item) const override;
    [[nodiscard]] int itemLeft(int item) const override;
    [[nodiscard]] int itemRight(int item) const override;
    [[nodiscard]] int pageUp(int item) const override;
    [[nodiscard]] int pageDown(int item) const override;

    void renderToPixmap(QPainter* painter, const ItemViewPaintPairs& paintPairs, QRect& rect) const override;

    void updateScrollBars() override;
    void updateColumns() override;

    [[nodiscard]] int itemHeight(int item) const;
    [[nodiscard]] int itemPadding(int item) const;

    void drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
    void drawRowBackground(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index,
                           int y) const;

private:
    [[nodiscard]] int indexRowSizeHint(const QModelIndex& index) const;
    [[nodiscard]] int indexSizeHint(const QModelIndex& index, bool span = false) const;
    void recalculatePadding();
    void drawAndClipSpans(QPainter* painter, const QStyleOptionViewItem& option, int firstVisibleItem,
                          int firstVisibleItemOffset) const;
    void adjustViewOptionsForIndex(QStyleOptionViewItem* option, const QModelIndex& currentIndex) const;
};

void TreeView::invalidate()
{
    m_uniformRowHeight = 0;
}

void TreeView::drawView(QPainter* painter, const QRegion& region) const
{
    QStyleOptionViewItem opt;
    m_view->initViewItemOption(&opt);

    if(empty() || header()->count() == 0) {
        return;
    }

    int firstVisibleOffset{0};
    const int firstVisible = firstVisibleItem(&firstVisibleOffset);
    if(firstVisible < 0) {
        return;
    }

    if(!m_p->m_spans.empty()) {
        drawAndClipSpans(painter, opt, firstVisible, firstVisibleOffset);
    }

    const int count          = itemCount();
    const int viewportWidth  = viewport()->width();
    const bool multipleRects = (region.rectCount() > 1);
    std::set<int> drawn;

    for(const QRect& regionArea : region) {
        const QRect area = multipleRects ? QRect{0, regionArea.y(), viewportWidth, regionArea.height()} : regionArea;
        m_leftAndRight   = startAndEndColumns(area);

        int i{firstVisible};
        int y{firstVisibleOffset};

        for(; i < count; ++i) {
            const int height = itemHeight(i) + itemPadding(i);
            if(y + height > area.top()) {
                break;
            }
            y += height;
        }

        for(; i < count && y <= area.bottom(); ++i) {
            const auto& item         = viewItem(i);
            const QModelIndex& index = item.index;

            opt.rect = visualRect(index, RectRule::FullRow, false);
            opt.rect.setY(y);
            opt.rect.setHeight(indexRowSizeHint(index));
            opt.state |= QStyle::State_Open | (item.hasMoreSiblings ? QStyle::State_Sibling : QStyle::State_None);

            if(item.hasChildren) {
                opt.state |= QStyle::State_Children;
            }
            else {
                opt.state &= ~QStyle::State_Children;
            }

            if(!multipleRects || !drawn.contains(i)) {
                drawRow(painter, opt, item.index);

                if(multipleRects) {
                    drawn.emplace(i);
                }
            }

            y += itemHeight(i) + item.padding;
        }
    }
}

void TreeView::doItemLayout()
{
    invalidate();
    layout(-1);
    recalculatePadding();
}

QRect TreeView::visualRect(const QModelIndex& index, RectRule rule, bool includePadding) const
{
    if(!isIndexValid(index)) {
        return {};
    }

    if(m_view->isIndexHidden(index) && rule != RectRule::FullRow) {
        return {};
    }

    m_p->layoutItems();

    const int viewIndex = m_p->viewIndex(index);
    if(viewIndex < 0) {
        return {};
    }

    const int column = index.column();
    int x            = header()->sectionViewportPosition(column);
    int width        = header()->sectionSize(column);

    if(rule == RectRule::FullRow) {
        x     = 0;
        width = header()->length();
    }

    const int y = coordinateForItem(viewIndex).y();
    int height  = indexSizeHint(index);

    if(includePadding) {
        height += itemPadding(viewIndex);
    }

    return {x, y, width, height};
}

ExpandedTreeViewItem TreeView::indexToViewItem(const QModelIndex& index) const
{
    if(index.isValid() && index.row() < itemCount()) {
        return viewItem(index.row());
    }

    return {};
}

QModelIndex TreeView::findIndexAt(const QPoint& point, bool includeSpans, bool includePadding) const
{
    const int visualIndex = itemAtCoordinate(point, includePadding);
    QModelIndex index     = m_p->modelIndex(visualIndex);
    if(!index.isValid()) {
        return {};
    }

    const int column = header()->logicalIndexAt(point.x());

    if(!includeSpans && !model()->hasChildren(index) && m_view->isSpanning(column)) {
        return {};
    }

    if(column == index.column()) {
        return index;
    }

    if(column < 0) {
        return {};
    }

    return index.sibling(index.row(), column);
}

void TreeView::dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
    bool sizeChanged{false};
    const int topViewIndex = m_p->viewIndex(topLeft);

    if(topViewIndex == 0) {
        const int defaultHeight  = indexRowSizeHint(topLeft);
        sizeChanged              = m_p->m_defaultItemHeight != defaultHeight;
        m_p->m_defaultItemHeight = defaultHeight;
    }

    if(topViewIndex != -1) {
        // Single row
        if(topLeft.row() == bottomRight.row()) {
            const int oldHeight = itemHeight(topViewIndex);
            m_p->invalidateHeightCache(topViewIndex);
            sizeChanged |= (oldHeight != itemHeight(topViewIndex));
            if(topLeft.column() == 0) {
                viewItem(topViewIndex).hasChildren = m_p->hasVisibleChildren(topLeft);
            }
        }
        else {
            const int bottomViewIndex = m_p->viewIndex(bottomRight);
            for(int i = topViewIndex; i <= bottomViewIndex; ++i) {
                const int oldHeight = itemHeight(i);
                m_p->invalidateHeightCache(i);
                sizeChanged |= (oldHeight != itemHeight(i));
                if(topLeft.column() == 0) {
                    viewItem(i).hasChildren = m_p->hasVisibleChildren(viewItem(i).index);
                }
            }
        }
    }

    if(sizeChanged) {
        updateScrollBars();
        viewport()->update();
    }
}

int TreeView::sizeHintForColumn(int column) const
{
    QStyleOptionViewItem option;
    m_view->initViewItemOption(&option);

    const int count = itemCount();

    const int maximumProcessRows = header()->resizeContentsPrecision();

    int offset{0};
    int start = firstVisibleItem(&offset);
    int end   = lastVisibleItem(start, offset);
    if(start < 0 || end < 0 || end == count - 1) {
        end = count - 1;
        if(maximumProcessRows < 0) {
            start = 0;
        }
        else if(maximumProcessRows == 0) {
            start               = std::max(0, end - 1);
            int remainingHeight = viewport()->height();
            while(start > 0 && remainingHeight > 0) {
                remainingHeight -= itemHeight(start);
                --start;
            }
        }
        else {
            start = std::max(0, end - maximumProcessRows);
        }
    }

    int width{0};
    int rowsProcessed{0};

    for(int i = start; i <= end; ++i) {
        if(viewItem(i).hasChildren) {
            continue;
        }
        QModelIndex index = viewItem(i).index;
        index             = index.sibling(index.row(), column);
        width             = indexWidthHint(index, width, option);
        ++rowsProcessed;
        if(rowsProcessed == maximumProcessRows) {
            break;
        }
    }

    --end;

    const int actualBottom = count - 1;

    if(maximumProcessRows == 0) {
        rowsProcessed = 0;
    }

    while(rowsProcessed != maximumProcessRows && (start > 0 || end < actualBottom)) {
        int index{-1};

        if((rowsProcessed % 2 && start > 0) || end == actualBottom) {
            while(start > 0) {
                --start;
                if(viewItem(start).hasChildren) {
                    continue;
                }
                index = start;
                break;
            }
        }
        else {
            while(end < actualBottom) {
                ++end;
                if(viewItem(end).hasChildren) {
                    continue;
                }
                index = end;
                break;
            }
        }
        if(index < 0) {
            continue;
        }

        QModelIndex viewIndex = viewItem(index).index;
        viewIndex             = viewIndex.sibling(viewIndex.row(), column);
        width                 = indexWidthHint(viewIndex, width, option);
        ++rowsProcessed;
    }

    return width;
}

int TreeView::firstVisibleItem(int* offset) const
{
    const int value = verticalScrollBar()->value();
    const int count = itemCount();

    int y{0};
    for(int i{0}; i < count; ++i) {
        const int height = itemHeight(i) + itemPadding(i);
        y += height;
        if(y > value) {
            if(offset) {
                *offset = y - value - height;
            }
            return i;
        }
    }
    return -1;
}

int TreeView::lastVisibleItem(int firstVisual, int offset) const
{
    if(firstVisual < 0 || offset < 0) {
        firstVisual = firstVisibleItem(&offset);
        if(firstVisual < 0) {
            return -1;
        }
    }

    int y{-offset};
    const int value = viewport()->height();

    const int count = itemCount();
    for(int i{firstVisual}; i < count; ++i) {
        y += itemHeight(i) + itemPadding(i);
        if(y > value) {
            return i;
        }
    }

    return count - 1;
}

QPoint TreeView::coordinateForItem(int item) const
{
    const int vertScrollValue = verticalScrollBar()->value();

    const auto& items = viewItems();
    for(int index{0}, y{0}; const auto& viewItem : items) {
        if(index == item) {
            return {0, y - vertScrollValue};
        }
        y += itemHeight(index++) + viewItem.padding;
    }

    return {0, 0};
}

int TreeView::itemAtCoordinate(QPoint coordinate, bool includePadding) const
{
    const int count = itemCount();
    if(count == 0) {
        return -1;
    }

    const int contentsCoord = coordinate.y() + verticalScrollBar()->value();

    int itemCoord{0};
    for(int index{0}; index < count; ++index) {
        const int height  = itemHeight(index);
        const int padding = itemPadding(index);
        itemCoord += height + padding;

        if(itemCoord > contentsCoord) {
            if(includePadding && (itemCoord - padding) < contentsCoord) {
                return -1;
            }
            return index >= count ? -1 : index;
        }
    }

    return -1;
}

int TreeView::itemAbove(int item) const
{
    const int i{item};
    while(m_p->isItemDisabled(--item) || m_p->itemHasChildren(item)) { }
    return item < 0 ? i : item;
}

int TreeView::itemBelow(int item) const
{
    const int i{item};
    while(m_p->isItemDisabled(++item) || m_p->itemHasChildren(item)) { }
    return item >= itemCount() ? i : item;
}

int TreeView::itemLeft(int /*item*/) const
{
    return -1;
}

int TreeView::itemRight(int /*item*/) const
{
    return -1;
}

int TreeView::pageUp(int item) const
{
    auto coord = coordinateForItem(item);
    coord.ry() -= viewport()->height();

    int index = itemAtCoordinate(coord, false);

    while(m_p->isItemDisabled(index) || m_p->itemHasChildren(index)) {
        index--;
    }

    if(index == -1) {
        index = 0;
    }

    while(m_p->isItemDisabled(index) || m_p->itemHasChildren(index)) {
        index++;
    }

    return index >= itemCount() ? 0 : index;
}

int TreeView::pageDown(int item) const
{
    auto coord = coordinateForItem(item);
    coord.ry() += viewport()->height();

    int index = itemAtCoordinate(coord, false);

    while(m_p->isItemDisabled(index) || m_p->itemHasChildren(index)) {
        index++;
    }

    if(index == -1 || index >= itemCount()) {
        index = itemCount() - 1;
    }

    while(m_p->isItemDisabled(index) || m_p->itemHasChildren(index)) {
        index--;
    }

    return index == -1 ? itemCount() - 1 : index;
}

void TreeView::renderToPixmap(QPainter* painter, const ItemViewPaintPairs& paintPairs, QRect& rect) const
{
    QStyleOptionViewItem opt;
    m_view->initViewItemOption(&opt);

    opt.state |= QStyle::State_Selected;

    int lastRow{0};
    bool paintedBg{false};
    for(const auto& [paintRect, index] : paintPairs) {
        if(m_view->isSpanning(index.column())) {
            continue;
        }

        if(std::exchange(lastRow, index.row()) != lastRow) {
            paintedBg = false;
        }

        opt.rect = paintRect.translated(-rect.topLeft());
        adjustViewOptionsForIndex(&opt, index);

        const int cellHeight = indexSizeHint(index);
        opt.rect.setHeight(cellHeight);

        if(!paintedBg) {
            const QRect cellRect{opt.rect};
            paintedBg = true;

            opt.rect.setRect(0, opt.rect.top(), header()->length(), cellHeight);

            const auto bg = index.data(Qt::BackgroundRole).value<QBrush>();
            if(bg != Qt::NoBrush) {
                painter->fillRect(opt.rect, bg);
            }
            m_view->style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, m_view);

            opt.rect = cellRect;
        }

        delegate(index)->paint(painter, opt, index);
    }
}

void TreeView::updateScrollBars()
{
    QSize viewportSize = m_view->viewport()->size();
    if(!viewportSize.isValid()) {
        viewportSize = {0, 0};
    }

    m_p->layoutItems();

    if(m_p->m_viewItems.empty()) {
        m_view->doItemsLayout();
    }

    int itemsInViewport{0};
    const int count          = itemCount();
    const int viewportHeight = viewportSize.height();

    for(int height{0}, item = count - 1; item >= 0; --item) {
        height += itemHeight(item);
        if(height > viewportHeight) {
            break;
        }
        ++itemsInViewport;
    }

    auto* verticalBar = verticalScrollBar();

    int contentsHeight{0};
    for(int i{0}; i < count; ++i) {
        contentsHeight += itemHeight(i) + itemPadding(i);
    }

    const int vMax = contentsHeight - viewportHeight;
    if(verticalBar->isVisible() && vMax <= 0) {
        m_p->m_hidingScrollbar = 2;
    }

    verticalBar->setRange(0, vMax);
    verticalBar->setPageStep(viewportHeight);
    verticalBar->setSingleStep(std::max(viewportHeight / (itemsInViewport + 1), 2));

    const int columnCount   = header()->count();
    const int viewportWidth = viewportSize.width();
    int columnsInViewport{0};

    for(int width{0}, column = columnCount - 1; column >= 0; --column) {
        const int logical = header()->logicalIndex(column);
        width += header()->sectionSize(logical);
        if(width > viewportWidth) {
            break;
        }
        ++columnsInViewport;
    }

    if(columnCount > 0) {
        columnsInViewport = std::max(1, columnsInViewport);
    }

    auto* horizontalBar = horizontalScrollBar();

    const int horizontalLength = header()->length();
    const QSize maxSize        = m_view->maximumViewportSize();

    if(maxSize.width() >= horizontalLength && verticalBar->maximum() <= 0) {
        viewportSize = maxSize;
    }

    horizontalBar->setPageStep(viewportSize.width());
    horizontalBar->setRange(0, qMax(horizontalLength - viewportSize.width(), 0));
    horizontalBar->setSingleStep(std::max(viewportSize.width() / (columnsInViewport + 1), 2));
}

void TreeView::updateColumns()
{
    recalculatePadding();
}

int TreeView::itemHeight(int item) const
{
    if(empty()) {
        return 0;
    }

    if(m_p->m_uniformRowHeights && m_uniformRowHeight > 0) {
        return m_uniformRowHeight;
    }

    const QModelIndex& index = viewItem(item).index;
    if(!index.isValid()) {
        return 0;
    }

    int height = viewItem(item).height;
    if(height <= 0) {
        height                = indexRowSizeHint(index);
        viewItem(item).height = height;
    }

    height = std::max(height, 0);

    if(m_p->m_uniformRowHeights) {
        m_uniformRowHeight = height;
    }

    return height;
}

int TreeView::itemPadding(int item) const
{
    if(empty()) {
        return 0;
    }

    return viewItem(item).padding;
}

void TreeView::drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if(!index.isValid()) {
        return;
    }

    QStyleOptionViewItem opt{option};

    const QModelIndex hover = m_p->m_hoverIndex;
    const bool hoverRow     = index.parent() == hover.parent() && index.row() == hover.row();
    const bool rowFocused   = m_view->hasFocus();

    opt.state.setFlag(QStyle::State_MouseOver, hoverRow);

    if(m_view->alternatingRowColors()) {
        opt.features.setFlag(QStyleOptionViewItem::Alternate, index.row() & 1);
    }

    if(model()->hasChildren(index)) {
        // Span first column of headers/subheaders
        opt.rect.setX(0);
        opt.rect.setWidth(header()->length());
        m_view->style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, m_view);
        delegate(index)->paint(painter, opt, index);
        return;
    }

    const QPoint offset       = m_p->m_scrollDelayOffset;
    const int y               = opt.rect.y() + offset.y();
    const int left            = m_leftAndRight.first;
    const int right           = m_leftAndRight.second;
    const QModelIndex parent  = index.parent();
    const QModelIndex current = m_view->currentIndex();

    QModelIndex modelIndex;
    std::vector<int> logicalIndices;
    std::vector<QStyleOptionViewItem::ViewItemPosition> viewItemPosList;

    calcLogicalIndexes(logicalIndices, viewItemPosList, left, right);

    const auto count = static_cast<int>(logicalIndices.size());
    bool paintedBg{false};
    bool currentRowHasFocus{false};

    for(int section{0}; section < count; ++section) {
        const int headerSection = logicalIndices.at(section);
        const bool spanning     = m_view->isSpanning(headerSection);

        if(spanning) {
            continue;
        }

        const int width    = header()->sectionSize(headerSection);
        const int position = header()->sectionViewportPosition(headerSection) + offset.x();

        modelIndex = model()->index(index.row(), headerSection, parent);

        if(!modelIndex.isValid()) {
            continue;
        }

        const auto icon      = index.data(Qt::DecorationRole).value<QPixmap>();
        opt.icon             = QIcon{icon};
        opt.decorationSize   = icon.deviceIndependentSize().toSize();
        opt.viewItemPosition = viewItemPosList.at(section);

        if(m_view->selectionModel()->isSelected(modelIndex)) {
            opt.state |= QStyle::State_Selected;
        }
        if(rowFocused && (current == modelIndex)) {
            currentRowHasFocus = true;
        }

        if(opt.state & QStyle::State_Enabled) {
            QPalette::ColorGroup cg;
            if((model()->flags(modelIndex) & Qt::ItemIsEnabled) == 0) {
                opt.state &= ~QStyle::State_Enabled;
                cg = QPalette::Disabled;
            }
            else if(opt.state & QStyle::State_Active) {
                cg = QPalette::Active;
            }
            else {
                cg = QPalette::Inactive;
            }
            opt.palette.setCurrentColorGroup(cg);
        }

        const int cellHeight = indexRowSizeHint(modelIndex);
        if(cellHeight > 0) {
            const QRect cellRect{position, y, width, cellHeight};

            if(!paintedBg) {
                // Some styles use a gradient for the selection bg which only covers each individual cell
                // Show this as a single gradient across the entire row
                paintedBg = true;
                drawRowBackground(painter, opt, modelIndex, y);
            }

            opt.rect = cellRect;

            delegate(modelIndex)->paint(painter, opt, modelIndex);
        }
    }

    if(currentRowHasFocus) {
        m_view->drawFocus(painter, opt, modelIndex, y);
    }
}

void TreeView::drawRowBackground(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index,
                                 int y) const
{
    QStyleOptionViewItem opt{option};

    const auto bg = index.data(Qt::BackgroundRole).value<QBrush>();

    const auto paintRects = m_p->rectsToPaint(option, y);
    for(const auto& rect : paintRects) {
        if(rect.width() > 0) {
            opt.rect = rect;
            painter->fillRect(opt.rect, bg);
            m_view->style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, m_view);
        }
    }
}

int TreeView::indexRowSizeHint(const QModelIndex& index) const
{
    if(!isIndexValid(index)) {
        return 0;
    }

    if(m_p->m_uniformRowHeights && m_uniformRowHeight > 0) {
        return m_uniformRowHeight;
    }

    int start{-1};
    int end{-1};
    const int indexRow       = index.row();
    const QModelIndex parent = index.parent();
    int count                = header()->count();

    if(count > 0 && m_view->isVisible()) {
        start = std::min(header()->visualIndexAt(0), 0);
    }
    else {
        count = model()->columnCount(parent);
    }

    end = count - 1;

    if(end < start) {
        std::swap(end, start);
    }

    QStyleOptionViewItem opt;
    m_view->initViewItemOption(&opt);

    opt.rect.setWidth(-1);
    int height{-1};

    for(int column{start}; column <= end; ++column) {
        const int logical = count == 0 ? column : header()->logicalIndex(column);
        if(header()->isSectionHidden(logical)) {
            continue;
        }

        const QModelIndex colIndex = model()->index(indexRow, logical, parent);
        if(colIndex.isValid()) {
            const int hint = delegate(colIndex)->sizeHint(opt, colIndex).height();
            height         = std::max(height, hint);
        }
    }

    if(m_p->m_uniformRowHeights) {
        m_uniformRowHeight = height;
    }

    return height;
}

int TreeView::indexSizeHint(const QModelIndex& index, bool span) const
{
    if(!isIndexValid(index)) {
        return 0;
    }

    if(m_p->m_uniformRowHeights && m_uniformRowHeight > 0) {
        return m_uniformRowHeight;
    }

    QStyleOptionViewItem opt;
    m_view->initViewItemOption(&opt);

    int height{0};
    if(span && m_view->isSpanning(index.column())) {
        height = header()->sectionSize(index.column());
    }

    const int column = index.column();
    if(header()->isSectionHidden(column)) {
        return 0;
    }

    opt.rect.setWidth(header()->sectionSize(column));
    const int hint = delegate(index)->sizeHint(opt, index).height();
    height         = std::max(height, hint);

    if(m_p->m_uniformRowHeights) {
        m_uniformRowHeight = height;
    }

    return height;
}

void TreeView::recalculatePadding()
{
    if(empty()) {
        return;
    }

    int max{0};

    const int sectionCount = header()->count();
    for(int section{0}; section < sectionCount; ++section) {
        const auto columnIndex = model()->index(0, section, {});
        if(columnIndex.isValid()) {
            if(m_view->isSpanning(section)) {
                max = std::max(max, header()->sectionSize(section));
            }
        }
    }

    int rowHeight{0};
    auto& items = viewItems();

    for(auto& item : items) {
        item.padding = 0;

        if(item.hasChildren || item.parentItem == -1) {
            continue;
        }

        const QModelIndex parent = m_p->modelIndex(item.parentItem);
        const int rowCount       = model()->rowCount(parent);
        const int row            = item.index.row();

        if(row == rowCount - 1) {
            if(rowHeight == 0) {
                // Assume all track rows have the same height
                rowHeight = indexRowSizeHint(item.index);
            }

            const int sectionHeight = rowCount * rowHeight;
            item.padding            = (max > sectionHeight) ? max - sectionHeight : 0;
        }
    }
}

void TreeView::drawAndClipSpans(QPainter* painter, const QStyleOptionViewItem& option, int firstVisibleItem,
                                int firstVisibleItemOffset) const
{
    QStyleOptionViewItem opt{option};

    const QRect rect = viewport()->rect();
    QRegion region{rect};
    const int count = itemCount();

    int i{firstVisibleItem};
    int y{firstVisibleItemOffset};

    for(; i < count; ++i) {
        const int height = itemHeight(i) + itemPadding(i);
        if(y + height > rect.top()) {
            break;
        }
        y += height;
    }

    for(; i < count && y <= rect.bottom(); ++i) {
        const auto& item  = viewItem(i);
        QModelIndex index = item.index;

        if(!index.isValid() || model()->hasChildren(index)) {
            y += itemHeight(i) + item.padding;
            continue;
        }

        if(i == firstVisibleItem && index.row() > 0) {
            index = index.siblingAtRow(0);
        }

        if(index.row() > 0) {
            y += itemHeight(i) + item.padding;
            continue;
        }

        y = visualRect(index, RectRule::FullRow, false).y();

        m_leftAndRight           = startAndEndColumns(rect);
        const int left           = m_leftAndRight.first;
        const int right          = m_leftAndRight.second;
        const QModelIndex parent = index.parent();

        int position;
        QModelIndex modelIndex;

        std::vector<int> logicalIndices;
        std::vector<QStyleOptionViewItem::ViewItemPosition> viewItemPosList;

        calcLogicalIndexes(logicalIndices, viewItemPosList, left, right);

        const auto sectionCount = static_cast<int>(logicalIndices.size());

        for(int section{0}; section < sectionCount; ++section) {
            const int headerSection = logicalIndices.at(section);
            const bool spanning     = m_view->isSpanning(headerSection);

            if(!spanning) {
                continue;
            }

            const int width = header()->sectionSize(headerSection);
            position        = header()->sectionViewportPosition(headerSection);

            modelIndex = model()->index(0, headerSection, parent);
            if(!modelIndex.isValid()) {
                continue;
            }

            opt.rect = {position, y, width, indexSizeHint(modelIndex, true)};
            delegate(modelIndex)->paint(painter, opt, modelIndex);
            region -= opt.rect;
        }

        y += itemHeight(i) + item.padding;
    }

    painter->setClipRegion(region);
}

void TreeView::adjustViewOptionsForIndex(QStyleOptionViewItem* option, const QModelIndex& currentIndex) const
{
    const int row = m_p->viewIndex(currentIndex);
    option->state = option->state | QStyle::State_Open
                  | (viewItem(row).hasChildren ? QStyle::State_Children : QStyle::State_None)
                  | (viewItem(row).hasMoreSiblings ? QStyle::State_Sibling : QStyle::State_None);

    std::vector<int> logicalIndexes;
    std::vector<QStyleOptionViewItem::ViewItemPosition> viewItemPosList;

    const bool isSpanning = m_view->isSpanning(currentIndex.column());
    const int left        = (isSpanning ? header()->visualIndex(0) : 0);
    const int right       = (isSpanning ? header()->visualIndex(0) : header()->count() - 1);

    calcLogicalIndexes(logicalIndexes, viewItemPosList, left, right);

    const int visualIndex = static_cast<int>(
        std::ranges::distance(logicalIndexes.cbegin(), std::ranges::find(logicalIndexes, currentIndex.column())));
    option->viewItemPosition = viewItemPosList.at(visualIndex);
}

class IconView : public BaseView
{
public:
    using BaseView::BaseView;

    void invalidate() override;

    void doItemLayout() override;
    void drawView(QPainter* painter, const QRegion& region) const override;

    [[nodiscard]] QRect visualRect(const QModelIndex& index, RectRule rule, bool includePadding) const override;
    [[nodiscard]] ExpandedTreeViewItem indexToViewItem(const QModelIndex& index) const override;
    [[nodiscard]] QModelIndex findIndexAt(const QPoint& point, bool includeSpans, bool includePadding) const override;
    void dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight) override;
    [[nodiscard]] int sizeHintForColumn(int column) const override;

    [[nodiscard]] int firstVisibleItem(int* offset = nullptr) const override;
    [[nodiscard]] int lastVisibleItem(int firstVisual, int offset) const override;
    [[nodiscard]] QPoint coordinateForItem(int item) const override;
    [[nodiscard]] int itemAtCoordinate(QPoint coordinate, bool includePadding) const override;
    [[nodiscard]] int itemAbove(int item) const override;
    [[nodiscard]] int itemBelow(int item) const override;
    [[nodiscard]] int itemLeft(int item) const override;
    [[nodiscard]] int itemRight(int item) const override;
    [[nodiscard]] int pageUp(int item) const override;
    [[nodiscard]] int pageDown(int item) const override;

    void renderToPixmap(QPainter* painter, const ItemViewPaintPairs& paintPairs, QRect& rect) const override;

    void updateScrollBars() override;

private:
    struct SizeHint
    {
        int width{0};
        int height{0};
        int baseHeight{0};
        int captionHeight{0};
    };

    void prepareItemLayout();
    void drawItem(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
    void drawFocus(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
    [[nodiscard]] std::vector<ExpandedTreeViewItem> itemsOnRow(int y, int x) const;
    [[nodiscard]] SizeHint indexSizeHint(const QModelIndex& index) const;
    [[nodiscard]] int itemWidth(int item) const;
    [[nodiscard]] int itemHeight(int item) const;
    [[nodiscard]] int spacing() const;
    [[nodiscard]] QSize iconSize() const;

    QRect m_layoutBounds;
    int m_segmentSize{0};
    int m_itemSpacing{MinItemSpacing};
    int m_rowSpacing{IconRowSpacing};
    mutable int m_uniformBaseHeight{0};
    mutable int m_uniformRowWidth{0};
    mutable int m_uniformRowHeight{0};
    mutable int m_uniformCaptionHeight{0};
};

void IconView::drawView(QPainter* painter, const QRegion& region) const
{
    QStyleOptionViewItem opt;
    m_view->initViewItemOption(&opt);

    if(m_p->m_viewItems.empty() || header()->count() == 0) {
        return;
    }

    int firstVisibleOffset{0};
    const int firstVisible = firstVisibleItem(&firstVisibleOffset);
    if(firstVisible < 0) {
        return;
    }

    const int count  = itemCount();
    const QRect area = region.boundingRect();

    m_leftAndRight = startAndEndColumns(area);

    int i{firstVisible};
    int y{firstVisibleOffset};
    const int max{area.bottom()};
    const int x{area.left()};

    for(; i < count; i += m_segmentSize) {
        const int height = itemHeight(i) + spacing();
        if(y + height > area.top()) {
            break;
        }
        y += height;
    }

    for(; i < count && y <= max; i += m_segmentSize) {
        const auto& item    = viewItem(i);
        const auto rowItems = itemsOnRow(item.y, x);
        for(const auto& rowItem : rowItems) {
            const QModelIndex& index = rowItem.index;
            drawItem(painter, opt, index);
        }
        y += itemHeight(i) + spacing();
    }
}

QRect IconView::visualRect(const QModelIndex& index, RectRule rule, bool /*includePadding*/) const
{
    if(!isIndexValid(index)) {
        return {};
    }

    if(m_view->isIndexHidden(index) && rule != RectRule::FullRow) {
        return {};
    }

    m_p->layoutItems();

    const int viewIndex = m_p->viewIndex(index);
    if(viewIndex < 0) {
        return {};
    }

    if(rule == RectRule::FullRow) {
        return mapToViewport(viewItem(viewIndex).rect());
    }

    const int visual = Utils::realVisualIndex(header(), index.column());

    if(visual == 0) {
        return mapToViewport(viewItem(viewIndex).baseRect());
    }

    return mapToViewport(viewItem(viewIndex).columnRect(visual));
}

void IconView::invalidate()
{
    m_uniformRowWidth      = 0;
    m_uniformRowHeight     = 0;
    m_uniformBaseHeight    = 0;
    m_uniformCaptionHeight = 0;
    m_segmentSize          = 0;
    m_itemSpacing          = MinItemSpacing;
}

void IconView::doItemLayout()
{
    invalidate();
    layout(-1);

    if(empty()) {
        return;
    }

    prepareItemLayout();

    const QPoint topLeft{m_layoutBounds.x(), m_layoutBounds.y() + m_rowSpacing};

    const int segStartPosition{m_layoutBounds.left()};
    const int segEndPosition{m_layoutBounds.right()};

    int deltaSegPosition{0};
    int segPosition{topLeft.y()};

    // Determine the number of items per row
    const int count = itemCount();
    for(int i{1}; i <= count; ++i) {
        const int requiredWidth = i * itemWidth(0) + (i - 1) * m_itemSpacing;
        if(requiredWidth > (segEndPosition - segStartPosition)) {
            m_segmentSize = (i == 1) ? 1 : i - 1;
            break;
        }
    }

    double maxPaddingRatio{1.0};

    if(m_segmentSize == 0) {
        m_segmentSize   = std::max(count, 1);
        maxPaddingRatio = 0.13;
    }

    const int totalWidthAvailable = segEndPosition - segStartPosition;
    const int totalItemWidth      = m_segmentSize * itemWidth(0);
    const int totalPadding        = totalWidthAvailable - totalItemWidth;

    m_itemSpacing = std::max(0, totalPadding / (m_segmentSize + 1));

    const int maxPadding = static_cast<int>(totalWidthAvailable * maxPaddingRatio);
    if(maxPadding > 0 && m_itemSpacing > maxPadding) {
        m_itemSpacing = MinItemSpacing;
    }

    QRect rect{{}, topLeft};

    for(int i{0}; i < count; ++i) {
        auto& item = viewItem(i);

        const int segColumn = i % m_segmentSize;
        if(segColumn == 0 && i > 0) {
            segPosition += deltaSegPosition;
            deltaSegPosition = 0;
        }

        item.x = segStartPosition + m_itemSpacing + segColumn * (itemWidth(i) + m_itemSpacing);
        item.y = segPosition;

        deltaSegPosition = std::max(deltaSegPosition, itemHeight(i) + spacing());
        rect |= item.rect();
    }

    m_contentsSize = rect.size();
    m_contentsSize.rwidth() += m_itemSpacing;
}

ExpandedTreeViewItem IconView::indexToViewItem(const QModelIndex& index) const
{
    if(index.isValid() && index.row() < itemCount()) {
        return viewItem(index.row());
    }

    return {};
}

QModelIndex IconView::findIndexAt(const QPoint& point, bool /*includeSpans*/, bool /*includePadding*/) const
{
    const int count = itemCount();
    if(count == 0) {
        return {};
    }

    const int contentsXCoord = point.x() + horizontalScrollBar()->value();
    const int contentsYCoord = point.y() + verticalScrollBar()->value();

    for(int i{0}; i < count; ++i) {
        const auto& item = viewItem(i);

        if(item.rect().contains(contentsXCoord, contentsYCoord)) {
            return item.index;
        }
    }

    return {};
}

void IconView::dataChanged(const QModelIndex& /*topLeft*/, const QModelIndex& /*bottomRight*/) { }

int IconView::sizeHintForColumn(int /*column*/) const
{
    return -1;
}

void IconView::updateScrollBars()
{
    if(empty()) {
        return;
    }

    verticalScrollBar()->setSingleStep(itemHeight(0) / 4);
    verticalScrollBar()->setPageStep(viewport()->height());
    horizontalScrollBar()->setSingleStep(itemWidth(0) / 4);
    horizontalScrollBar()->setPageStep(viewport()->width());

    const bool bothScrollBarsAuto = m_view->verticalScrollBarPolicy() == Qt::ScrollBarAsNeeded
                                 && m_view->horizontalScrollBarPolicy() == Qt::ScrollBarAsNeeded;

    const QSize vpSize = viewportSize();

    const bool horizontalWantsToShow = m_contentsSize.width() > vpSize.width();
    bool verticalWantsToShow         = m_contentsSize.height() > vpSize.height();
    if(horizontalWantsToShow) {
        verticalWantsToShow = m_contentsSize.height() > vpSize.height() - m_view->horizontalScrollBar()->height();
    }
    else {
        verticalWantsToShow = m_contentsSize.height() > vpSize.height();
    }

    if(bothScrollBarsAuto && !verticalWantsToShow) {
        verticalScrollBar()->setRange(0, 0);
    }
    else {
        verticalScrollBar()->setRange(0, m_contentsSize.height() - viewport()->height());
    }

    if(bothScrollBarsAuto && !horizontalWantsToShow) {
        horizontalScrollBar()->setRange(0, 0);
    }
    else {
        horizontalScrollBar()->setRange(0, m_contentsSize.width() - viewport()->width());
    }
}

IconView::SizeHint IconView::indexSizeHint(const QModelIndex& index) const
{
    if(!m_p->isIndexValid(index) || !m_view->itemDelegate()) {
        return {0, 0, 0};
    }

    QStyleOptionViewItem opt;
    m_view->initViewItemOption(&opt);
    opt.showDecorationSelected = true;
    opt.decorationPosition     = QStyleOptionViewItem::Top;
    opt.displayAlignment       = Qt::AlignCenter;

    SizeHint size;

    const int colCount = model()->columnCount(index.parent());
    for(int col{0}; col < colCount; ++col) {
        if(header()->isSectionHidden(col)) {
            continue;
        }

        const auto colIndex = index.siblingAtColumn(col);
        opt.decorationSize  = Utils::realVisualIndex(header(), colIndex.column()) == 0 ? iconSize() : QSize{};

        const QSize hint = delegate(colIndex)->sizeHint(opt, colIndex);

        if(size.baseHeight == 0 && Utils::realVisualIndex(header(), colIndex.column()) == 0) {
            size.baseHeight = hint.height();
        }
        else if(size.captionHeight == 0) {
            size.captionHeight = hint.height();
        }

        size.width = std::clamp(size.width, hint.width(), iconSize().width() + (2 * MinItemSpacing));

        if(m_p->m_captionDisplay == ExpandedTreeView::CaptionDisplay::None) {
            size.height        = iconSize().height();
            size.baseHeight    = size.height;
            size.captionHeight = 0;
            return size;
        }

        size.height += hint.height();
    }

    return size;
}

int IconView::itemWidth(int item) const
{
    if(empty()) {
        return 0;
    }

    const QModelIndex& index = m_p->m_viewItems.at(item).index;
    if(!index.isValid()) {
        return 0;
    }

    int width = viewItem(item).width;

    if(width <= 0) {
        if(m_uniformRowWidth == 0) {
            const SizeHint hint = indexSizeHint(index);
            m_uniformRowWidth   = hint.width;
        }
        width                = m_uniformRowWidth;
        viewItem(item).width = width;
    }

    return std::max(width, 0);
}

int IconView::itemHeight(int item) const
{
    if(m_p->m_viewItems.empty()) {
        return 0;
    }

    const QModelIndex& index = m_p->m_viewItems.at(item).index;
    if(!index.isValid()) {
        return 0;
    }

    int height = viewItem(item).height;

    if(height <= 0) {
        if(m_uniformRowHeight == 0) {
            const SizeHint hint    = indexSizeHint(index);
            m_uniformRowHeight     = hint.height;
            m_uniformBaseHeight    = hint.baseHeight;
            m_uniformCaptionHeight = hint.captionHeight;
        }
        height                       = m_uniformRowHeight;
        viewItem(item).height        = height;
        viewItem(item).baseHeight    = m_uniformBaseHeight;
        viewItem(item).captionHeight = m_uniformCaptionHeight;
    }

    return std::max(height, 0);
}

int IconView::spacing() const
{
    return m_rowSpacing;
}

QSize IconView::iconSize() const
{
    return m_view->iconSize();
}

void IconView::prepareItemLayout()
{
    m_layoutBounds = {{}, m_view->maximumViewportSize()};

    const QStyle* style = m_view->style();

    int frameAroundContents{0};
    if(style->styleHint(QStyle::SH_ScrollView_FrameOnlyAroundContents)) {
        QStyleOption option;
        option.initFrom(m_view);
        frameAroundContents = m_view->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, &option, m_view) * 2;
    }

    const int verticalMargin
        = (verticalScrollBarPolicy() == Qt::ScrollBarAsNeeded) && (verticalScrollBar()->isVisible())
               && !style->pixelMetric(QStyle::PM_ScrollView_ScrollBarOverlap, nullptr, verticalScrollBar())
            ? style->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, verticalScrollBar()) + frameAroundContents
            : 0;
    const int horizontalMargin
        = horizontalScrollBarPolicy() == Qt::ScrollBarAsNeeded
            ? style->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, horizontalScrollBar()) + frameAroundContents
            : 0;

    m_layoutBounds.adjust(0, 0, -verticalMargin, -horizontalMargin);
}

void IconView::drawItem(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if(!index.isValid()) {
        return;
    }

    QStyleOptionViewItem opt{option};

    const QModelIndex hover = m_p->m_hoverIndex;
    const bool hoverRow     = index.parent() == hover.parent() && index.row() == hover.row();
    const bool rowFocused   = m_view->hasFocus();

    opt.state.setFlag(QStyle::State_MouseOver, hoverRow);

    const int left            = m_leftAndRight.first;
    const int right           = m_leftAndRight.second;
    const QModelIndex current = m_view->currentIndex();

    std::vector<int> logicalIndices;
    std::vector<QStyleOptionViewItem::ViewItemPosition> viewItemPosList;

    calcLogicalIndexes(logicalIndices, viewItemPosList, left, right);

    bool currentRowHasFocus{false};

    if(m_view->selectionModel()->isSelected(index)) {
        opt.state |= QStyle::State_Selected;
    }
    if(rowFocused && (current == index)) {
        currentRowHasFocus = true;
    }

    if(opt.state & QStyle::State_Enabled) {
        QPalette::ColorGroup cg;
        if((model()->flags(index) & Qt::ItemIsEnabled) == 0) {
            opt.state &= ~QStyle::State_Enabled;
            cg = QPalette::Disabled;
        }
        else if(opt.state & QStyle::State_Active) {
            cg = QPalette::Active;
        }
        else {
            cg = QPalette::Inactive;
        }
        opt.palette.setCurrentColorGroup(cg);
    }

    const auto count = static_cast<int>(logicalIndices.size());

    for(int section{0}; section < count; ++section) {
        const int headerSection      = logicalIndices.at(section);
        const QModelIndex modelIndex = model()->index(index.row(), headerSection, index.parent());

        if(!modelIndex.isValid()) {
            continue;
        }

        opt.rect = m_view->visualRect(modelIndex);

        const int visualIndex = Utils::realVisualIndex(header(), headerSection);
        if(visualIndex == 0) {
            QStyleOptionViewItem mainOpt{opt};
            mainOpt.decorationSize         = iconSize();
            mainOpt.showDecorationSelected = true;
            mainOpt.decorationPosition     = QStyleOptionViewItem::Top;
            mainOpt.displayAlignment       = Qt::AlignCenter;

            delegate(modelIndex)->paint(painter, mainOpt, modelIndex);
        }
        else {
            opt.decorationSize = {0, 0};
            delegate(modelIndex)->paint(painter, opt, modelIndex);
        }
    }

    if(currentRowHasFocus) {
        drawFocus(painter, opt, index);
    }
}

void IconView::drawFocus(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionFocusRect focusOpt;
    focusOpt.QStyleOption::operator=(option);
    focusOpt.state |= QStyle::State_KeyboardFocusChange;
    const QPalette::ColorGroup cg = (option.state & QStyle::State_Enabled) ? QPalette::Normal : QPalette::Disabled;
    focusOpt.backgroundColor      = option.palette.color(
        cg, m_view->selectionModel()->isSelected(index) ? QPalette::Highlight : QPalette::Window);

    const auto item  = indexToViewItem(index);
    const auto& rect = mapToViewport(item.rect());

    if(rect.width() > 0) {
        focusOpt.rect = QStyle::visualRect(m_view->layoutDirection(), viewport()->rect(), rect);
        m_view->style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focusOpt, painter);
    }
}

int IconView::firstVisibleItem(int* offset) const
{
    const int value = verticalScrollBar()->value();
    const int count = itemCount();

    int y{0};

    for(int i{0}; i < count; i += m_segmentSize) {
        const int height = itemHeight(i) + spacing();
        y += height;
        if(y > value) {
            if(offset) {
                *offset = y - value - height;
            }
            return i;
        }
    }

    return -1;
}

int IconView::lastVisibleItem(int firstVisual, int offset) const
{
    if(firstVisual < 0 || offset < 0) {
        firstVisual = firstVisibleItem(&offset);
        if(firstVisual < 0) {
            return -1;
        }
    }

    int y{-offset};
    const int value = viewport()->height();

    const int count = itemCount();
    for(int i{firstVisual}; i < count; ++i) {
        y += itemHeight(i);
        if(y > value) {
            return i;
        }
    }

    return count - 1;
}

QPoint IconView::coordinateForItem(int item) const
{
    if(item < 0 || item >= itemCount()) {
        return {0, 0};
    }

    const auto& itm = viewItem(item);

    return {mapToViewport(itm.rect()).topLeft()};
}

int IconView::itemAtCoordinate(QPoint coordinate, bool includePadding) const
{
    const int count = itemCount();
    if(count == 0) {
        return -1;
    }

    coordinate += {0, verticalScrollBar()->value()};

    for(int i{0}; i < count; ++i) {
        const auto& rect = includePadding ? viewItem(i).rect() : mapToViewport(viewItem(i).rect());
        if(rect.contains(coordinate)) {
            return i;
        }
    }

    return -1;
}

int IconView::itemAbove(int item) const
{
    const int i{item};
    while(m_p->isItemDisabled(item -= m_segmentSize)) { }
    return item < 0 ? i : item;
}

int IconView::itemBelow(int item) const
{
    const int i{item};
    while(m_p->isItemDisabled(item += m_segmentSize)) { }
    return item >= itemCount() ? i : item;
}

int IconView::itemLeft(int item) const
{
    if(item % m_segmentSize == 0) {
        return item;
    }
    while(m_p->isItemDisabled(--item)) { }
    return item;
}

int IconView::itemRight(int item) const
{
    if((item + 1) % m_segmentSize == 0) {
        return item;
    }
    while(m_p->isItemDisabled(++item)) { }
    return item;
}

int IconView::pageUp(int item) const
{
    auto coord = coordinateForItem(item);
    coord.ry() -= viewport()->height();

    return itemAtCoordinate(coord, true);
}

int IconView::pageDown(int item) const
{
    auto coord = coordinateForItem(item);
    coord.ry() += viewport()->height();

    return itemAtCoordinate(coord, true);
}

void IconView::renderToPixmap(QPainter* painter, const ItemViewPaintPairs& paintPairs, QRect& rect) const
{
    QStyleOptionViewItem opt;
    m_view->initViewItemOption(&opt);

    opt.state |= QStyle::State_Selected;

    for(const auto& [paintRect, index] : paintPairs) {
        QStyleOptionViewItem paintOpt{opt};

        const int visualColumn = Utils::realVisualIndex(header(), index.column());
        if(visualColumn == 0) {
            paintOpt.decorationSize         = iconSize();
            paintOpt.showDecorationSelected = true;
            paintOpt.decorationPosition     = QStyleOptionViewItem::Top;
            paintOpt.displayAlignment       = Qt::AlignCenter;
        }
        else {
            paintOpt.decorationSize = {};
        }

        paintOpt.rect = paintRect.translated(-rect.topLeft());

        delegate(index)->paint(painter, paintOpt, index);
    }
}

std::vector<ExpandedTreeViewItem> IconView::itemsOnRow(int y, int x) const
{
    std::vector<ExpandedTreeViewItem> result;

    bool foundAjacant{false};
    for(const auto& item : m_p->m_viewItems) {
        if(item.y == y && item.x >= x) {
            foundAjacant = true;
            result.push_back(item);
        }
        else if(foundAjacant) {
            break;
        }
    }

    return result;
}

ExpandedTreeView::Private::Private(ExpandedTreeView* self)
    : m_self{self}
    , m_header{new QHeaderView(Qt::Horizontal, m_self)}
{
    m_header->setSectionsClickable(true);
    m_header->setContextMenuPolicy(Qt::CustomContextMenu);
    m_header->setSectionsMovable(true);
    m_header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    QObject::connect(m_header, &QHeaderView::sectionResized, this, &ExpandedTreeView::Private::columnResized);
    QObject::connect(m_header, &QHeaderView::sectionMoved, this, [this]() { m_self->viewport()->update(); });
    QObject::connect(m_header, &QHeaderView::sectionCountChanged, this, &ExpandedTreeView::Private::columnCountChanged);
    QObject::connect(m_header, &QHeaderView::sectionHandleDoubleClicked, this,
                     &ExpandedTreeView::Private::resizeColumnToContents);
    QObject::connect(m_header, &QHeaderView::geometriesChanged, this, [this]() { m_self->updateGeometries(); });
}

int ExpandedTreeView::Private::itemCount() const
{
    return static_cast<int>(m_viewItems.size());
}

QModelIndex ExpandedTreeView::Private::modelIndex(int i, int column) const
{
    if(i < 0 || i >= itemCount()) {
        return {};
    }

    QModelIndex index = m_viewItems.at(i).index;

    if(column > 0) {
        index = index.sibling(index.row(), column);
    }

    return index;
}

void ExpandedTreeView::Private::select(const QModelIndex& topIndex, const QModelIndex& bottomIndex,
                                       QItemSelectionModel::SelectionFlags command) const
{
    QItemSelection selection;
    const int top    = viewIndex(topIndex);
    const int bottom = viewIndex(bottomIndex);

    const int left{0};
    const int right{m_header->count() - 1};

    QModelIndex previous;
    QItemSelectionRange currentRange;
    QStack<QItemSelectionRange> rangeStack;

    for(int i{top}; i <= bottom; ++i) {
        QModelIndex index         = modelIndex(i);
        const auto parent         = index.parent();
        const auto previousParent = previous.parent();

        if(previous.isValid() && parent == previousParent) {
            if(std::abs(previous.row() - index.row()) > 1) {
                if(currentRange.isValid()) {
                    selection.append(currentRange);
                }
                currentRange = {index.sibling(index.row(), left), index.sibling(index.row(), right)};
            }
            else {
                const auto tl = m_model->index(currentRange.top(), currentRange.left(), currentRange.parent());
                currentRange  = {tl, index.sibling(index.row(), right)};
            }
        }
        else if(previous.isValid() && parent == m_model->index(previous.row(), 0, previousParent)) {
            rangeStack.push(currentRange);
            currentRange = {index.sibling(index.row(), left), index.sibling(index.row(), right)};
        }
        else {
            if(currentRange.isValid()) {
                selection.append(currentRange);
            }

            if(rangeStack.empty()) {
                currentRange = {index.sibling(index.row(), left), index.sibling(index.row(), right)};
            }
            else {
                currentRange = rangeStack.pop();
                index        = currentRange.bottomRight();
                --i;
            }
        }
        previous = index;
    }

    if(currentRange.isValid()) {
        selection.append(currentRange);
    }

    for(int i{0}; i < rangeStack.size(); ++i) {
        selection.append(rangeStack.at(i));
    }

    m_self->selectionModel()->select(selection, command);
}

void ExpandedTreeView::Private::resizeColumnToContents(int column) const
{
    layoutItems();

    if(column < 0 || column >= m_header->count()) {
        return;
    }

    const int contents = m_self->sizeHintForColumn(column);
    const int header   = m_header->isHidden() ? 0 : m_header->sectionSizeHint(column);
    m_header->resizeSection(column, std::max(contents, header));
}

void ExpandedTreeView::Private::columnCountChanged(int oldCount, int newCount) const
{
    if(oldCount == 0 && newCount > 0) {
        layoutItems();
    }

    if(m_self->isVisible()) {
        m_self->updateGeometries();
    }

    m_self->viewport()->update();
}

void ExpandedTreeView::Private::columnResized(int logical, int oldSize, int newSize)
{
    if(m_viewMode == ViewMode::Tree) {
        if(m_spans.contains(logical)) {
            m_view->updateColumns();
        }
        if(m_columnResizeTimerId == 0) {
            m_columnResizeTimerId = m_self->startTimer(20);
        }
    }
    else if(Utils::visibleSectionCount(m_header) > 1 || newSize == 0 || (oldSize == 0 && newSize > 0)) {
        doDelayedItemsLayout();
    }
}

void ExpandedTreeView::Private::doDelayedItemsLayout(int delay) const
{
    if(!m_delayedPendingLayout) {
        m_delayedPendingLayout = true;
        m_delayedLayout.start(delay, m_self);
    }
}

void ExpandedTreeView::Private::interruptDelayedItemsLayout() const
{
    m_delayedLayout.stop();
    m_delayedPendingLayout = false;
}

void ExpandedTreeView::Private::layoutItems() const
{
    if(m_delayedPendingLayout) {
        interruptDelayedItemsLayout();
        m_self->doItemsLayout();
    }
}

ExpandedTreeViewItem ExpandedTreeView::Private::indexToListViewItem(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return {};
    }

    return m_view->indexToViewItem(index);
}

int ExpandedTreeView::Private::viewIndex(const QModelIndex& index) const
{
    if(!index.isValid() || m_viewItems.empty()) {
        return -1;
    }

    const int totalCount         = itemCount();
    const QModelIndex firstIndex = index.sibling(index.row(), 0);
    const int row                = firstIndex.row();
    const auto internalId        = firstIndex.internalId();

    const int localCount = std::min(m_lastViewedItem - 1, totalCount - m_lastViewedItem);
    for(int i{0}; i < localCount; ++i) {
        const QModelIndex& idx1 = m_viewItems.at(m_lastViewedItem + i).index;
        if(idx1.row() == row && idx1.internalId() == internalId) {
            m_lastViewedItem = m_lastViewedItem + i;
            return m_lastViewedItem;
        }
        const QModelIndex& idx2 = m_viewItems.at(m_lastViewedItem - i - 1).index;
        if(idx2.row() == row && idx2.internalId() == internalId) {
            m_lastViewedItem = m_lastViewedItem - i - 1;
            return m_lastViewedItem;
        }
    }

    const int minItem = std::max(0, m_lastViewedItem + localCount);
    for(int j{minItem}; j < totalCount; ++j) {
        const QModelIndex& idx = m_viewItems.at(j).index;
        if(idx.row() == row && idx.internalId() == internalId) {
            m_lastViewedItem = j;
            return j;
        }
    }

    const int maxItem = std::min(totalCount, m_lastViewedItem - localCount) - 1;
    for(int j{maxItem}; j >= 0; --j) {
        const QModelIndex& idx = m_viewItems.at(j).index;
        if(idx.row() == row && idx.internalId() == internalId) {
            m_lastViewedItem = j;
            return j;
        }
    }

    return -1;
}

void ExpandedTreeView::Private::insertViewItems(int pos, int count, const ExpandedTreeViewItem& viewItem)
{
    m_viewItems.insert(m_viewItems.begin() + pos, count, viewItem);

    const int itmCount = itemCount();
    for(int i{pos + count}; i < itmCount; i++) {
        if(m_viewItems.at(i).parentItem >= pos) {
            m_viewItems.at(i).parentItem += count;
        }
    }
}

bool ExpandedTreeView::Private::hasVisibleChildren(const QModelIndex& parent) const
{
    if(parent.flags() & Qt::ItemNeverHasChildren) {
        return false;
    }

    return m_model->hasChildren(parent);
}

bool ExpandedTreeView::Private::isIndexEnabled(const QModelIndex& index) const
{
    return m_model->flags(index) & Qt::ItemIsEnabled;
}

bool ExpandedTreeView::Private::isItemDisabled(int i) const
{
    if(i < 0 || i >= itemCount()) {
        return false;
    }

    const QModelIndex index = m_viewItems.at(i).index;
    return !isIndexEnabled(index);
}

bool ExpandedTreeView::Private::itemHasChildren(int i) const
{
    if(i < 0 || i >= itemCount()) {
        return false;
    }

    return m_viewItems.at(i).hasChildren;
}

void ExpandedTreeView::Private::invalidateHeightCache(int item) const
{
    m_viewItems[item].height = 0;
}

int ExpandedTreeView::Private::itemForHomeKey() const
{
    int index{0};
    while(isItemDisabled(index) || itemHasChildren(index)) {
        index++;
    }
    return index >= itemCount() ? 0 : index;
}

int ExpandedTreeView::Private::itemForEndKey() const
{
    int index = itemCount() - 1;
    while(isItemDisabled(index)) {
        index--;
    }
    return index == -1 ? itemCount() - 1 : index;
}

void ExpandedTreeView::Private::setHoverIndex(const QPersistentModelIndex& index)
{
    if(m_hoverIndex == index) {
        return;
    }

    auto* viewport = m_self->viewport();

    const QRect oldHoverRect = m_view->visualRect(m_hoverIndex, RectRule::FullRow, false);
    const QRect newHoverRect = m_view->visualRect(index, RectRule::FullRow, false);

    viewport->update(QRect{0, newHoverRect.y(), viewport->width(), newHoverRect.height()});
    viewport->update(QRect{0, oldHoverRect.y(), viewport->width(), oldHoverRect.height()});

    m_hoverIndex = index;
}

bool ExpandedTreeView::Private::isIndexDropEnabled(const QModelIndex& index) const
{
    return m_model->flags(index) & Qt::ItemIsDropEnabled;
}

QModelIndexList ExpandedTreeView::Private::selectedDraggableIndexes(bool fullRow) const
{
    QModelIndexList indexes
        = fullRow ? m_self->selectionModel()->selectedRows(m_header->logicalIndex(0)) : m_self->selectedIndexes();

    auto isNotDragEnabled = [this](const QModelIndex& index) {
        return !(m_model->flags(index) & Qt::ItemIsDragEnabled);
    };
    indexes.removeIf(isNotDragEnabled);

    return indexes;
}

bool ExpandedTreeView::Private::shouldAutoScroll() const
{
    if(!m_self->hasAutoScroll()) {
        return false;
    }

    const QRect area       = m_self->viewport()->rect();
    const int scrollMargin = m_self->autoScrollMargin();

    return (m_dragPos.y() - area.top() < scrollMargin) || (area.bottom() - m_dragPos.y() < scrollMargin)
        || (m_dragPos.x() - area.left() < scrollMargin) || (area.right() - m_dragPos.x() < scrollMargin);
}

void ExpandedTreeView::Private::startAutoScroll()
{
    m_autoScrollTimer.start(50, m_self);
    m_autoScrollCount = 0;
}

void ExpandedTreeView::Private::stopAutoScroll()
{
    m_autoScrollTimer.stop();
    m_autoScrollCount = 0;
}

void ExpandedTreeView::Private::doAutoScroll()
{
    QScrollBar* scroll = m_self->verticalScrollBar();

    if(m_autoScrollCount < scroll->pageStep()) {
        ++m_autoScrollCount;
    }

    const int value        = scroll->value();
    const QPoint pos       = m_dragPos;
    const QRect area       = m_self->viewport()->rect();
    const int scrollMargin = m_self->autoScrollMargin();

    if(pos.y() - area.top() < scrollMargin) {
        scroll->setValue(value - m_autoScrollCount);
    }
    else if(area.bottom() - pos.y() < scrollMargin) {
        scroll->setValue(value + m_autoScrollCount);
    }

    const bool verticalUnchanged = value == scroll->value();

    if(verticalUnchanged) {
        stopAutoScroll();
    }
    else {
        m_dropIndicatorRect = {};
        m_dropIndicatorPos  = OnViewport;
    }
}

bool ExpandedTreeView::Private::dropOn(QDropEvent* event, int& dropRow, int& dropCol, QModelIndex& dropIndex)
{
    if(event->isAccepted()) {
        return false;
    }

    QModelIndex index{dropIndex};
    const QPoint pos = event->position().toPoint();

    if(!(m_model->supportedDropActions() & event->dropAction())) {
        return false;
    }

    int row{-1};
    int col{-1};

    if(index.isValid()) {
        m_dropIndicatorPos = m_self->dropPosition(pos, m_view->visualRect(index, RectRule::FullRow, false), index);
        switch(m_dropIndicatorPos) {
            case(AboveItem):
                row   = index.row();
                col   = index.column();
                index = index.parent();
                break;
            case(BelowItem):
                row   = index.row() + 1;
                col   = index.column();
                index = index.parent();
                break;
            case(OnItem):
            case(OnViewport):
                break;
        }
    }
    else {
        m_dropIndicatorPos = OnViewport;
        row                = m_model->rowCount({});
    }

    dropIndex = index;
    dropRow   = row;
    dropCol   = col;

    return true;
}

std::vector<QRect> ExpandedTreeView::Private::rectsToPaint(const QStyleOptionViewItem& option, int y) const
{
    std::vector<QRect> rects;

    QRect currRect{0, y, 0, option.rect.height()};

    const int offset = m_header->offset();
    const int count  = m_header->count();

    for(int section{0}; section < count; ++section) {
        const int logical  = m_header->logicalIndex(section);
        const int position = m_header->sectionPosition(logical) - offset;

        if(m_header->isSectionHidden(logical)) {
            continue;
        }

        if(m_self->isSpanning(logical)) {
            if(currRect.width() > 0) {
                rects.emplace_back(currRect);
            }
            currRect.setWidth(0);
        }
        else {
            if(currRect.width() == 0) {
                currRect.setX(position);
            }
            currRect.setRight(position + m_header->sectionSize(logical) - 1);
        }
    }

    if(currRect.width() > 0) {
        rects.emplace_back(currRect);
    }

    return rects;
}

bool ExpandedTreeView::Private::isIndexValid(const QModelIndex& index) const
{
    return (index.row() >= 0) && (index.column() >= 0) && (index.model() == m_model);
}

ExpandedTreeView::ExpandedTreeView(QWidget* parent)
    : QAbstractItemView{parent}
    , p{std::make_unique<Private>(this)}
{
    setObjectName(QStringLiteral("ExpandedTreeView"));

    setViewMode(ViewMode::Tree);
    // setIconSize({180, 180});

    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::MoveAction);
    setDropIndicatorShown(true);
    setTextElideMode(Qt::ElideRight);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    viewport()->setAcceptDrops(true);
}

ExpandedTreeView::~ExpandedTreeView()
{
    p->m_autoScrollTimer.stop();
    p->m_delayedLayout.stop();
}

QHeaderView* ExpandedTreeView::header() const
{
    return p->m_header;
}

void ExpandedTreeView::setHeader(QHeaderView* header)
{
    if(p->m_header == header) {
        return;
    }

    if(p->m_header && p->m_header->parent() == this) {
        delete p->m_header;
    }

    p->m_header = header;
    p->m_header->setParent(this);

    if(!p->m_header->model()) {
        p->m_header->setModel(model());
        if(selectionModel()) {
            p->m_header->setSelectionModel(selectionModel());
        }
    }

    QObject::connect(p->m_header, &QHeaderView::sectionResized, p.get(), &ExpandedTreeView::Private::columnResized);
    QObject::connect(p->m_header, &QHeaderView::sectionMoved, this, [this]() { viewport()->update(); });
    QObject::connect(p->m_header, &QHeaderView::sectionCountChanged, p.get(),
                     &ExpandedTreeView::Private::columnCountChanged);
    QObject::connect(p->m_header, &QHeaderView::sectionHandleDoubleClicked, p.get(),
                     &ExpandedTreeView::Private::resizeColumnToContents);
    QObject::connect(p->m_header, &QHeaderView::geometriesChanged, this, [this]() { updateGeometries(); });

    updateGeometries();
}

void ExpandedTreeView::setModel(QAbstractItemModel* model)
{
    if(!model || p->m_model == model) {
        return;
    }

    if(p->m_model) {
        QObject::disconnect(p->m_model, nullptr, this, nullptr);
    }

    p->m_model = model;

    QAbstractItemView::setModel(model);

    QObject::connect(model, &QAbstractItemModel::rowsRemoved, this, &ExpandedTreeView::rowsRemoved);
}

bool ExpandedTreeView::isSpanning(int column) const
{
    return p->m_spans.contains(column);
}

void ExpandedTreeView::setSpan(int column, bool span)
{
    if(span) {
        p->m_spans.emplace(column);
    }
    else {
        p->m_spans.erase(column);
    }
}

ExpandedTreeView::ViewMode ExpandedTreeView::viewMode() const
{
    return p->m_viewMode;
}

void ExpandedTreeView::setViewMode(ViewMode mode)
{
    p->interruptDelayedItemsLayout();
    p->m_viewItems.clear();
    p->m_viewMode = mode;

    p->m_view.reset();

    if(mode == ViewMode::Tree) {
        p->m_view = std::make_unique<TreeView>(this, p.get());
    }
    else {
        p->m_view = std::make_unique<IconView>(this, p.get());
    }

    p->doDelayedItemsLayout();

    emit viewModeChanged(mode);
}

ExpandedTreeView::CaptionDisplay ExpandedTreeView::captionDisplay() const
{
    return p->m_captionDisplay;
}

void ExpandedTreeView::setCaptionDisplay(CaptionDisplay display)
{
    p->m_captionDisplay = display;
}

bool ExpandedTreeView::uniformRowHeights() const
{
    return p->m_uniformRowHeights;
}

void ExpandedTreeView::setUniformRowHeights(bool enabled)
{
    p->m_uniformRowHeights = enabled;
}

void ExpandedTreeView::changeIconSize(const QSize& size)
{
    if(iconSize() != size) {
        setIconSize(size);
        p->doDelayedItemsLayout(100);
    }
}

QRect ExpandedTreeView::visualRect(const QModelIndex& index) const
{
    return p->m_view->visualRect(index, RectRule::SingleSection, true);
}

int ExpandedTreeView::sizeHintForColumn(int column) const
{
    p->layoutItems();

    if(p->m_viewItems.empty()) {
        return -1;
    }

    ensurePolished();

    return p->m_view->sizeHintForColumn(column);
}

void ExpandedTreeView::scrollTo(const QModelIndex& index, ScrollHint hint)
{
    if(!p->isIndexValid(index)) {
        return;
    }

    if(state() == QAbstractItemView::DraggingState || state() == QAbstractItemView::DragSelectingState) {
        // Prevent scrolling to index during drag-n-drop
        return;
    }

    p->layoutItems();
    if(p->m_view) {
        p->m_view->updateScrollBars();
    }

    const QRect rect = visualRect(index);
    if(!rect.isValid()) {
        return;
    }

    const QRect area = viewport()->rect();

    if(!rect.isEmpty()) {
        if(hint == EnsureVisible && area.contains(rect)) {
            viewport()->update(rect);
        }
        else {
            const bool above = (hint == EnsureVisible && (rect.top() < area.top() || area.height() < rect.height()));
            const bool below
                = (hint == EnsureVisible && rect.bottom() > area.bottom() && rect.height() < area.height());
            int verticalValue = verticalScrollBar()->value();

            if(hint == PositionAtTop || above) {
                verticalValue += rect.top();
            }
            else if(hint == PositionAtBottom || below) {
                verticalValue += rect.bottom() - area.height();
            }
            else if(hint == PositionAtCenter) {
                verticalValue += rect.top() - ((area.height() - rect.height()) / 2);
            }

            verticalScrollBar()->setValue(verticalValue);
        }
    }

    const int viewportWidth      = viewport()->width();
    const int horizontalPosition = p->m_header->sectionPosition(index.column());
    const int sectionWidth       = p->m_header->sectionSize(index.column());

    if(hint == PositionAtCenter) {
        horizontalScrollBar()->setValue(horizontalPosition - ((viewportWidth - sectionWidth) / 2));
    }
    else {
        const int horizontalOffset = p->m_header->offset();

        if(horizontalPosition - horizontalOffset < 0 || sectionWidth > viewportWidth) {
            horizontalScrollBar()->setValue(horizontalPosition);
        }
        else if(horizontalPosition - horizontalOffset + sectionWidth > viewportWidth) {
            horizontalScrollBar()->setValue(horizontalPosition - viewportWidth + sectionWidth);
        }
    }
}

QModelIndex ExpandedTreeView::indexAt(const QPoint& point) const
{
    return findIndexAt(point, false, true);
}

QModelIndex ExpandedTreeView::findIndexAt(const QPoint& point, bool includeSpans, bool includePadding) const
{
    p->layoutItems();

    return p->m_view->findIndexAt(point, includeSpans, includePadding);
}

QModelIndex ExpandedTreeView::indexAbove(const QModelIndex& index) const
{
    if(!p->isIndexValid(index)) {
        return {};
    }

    p->layoutItems();

    const int i = p->viewIndex(index) - 1;
    if(i < 0) {
        return {};
    }

    const QModelIndex firstIndex = p->m_viewItems.at(i).index;
    return firstIndex.sibling(firstIndex.row(), index.column());
}

QModelIndex ExpandedTreeView::indexBelow(const QModelIndex& index) const
{
    if(!p->isIndexValid(index)) {
        return {};
    }

    p->layoutItems();

    const int i = p->viewIndex(index) + 1;
    if(i > p->itemCount()) {
        return {};
    }

    const QModelIndex firstIndex = p->m_viewItems.at(i).index;
    return firstIndex.sibling(firstIndex.row(), index.column());
}

void ExpandedTreeView::doItemsLayout()
{
    if(p->m_layingOutItems) {
        return;
    }

    p->m_layingOutItems = true;
    p->m_viewItems.clear();

    if(p->m_model && p->m_model->hasChildren(rootIndex())) {
        p->m_view->doItemLayout();
    }

    QAbstractItemView::doItemsLayout();

    p->m_header->doItemsLayout();

    p->m_layingOutItems = false;
}

void ExpandedTreeView::reset()
{
    QAbstractItemView::reset();
    p->doDelayedItemsLayout();
}

void ExpandedTreeView::updateGeometries()
{
    if(p->m_updatingGeometry) {
        return;
    }

    p->m_updatingGeometry = true;
    int height{0};

    if(!p->m_header->isHidden()) {
        height = std::max(p->m_header->minimumHeight(), p->m_header->sizeHint().height());
        height = std::min(height, p->m_header->maximumHeight());
    }

    setViewportMargins(0, height, 0, 0);
    const QRect geometry = viewport()->geometry();
    const QRect headerGeometry{geometry.left(), geometry.top() - height, geometry.width(), height};

    p->m_header->setGeometry(headerGeometry);
    QMetaObject::invokeMethod(p->m_header, "updateGeometries");

    if(!p->m_layingOutItems && p->m_hidingScrollbar > 0) {
        --p->m_hidingScrollbar;
    }
    else if(p->m_view) {
        p->m_view->updateScrollBars();
    }

    p->m_updatingGeometry = false;

    QAbstractItemView::updateGeometries();
}

void ExpandedTreeView::dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles)
{
    p->m_view->dataChanged(topLeft, bottomRight);

    QAbstractItemView::dataChanged(topLeft, bottomRight, roles);
}

void ExpandedTreeView::selectAll()
{
    if(!selectionModel()) {
        return;
    }

    p->layoutItems();

    const SelectionMode mode = selectionMode();
    if(mode != SingleSelection && mode != NoSelection && !p->m_viewItems.empty()) {
        const QModelIndex& idx          = p->m_viewItems.back().index;
        const QModelIndex lastItemIndex = idx.sibling(idx.row(), p->m_model->columnCount(idx.parent()) - 1);
        p->select(p->m_viewItems.front().index, lastItemIndex,
                  QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
}

bool ExpandedTreeView::viewportEvent(QEvent* event)
{
    switch(event->type()) {
        case(QEvent::HoverEnter):
        case(QEvent::HoverMove): {
            if(auto* hoverEvent = static_cast<QHoverEvent*>(event)) {
                p->setHoverIndex(indexAt(hoverEvent->position().toPoint()));
            }
            break;
        }
        case(QEvent::HoverLeave):
        case(QEvent::Leave):
            p->setHoverIndex({});
            break;
        default:
            break;
    }

    return QAbstractItemView::viewportEvent(event);
}

void ExpandedTreeView::dragMoveEvent(QDragMoveEvent* event)
{
    const QPoint pos = event->position().toPoint();
    p->m_dragPos     = pos;

    event->ignore();

    if(event->modifiers() & Qt::ControlModifier) {
        event->setDropAction(Qt::CopyAction);
    }
    else {
        event->setDropAction(Qt::MoveAction);
    }

    QModelIndex index = findIndexAt(pos, true);
    p->m_hoverIndex   = index;

    if(!index.isValid() && p->itemCount() > 0) {
        index = p->m_viewItems.back().index;
        index = index.siblingAtColumn(Utils::firstVisualIndex(p->m_header));
    }

    if(index.isValid() && showDropIndicator()) {
        const QRect rect      = p->m_view->visualRect(index, RectRule::FullRow, false);
        p->m_dropIndicatorPos = dropPosition(pos, rect, index);

        switch(p->m_dropIndicatorPos) {
            case(AboveItem): {
                if(p->isIndexDropEnabled(index.parent())) {
                    p->m_dropIndicatorRect = {rect.left(), rect.top(), rect.width(), 0};
                    event->acceptProposedAction();
                }
                else {
                    p->m_dropIndicatorRect = {};
                }
                break;
            }
            case(BelowItem): {
                if(p->isIndexDropEnabled(index.parent())) {
                    p->m_dropIndicatorRect = {rect.left(), rect.bottom(), rect.width(), 0};
                    event->acceptProposedAction();
                }
                else {
                    p->m_dropIndicatorRect = {};
                }
                break;
            }
            case(OnItem): {
                p->m_dropIndicatorRect = {};
                event->ignore();
                break;
            }
            case(OnViewport): {
                p->m_dropIndicatorRect = {};

                if(p->isIndexDropEnabled({})) {
                    event->acceptProposedAction();
                }
                break;
            }
        }
    }
    else {
        p->m_dropIndicatorRect = {};
        p->m_dropIndicatorPos  = OnViewport;

        if(p->isIndexDropEnabled({})) {
            event->acceptProposedAction();
        }
    }

    viewport()->update();

    if(p->shouldAutoScroll()) {
        p->startAutoScroll();
    }
}

void ExpandedTreeView::dragLeaveEvent(QDragLeaveEvent* /*event*/)
{
    setState(NoState);
    p->m_hoverIndex = QModelIndex{};
    viewport()->update();
}

void ExpandedTreeView::mousePressEvent(QMouseEvent* event)
{
    const QPoint pos        = event->position().toPoint();
    const QModelIndex index = indexAt(pos);

    if(!index.isValid()) {
        QAbstractItemView::mousePressEvent(event);
        clearSelection();
        return;
    }

    auto* selectModel            = selectionModel();
    const QModelIndex modelIndex = index.siblingAtColumn(0);

    if(!model()->hasChildren(modelIndex)) {
        setDragEnabled(selectModel->isSelected(modelIndex)); // Prevent drag-and-drop when first selecting leafs
        QAbstractItemView::mousePressEvent(event);
        return;
    }

    setDragEnabled(true);

    const QItemSelection selection = selectRecursively(p->m_model, {modelIndex, modelIndex});

    QAbstractItemView::mousePressEvent(event);

    auto command = selectionCommand(index, event);
    selectModel->select(selection, command);
}

void ExpandedTreeView::wheelEvent(QWheelEvent* event)
{
    if(!(event->modifiers() & Qt::ControlModifier)) {
        QAbstractItemView::wheelEvent(event);
        return;
    }

    const int delta     = event->angleDelta().y();
    const int increment = (delta > 0) ? 1 : -1;
    int newSize         = iconSize().width() + increment * 2;
    newSize             = std::clamp(newSize, 16, 1024);
    changeIconSize({newSize, newSize});

    event->accept();
}

void ExpandedTreeView::resizeEvent(QResizeEvent* event)
{
    if(p->m_delayedPendingLayout) {
        return;
    }

    const QSize delta = event->size() - event->oldSize();

    if(delta.isNull()) {
        return;
    }

    if(p->m_viewMode == ViewMode::Icon && delta.width() != 0) {
        p->doDelayedItemsLayout(100);
    }
    else {
        QAbstractItemView::resizeEvent(event);
    }
}

void ExpandedTreeView::dropEvent(QDropEvent* event)
{
    if(dragDropMode() == InternalMove && (event->source() != this || !(event->possibleActions() & Qt::MoveAction))) {
        return;
    }

    int col{-1};
    int row{-1};
    QModelIndex index = findIndexAt(event->position().toPoint(), true);

    if(!index.isValid() && p->itemCount() > 0) {
        index = p->m_viewItems.back().index;
    }

    if(p->dropOn(event, row, col, index)) {
        const Qt::DropAction action = dragDropMode() == InternalMove ? Qt::MoveAction : event->dropAction();

        if(p->m_model->dropMimeData(event->mimeData(), action, row, col, index)) {
            if(action != event->dropAction()) {
                event->setDropAction(action);
                event->accept();
            }
            else {
                event->acceptProposedAction();
            }
        }
    }

    p->stopAutoScroll();
    setState(NoState);
    viewport()->update();
}

void ExpandedTreeView::paintEvent(QPaintEvent* event)
{
    p->layoutItems();

    QStylePainter painter{viewport()};

    p->m_view->drawView(&painter, event->region());

    if(!p->m_viewItems.empty() && state() == QAbstractItemView::DraggingState) {
        QStyleOptionFrame opt;
        initStyleOption(&opt);
        opt.rect = p->m_dropIndicatorRect;
        style()->drawPrimitive(QStyle::PE_IndicatorItemViewItemDrop, &opt, &painter);
    }
}

void ExpandedTreeView::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == p->m_delayedLayout.timerId()) {
        p->m_delayedLayout.stop();
        p->m_delayedPendingLayout = false;
        if(isVisible()) {
            doItemsLayout();
        }
    }
    else if(event->timerId() == p->m_autoScrollTimer.timerId()) {
        p->doAutoScroll();
    }
    else if(event->timerId() == p->m_columnResizeTimerId) {
        killTimer(p->m_columnResizeTimerId);
        p->m_columnResizeTimerId = 0;
        updateGeometries();
        viewport()->update();
    }

    QAbstractItemView::timerEvent(event);
}

void ExpandedTreeView::scrollContentsBy(int dx, int dy)
{
    if(dx) {
        p->m_header->setOffset(horizontalScrollBar()->value());
    }

    const int itemHeight = p->m_defaultItemHeight <= 0 ? sizeHintForRow(0) : p->m_defaultItemHeight;
    if(p->m_viewItems.empty() || itemHeight == 0) {
        return;
    }

    // Guess the number of items in the viewport
    const int viewCount = viewport()->height() / itemHeight;
    const int maxDeltaY = std::min(p->itemCount(), viewCount);

    if(std::abs(dy) > std::abs(maxDeltaY)) {
        verticalScrollBar()->update();
        viewport()->update();
        return;
    }

    p->m_scrollDelayOffset = {-dx, -dy};
    scrollDirtyRegion(dx, dy);
    viewport()->scroll(dx, dy);
    p->m_scrollDelayOffset = {0, 0};
}

void ExpandedTreeView::rowsInserted(const QModelIndex& parent, int start, int end)
{
    if(p->m_delayedPendingLayout) {
        QAbstractItemView::rowsInserted(parent, start, end);
        return;
    }

    if(parent.column() != 0 && parent.isValid()) {
        QAbstractItemView::rowsInserted(parent, start, end);
        return;
    }

    const int parentRowCount = model()->rowCount(parent);
    const int delta          = end - start + 1;
    const int parentItem     = p->viewIndex(parent);

    if(parentItem != -1 || parent == rootIndex()) {
        p->doDelayedItemsLayout();
    }
    else if(parentItem != -1 && parentRowCount == delta) {
        p->m_viewItems[parentItem].hasChildren = true;
        viewport()->update();
    }

    QAbstractItemView::rowsInserted(parent, start, end);
}

void ExpandedTreeView::rowsAboutToBeRemoved(const QModelIndex& parent, int start, int end)
{
    p->m_viewItems.clear();
    QAbstractItemView::rowsAboutToBeRemoved(parent, start, end);
}

void ExpandedTreeView::rowsRemoved(const QModelIndex& /*parent*/, int /*first*/, int /*last*/)
{
    p->m_viewItems.clear();
    p->doDelayedItemsLayout();

    setState(QAbstractItemView::NoState);
    updateGeometry();
}

void ExpandedTreeView::startDrag(Qt::DropActions supportedActions)
{
    const QModelIndexList indexes = p->selectedDraggableIndexes(true);
    if(indexes.empty()) {
        return;
    }

    QMimeData* mimeData = p->m_model->mimeData(indexes);
    if(!mimeData) {
        return;
    }

    QRect rect;
    const ItemViewPaintPairs paintPairs = p->m_view->draggablePaintPairs(p->selectedDraggableIndexes(), rect);
    if(paintPairs.empty()) {
        return;
    }

    const qreal dpr = devicePixelRatioF();
    const auto size = rect.size() * dpr;
    QPixmap pixmap{size};
    pixmap.setDevicePixelRatio(dpr);

    pixmap.fill(Qt::transparent);
    QPainter painter{&pixmap};

    p->m_view->renderToPixmap(&painter, paintPairs, rect);
    rect.adjust(horizontalOffset(), verticalOffset(), 0, 0);

    auto* drag = new QDrag(this);
    drag->setPixmap(pixmap);
    drag->setMimeData(mimeData);

    Qt::DropAction dropAction = Qt::IgnoreAction;
    if(supportedActions & defaultDropAction()) {
        dropAction = defaultDropAction();
    }
    else if(supportedActions & Qt::CopyAction) {
        dropAction = Qt::CopyAction;
    }

    drag->exec(supportedActions, dropAction);

    p->m_dropIndicatorRect = {};
    p->m_dropIndicatorPos  = OnViewport;
}

void ExpandedTreeView::verticalScrollbarValueChanged(int value)
{
    QAbstractItemView::verticalScrollbarValueChanged(value);

    if(state() == QAbstractItemView::DraggingState) {
        p->setHoverIndex({});
        return;
    }

    const QPoint pos = viewport()->mapFromGlobal(QCursor::pos());
    if(viewport()->rect().contains(pos)) {
        p->setHoverIndex(indexAt(pos));
    }
}

QAbstractItemView::DropIndicatorPosition ExpandedTreeView::dropPosition(const QPoint& pos, const QRect& rect,
                                                                        const QModelIndex& index)
{
    DropIndicatorPosition dropPos{OnViewport};
    const int margin = std::clamp(static_cast<int>(std::round(static_cast<double>(rect.height()) / 5.5)), 2, 12);

    if(pos.y() - rect.top() < margin) {
        dropPos = QAbstractItemView::AboveItem;
    }
    else if(rect.bottom() - pos.y() < margin) {
        dropPos = QAbstractItemView::BelowItem;
    }
    else if(rect.contains(pos, true)) {
        dropPos = QAbstractItemView::OnItem;
    }

    if(dropPos == QAbstractItemView::OnItem && (!(p->m_model->flags(index) & Qt::ItemIsDropEnabled))) {
        dropPos = pos.y() < rect.center().y() ? QAbstractItemView::AboveItem : QAbstractItemView::BelowItem;
    }

    return dropPos;
}

int ExpandedTreeView::itemCount() const
{
    return p->itemCount();
}

std::set<int> ExpandedTreeView::spans() const
{
    return p->m_spans;
}

void ExpandedTreeView::drawFocus(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index,
                                 int y) const
{
    QStyleOptionFocusRect focusOpt;
    focusOpt.QStyleOption::operator=(option);
    focusOpt.state |= QStyle::State_KeyboardFocusChange;
    const QPalette::ColorGroup cg = (option.state & QStyle::State_Enabled) ? QPalette::Normal : QPalette::Disabled;
    focusOpt.backgroundColor
        = option.palette.color(cg, selectionModel()->isSelected(index) ? QPalette::Highlight : QPalette::Window);

    const auto paintRects = p->rectsToPaint(option, y);

    for(const auto& rect : paintRects) {
        if(rect.width() > 0) {
            focusOpt.rect = QStyle::visualRect(layoutDirection(), viewport()->rect(), rect);
            style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focusOpt, painter);
        }
    }
}

QModelIndex ExpandedTreeView::moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers /*modifiers*/)
{
    p->layoutItems();

    QModelIndex current = currentIndex();
    if(!current.isValid()) {
        const int count = p->m_header->count();
        const int i     = p->m_view->itemBelow(-1);
        int column{0};

        while(column < count && p->m_header->isSectionHidden(p->m_header->logicalIndex(column))) {
            ++column;
        }

        if(i < p->itemCount() && column < count) {
            return p->modelIndex(i, p->m_header->logicalIndex(column));
        }

        return {};
    }

    const int viewIndex = std::max(0, p->viewIndex(current));

    switch(cursorAction) {
        case(MoveNext):
        case(MoveDown):
            return p->modelIndex(p->m_view->itemBelow(viewIndex), current.column());
        case(MovePrevious):
        case(MoveUp):
            return p->modelIndex(p->m_view->itemAbove(viewIndex), current.column());
        case(MovePageUp):
            return p->modelIndex(p->m_view->pageUp(viewIndex), current.column());
        case(MovePageDown):
            return p->modelIndex(p->m_view->pageDown(viewIndex), current.column());
        case(MoveHome):
            return p->modelIndex(p->itemForHomeKey(), current.column());
        case(MoveEnd):
            return p->modelIndex(p->itemForEndKey(), current.column());
        case(MoveLeft):
            return p->modelIndex(p->m_view->itemLeft(viewIndex), current.column());
        case(MoveRight):
            return p->modelIndex(p->m_view->itemRight(viewIndex), current.column());
    }

    return current;
}

int ExpandedTreeView::horizontalOffset() const
{
    return p->m_header->offset();
}

int ExpandedTreeView::verticalOffset() const
{
    return verticalScrollBar()->value();
}

bool ExpandedTreeView::isIndexHidden(const QModelIndex& index) const
{
    return p->m_header->isSectionHidden(index.column());
}

void ExpandedTreeView::setSelection(const QRect& rect, QItemSelectionModel::SelectionFlags command)
{
    if(!selectionModel() || rect.isNull()) {
        return;
    }

    p->layoutItems();

    const QPoint tl{std::min(rect.left(), rect.right()), std::min(rect.top(), rect.bottom())};
    const QPoint br{std::max(rect.left(), rect.right()), std::max(rect.top(), rect.bottom())};

    QModelIndex topLeft     = findIndexAt(tl, false);
    QModelIndex bottomRight = findIndexAt(br, false);

    if(!topLeft.isValid() && !bottomRight.isValid()) {
        if(command & QItemSelectionModel::Clear) {
            selectionModel()->clear();
        }
        return;
    }
    if(!topLeft.isValid() && !p->m_viewItems.empty()) {
        topLeft = p->m_viewItems.front().index;
    }
    if(!bottomRight.isValid() && !p->m_viewItems.empty()) {
        const int column        = p->m_header->logicalIndex(p->m_header->count() - 1);
        const QModelIndex index = p->m_viewItems.back().index;
        bottomRight             = index.sibling(index.row(), column);
    }

    if(!p->isIndexEnabled(topLeft) || !p->isIndexEnabled(bottomRight)) {
        return;
    }

    p->select(topLeft, bottomRight, command);
}

void ExpandedTreeView::currentChanged(const QModelIndex& current, const QModelIndex& previous)
{
    QAbstractItemView::currentChanged(current, previous);

    if(previous.isValid()) {
        viewport()->update(p->m_view->visualRect(previous, RectRule::FullRow, false));
    }
    if(current.isValid()) {
        viewport()->update(p->m_view->visualRect(current, RectRule::FullRow, false));
    }
}

QRegion ExpandedTreeView::visualRegionForSelection(const QItemSelection& selection) const
{
    QRegion selectionRegion;

    const QRect viewportRect = viewport()->rect();

    if(p->m_viewMode == ViewMode::Icon) {
        // We need to make sure the full width is passed,
        // otherwise any sections outside an item's rect won't be updated/drawn
        const auto indexes = selection.indexes();
        for(const auto& index : indexes) {
            auto rect = visualRect(index);
            selectionRegion += QRect{0, rect.y(), viewportRect.width(), rect.height()};
        }
        return selectionRegion;
    }

    for(const auto& range : selection) {
        if(!range.isValid()) {
            continue;
        }

        const QModelIndex parent = range.parent();
        QModelIndex leftIndex    = range.topLeft();
        const int columnCount    = p->m_model->columnCount(parent);

        while(leftIndex.isValid() && isIndexHidden(leftIndex)) {
            if((leftIndex.column() + 1) < columnCount) {
                leftIndex = p->m_model->index(leftIndex.row(), (leftIndex.column() + 1), parent);
            }
            else {
                leftIndex = {};
            }
        }

        if(!leftIndex.isValid()) {
            continue;
        }

        const QRect leftRect   = visualRect(leftIndex);
        int top                = leftRect.top();
        QModelIndex rightIndex = range.bottomRight();

        while(rightIndex.isValid() && isIndexHidden(rightIndex)) {
            if(rightIndex.column() - 1 >= 0) {
                rightIndex = p->m_model->index(rightIndex.row(), rightIndex.column() - 1, parent);
            }
            else {
                rightIndex = {};
            }
        }

        if(!rightIndex.isValid()) {
            continue;
        }

        const QRect rightRect = visualRect(rightIndex);
        int bottom            = rightRect.bottom();

        if(top > bottom) {
            std::swap(top, bottom);
        }

        const int height = bottom - top + 1;

        if(p->m_header->sectionsMoved()) {
            for(int column{range.left()}; column <= range.right(); ++column) {
                const QRect rangeRect{p->m_header->sectionViewportPosition(column), top,
                                      p->m_header->sectionSize(column), height};
                if(viewportRect.intersects(rangeRect)) {
                    selectionRegion += rangeRect;
                }
            }
        }
        else {
            QRect combined = leftRect | rightRect;
            combined.setX(p->m_header->sectionViewportPosition(isRightToLeft() ? range.right() : range.left()));
            if(viewportRect.intersects(combined)) {
                selectionRegion += combined;
            }
        }
    }

    return selectionRegion;
}
} // namespace Fooyin

#include "utils/widgets/expandedtreeview.moc"
#include "utils/widgets/moc_expandedtreeview.cpp"
