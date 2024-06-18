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

#include <utils/widgets/expandedtreeview.h>

#include <QDrag>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QScrollBar>
#include <QStack>
#include <QTimer>
#include <QWindow>

#include <set>

using namespace std::chrono_literals;

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

class ExpandedTreeView::Private : public QObject
{
    Q_OBJECT

public:
    explicit Private(ExpandedTreeView* self);

    int itemCount() const;
    void updateScrollBars();
    int itemAtCoordinate(int coordinate, bool includePadding) const;
    QModelIndex modelIndex(int i, int column = 0) const;
    void select(const QModelIndex& topIndex, const QModelIndex& bottomIndex,
                QItemSelectionModel::SelectionFlags command) const;
    void resizeColumnToContents(int column) const;
    void columnCountChanged(int oldCount, int newCount) const;

    void doDelayedItemsLayout(int delay = 0) const;
    void interruptDelayedItemsLayout() const;
    void layoutItems() const;

    int viewIndex(const QModelIndex& index) const;
    int indexRowSizeHint(const QModelIndex& index) const;
    int indexSizeHint(const QModelIndex& index, bool span = false) const;
    int indexWidthHint(const QModelIndex& index, int hint, const QStyleOptionViewItem& option) const;
    int itemHeight(int item) const;
    int itemPadding(int item) const;
    int coordinateForItem(int item) const;
    void insertViewItems(int pos, int count, const ExpandedTreeViewItem& viewItem);
    bool hasVisibleChildren(const QModelIndex& parent) const;
    void recalculatePadding();
    void layout(int i, bool afterIsUninitialised = false);
    bool isIndexEnabled(const QModelIndex& index) const;
    bool isItemDisabled(int i) const;
    bool itemHasChildren(int i) const;
    int itemAbove(int item) const;
    int itemBelow(int item) const;
    void invalidateHeightCache(int item) const;
    int pageUp(int i) const;
    int pageDown(int i) const;
    int itemForHomeKey() const;
    int itemForEndKey() const;
    void calcLogicalIndexes(std::vector<int>& logicalIndexes,
                            std::vector<QStyleOptionViewItem::ViewItemPosition>& itemPositions, int left,
                            int right) const;
    void setHoverIndex(const QPersistentModelIndex& index);

    bool isIndexDropEnabled(const QModelIndex& index) const;
    QModelIndexList selectedDraggableIndexes(bool fullRow = false) const;
    ItemViewPaintPairs draggablePaintPairs(const QModelIndexList& indexes, QRect& rect) const;
    void adjustViewOptionsForIndex(QStyleOptionViewItem* option, const QModelIndex& currentIndex) const;
    QPixmap renderToPixmap(const QModelIndexList& indexes, QRect& rect) const;
    bool shouldAutoScroll() const;
    void startAutoScroll();
    void stopAutoScroll();
    void doAutoScroll();
    bool dropOn(QDropEvent* event, int& dropRow, int& dropCol, QModelIndex& dropIndex);

    QRect visualRect(const QModelIndex& index, RectRule rule, bool includePadding = true) const;
    std::vector<QRect> rectsToPaint(const QStyleOptionViewItem& option, int y) const;
    void drawAndClipSpans(QPainter* painter, const QStyleOptionViewItem& option, int firstVisibleItem,
                          int firstVisibleItemOffset) const;

    bool isIndexValid(const QModelIndex& index) const;
    int firstVisibleItem(int* offset) const;
    int lastVisibleItem(int firstVisual, int offset) const;
    std::pair<int, int> startAndEndColumns(const QRect& rect) const;

    ExpandedTreeView* m_self;

    QHeaderView* m_header;
    QAbstractItemModel* m_model{nullptr};

    mutable bool m_delayedPendingLayout{false};
    bool m_updatingGeometry{false};
    bool m_hidingScrollbar{false};
    bool m_layingOutItems{false};

    mutable std::vector<ExpandedTreeViewItem> m_viewItems;
    mutable int m_lastViewedItem{0};
    int m_defaultItemHeight{20};

    mutable std::pair<int, int> m_leftAndRight;
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

ExpandedTreeView::Private::Private(ExpandedTreeView* self)
    : m_self{self}
    , m_header{new QHeaderView(Qt::Horizontal, m_self)}
{
    m_header->setSectionsClickable(true);
    m_header->setContextMenuPolicy(Qt::CustomContextMenu);
    m_header->setSectionsMovable(true);
    m_header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    QObject::connect(m_header, &QHeaderView::sectionResized, this, [this]() {
        if(m_columnResizeTimerId == 0) {
            m_columnResizeTimerId = m_self->startTimer(20);
        }
    });
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

void ExpandedTreeView::Private::updateScrollBars()
{
    QSize viewportSize = m_self->viewport()->size();
    if(!viewportSize.isValid()) {
        viewportSize = {0, 0};
    }

    layoutItems();

    if(m_viewItems.empty()) {
        m_self->doItemsLayout();
    }

    int itemsInViewport{0};
    const int itemCount      = this->itemCount();
    const int viewportHeight = viewportSize.height();

    for(int height{0}, item = itemCount - 1; item >= 0; --item) {
        height += itemHeight(item);
        if(height > viewportHeight) {
            break;
        }
        ++itemsInViewport;
    }

    auto* verticalBar = m_self->verticalScrollBar();

    int contentsHeight{0};
    for(int i{0}; i < itemCount; ++i) {
        contentsHeight += itemHeight(i) + itemPadding(i);
    }

    const int vMax = contentsHeight - viewportHeight;
    if(verticalBar->isVisible() && vMax <= 0) {
        m_hidingScrollbar = true;
    }

    verticalBar->setRange(0, vMax);
    verticalBar->setPageStep(viewportHeight);
    verticalBar->setSingleStep(std::max(viewportHeight / (itemsInViewport + 1), 2));

    const int columnCount   = m_header->count();
    const int viewportWidth = viewportSize.width();
    int columnsInViewport{0};

    for(int width{0}, column = columnCount - 1; column >= 0; --column) {
        const int logical = m_header->logicalIndex(column);
        width += m_header->sectionSize(logical);
        if(width > viewportWidth) {
            break;
        }
        ++columnsInViewport;
    }

    if(columnCount > 0) {
        columnsInViewport = std::max(1, columnsInViewport);
    }

    auto* horizontalBar = m_self->horizontalScrollBar();

    const int horizontalLength = m_header->length();
    const QSize maxSize        = m_self->maximumViewportSize();

    if(maxSize.width() >= horizontalLength && verticalBar->maximum() <= 0) {
        viewportSize = maxSize;
    }

    horizontalBar->setPageStep(viewportSize.width());
    horizontalBar->setRange(0, qMax(horizontalLength - viewportSize.width(), 0));
    horizontalBar->setSingleStep(std::max(viewportSize.width() / (columnsInViewport + 1), 2));
}

int ExpandedTreeView::Private::itemAtCoordinate(int coordinate, bool includePadding) const
{
    const int itemCount = this->itemCount();
    if(itemCount == 0) {
        return -1;
    }

    const int contentsCoord = coordinate + m_self->verticalScrollBar()->value();

    int itemCoord{0};
    for(int index{0}; index < itemCount; ++index) {
        const int height  = itemHeight(index);
        const int padding = itemPadding(index);
        itemCoord += height + padding;

        if(itemCoord > contentsCoord) {
            if(includePadding && (itemCoord - padding) < contentsCoord) {
                return -1;
            }
            return index >= itemCount ? -1 : index;
        }
    }

    return -1;
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

int ExpandedTreeView::Private::indexRowSizeHint(const QModelIndex& index) const
{
    if(!isIndexValid(index) || !m_self->itemDelegate()) {
        return 0;
    }

    int start{-1};
    int end{-1};
    const int indexRow       = index.row();
    const QModelIndex parent = index.parent();
    int count                = m_header->count();

    if(count > 0 && m_self->isVisible()) {
        start = std::min(m_header->visualIndexAt(0), 0);
    }
    else {
        count = m_model->columnCount(parent);
    }

    end = count - 1;

    if(end < start) {
        std::swap(end, start);
    }

    QStyleOptionViewItem opt;
    m_self->initViewItemOption(&opt);

    opt.rect.setWidth(-1);
    int height{-1};

    for(int column{start}; column <= end; ++column) {
        const int logical = count == 0 ? column : m_header->logicalIndex(column);
        if(m_header->isSectionHidden(logical)) {
            continue;
        }

        const QModelIndex colIndex = m_model->index(indexRow, logical, parent);
        if(colIndex.isValid()) {
            const int hint = m_self->itemDelegateForIndex(colIndex)->sizeHint(opt, colIndex).height();
            height         = std::max(height, hint);
        }
    }

    return height;
}

int ExpandedTreeView::Private::indexSizeHint(const QModelIndex& index, bool span) const
{
    if(!isIndexValid(index) || !m_self->itemDelegate()) {
        return 0;
    }

    QStyleOptionViewItem opt;
    m_self->initViewItemOption(&opt);

    int height{0};
    if(span && m_self->isSpanning(index.column())) {
        height = m_header->sectionSize(index.column());
    }

    const int column = index.column();
    if(m_header->isSectionHidden(column)) {
        return 0;
    }

    opt.rect.setWidth(m_header->sectionSize(column));
    const int hint = m_self->itemDelegateForIndex(index)->sizeHint(opt, index).height();
    height         = std::max(height, hint);

    return height;
}

int ExpandedTreeView::Private::indexWidthHint(const QModelIndex& index, int hint,
                                              const QStyleOptionViewItem& option) const
{
    const int xHint = m_self->itemDelegateForIndex(index)->sizeHint(option, index).width();
    return std::max(hint, xHint);
}

int ExpandedTreeView::Private::itemHeight(int item) const
{
    if(m_viewItems.empty()) {
        return 0;
    }

    const QModelIndex& index = m_viewItems.at(item).index;
    if(!index.isValid()) {
        return 0;
    }

    int height = m_viewItems.at(item).height;
    if(height <= 0) {
        height                   = indexRowSizeHint(index);
        m_viewItems[item].height = height;
    }

    return std::max(height, 0);
}

int ExpandedTreeView::Private::itemPadding(int item) const
{
    if(m_viewItems.empty()) {
        return 0;
    }

    return m_viewItems.at(item).padding;
}

int ExpandedTreeView::Private::coordinateForItem(int item) const
{
    for(int index{0}, y{0}; const auto& viewItem : m_viewItems) {
        if(index == item) {
            return y - m_self->verticalScrollBar()->value();
        }
        y += itemHeight(index++) + viewItem.padding;
    }

    return 0;
}

void ExpandedTreeView::Private::insertViewItems(int pos, int count, const ExpandedTreeViewItem& viewItem)
{
    m_viewItems.insert(m_viewItems.begin() + pos, count, viewItem);

    const int itemCount = this->itemCount();
    for(int i{pos + count}; i < itemCount; i++) {
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

void ExpandedTreeView::Private::recalculatePadding()
{
    if(m_viewItems.empty()) {
        return;
    }

    int max{0};

    const int sectionCount = m_header->count();
    for(int section{0}; section < sectionCount; ++section) {
        const auto columnIndex = m_model->index(0, section, {});
        if(columnIndex.isValid()) {
            if(m_self->isSpanning(section)) {
                max = std::max(max, m_header->sectionSize(section));
            }
        }
    }

    int rowHeight{0};

    for(auto& item : m_viewItems) {
        item.padding = 0;

        if(item.hasChildren || item.parentItem == -1) {
            continue;
        }

        const QModelIndex parent = modelIndex(item.parentItem);
        const int rowCount       = m_model->rowCount(parent);
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

void ExpandedTreeView::Private::layout(int i, bool afterIsUninitialised)
{
    const QModelIndex parent = i < 0 ? m_self->rootIndex() : modelIndex(i);

    if(i >= 0 && !parent.isValid()) {
        return;
    }

    int count{0};
    if(m_model->hasChildren(parent)) {
        if(m_model->canFetchMore(parent)) {
            m_model->fetchMore(parent);
            const int itemHeight = m_defaultItemHeight <= 0 ? m_self->sizeHintForRow(0) : m_defaultItemHeight;
            const int viewCount  = itemHeight ? m_self->viewport()->height() / itemHeight : 0;

            int lastCount{-1};
            while((count = m_model->rowCount(parent)) < viewCount && count != lastCount
                  && m_model->canFetchMore(parent)) {
                m_model->fetchMore(parent);
                lastCount = count;
            }
        }
        else {
            count = m_model->rowCount(parent);
        }
    }

    if(i == -1) {
        m_viewItems.resize(count);
        afterIsUninitialised = true;
    }
    else if(m_viewItems[i].childCount != count) {
        if(!afterIsUninitialised) {
            insertViewItems(i + 1, count, ExpandedTreeViewItem{});
        }
        else if(count > 0) {
            m_viewItems.resize(itemCount() + count);
        }
    }

    const int first{i + 1};
    int children{0};
    ExpandedTreeViewItem* item{nullptr};
    QModelIndex currentIndex;

    for(int index{first}; index < first + count; ++index) {
        currentIndex = m_model->index(index - first, 0, parent);

        const int last = index + children;

        if(item) {
            item->hasMoreSiblings = true;
        }

        item                  = &m_viewItems[last];
        item->index           = currentIndex;
        item->parentItem      = i;
        item->height          = 0;
        item->childCount      = 0;
        item->hasMoreSiblings = false;
        item->hasChildren     = m_model->hasChildren(currentIndex);

        if(item->hasChildren) {
            layout(last, afterIsUninitialised);
            item = &m_viewItems[last];
            children += item->childCount;
        }
    }

    while(i > -1) {
        m_viewItems[i].childCount += count;
        i = m_viewItems[i].parentItem;
    }
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

int ExpandedTreeView::Private::itemAbove(int item) const
{
    const int i{item};
    while(isItemDisabled(--item) || itemHasChildren(item)) { }
    return item < 0 ? i : item;
}

int ExpandedTreeView::Private::itemBelow(int item) const
{
    const int i{item};
    while(isItemDisabled(++item) || itemHasChildren(item)) { }
    return item >= itemCount() ? i : item;
}

void ExpandedTreeView::Private::invalidateHeightCache(int item) const
{
    m_viewItems[item].height = 0;
}

int ExpandedTreeView::Private::pageUp(int i) const
{
    int index = itemAtCoordinate(coordinateForItem(i) - m_self->viewport()->height(), false);

    while(isItemDisabled(index) || itemHasChildren(index)) {
        index--;
    }

    if(index == -1) {
        index = 0;
    }

    while(isItemDisabled(index) || itemHasChildren(index)) {
        index++;
    }

    return index >= itemCount() ? 0 : index;
}

int ExpandedTreeView::Private::pageDown(int i) const
{
    int index = itemAtCoordinate(coordinateForItem(i) + m_self->viewport()->height(), false);

    while(isItemDisabled(index) || itemHasChildren(index)) {
        index++;
    }

    if(index == -1 || index >= itemCount()) {
        index = itemCount() - 1;
    }

    while(isItemDisabled(index) || itemHasChildren(index)) {
        index--;
    }

    return index == -1 ? itemCount() - 1 : index;
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

void ExpandedTreeView::Private::calcLogicalIndexes(std::vector<int>& logicalIndexes,
                                                   std::vector<QStyleOptionViewItem::ViewItemPosition>& itemPositions,
                                                   int left, int right) const
{
    const int columnCount = m_header->count();

    int logicalIndexBeforeLeft{-1};
    int logicalIndexAfterRight{-1};

    for(int visualIndex{left - 1}; visualIndex >= 0; --visualIndex) {
        const int logical = m_header->logicalIndex(visualIndex);
        if(!m_header->isSectionHidden(logical)) {
            logicalIndexBeforeLeft = logical;
            break;
        }
    }

    for(int visualIndex{left}; visualIndex < columnCount; ++visualIndex) {
        const int logical = m_header->logicalIndex(visualIndex);
        if(!m_header->isSectionHidden(logical)) {
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
           || m_self->isSpanning(logical)) {
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

void ExpandedTreeView::Private::setHoverIndex(const QPersistentModelIndex& index)
{
    if(m_hoverIndex == index) {
        return;
    }

    auto* viewport = m_self->viewport();

    const QRect oldHoverRect = visualRect(m_hoverIndex, RectRule::FullRow, false);
    const QRect newHoverRect = visualRect(index, RectRule::FullRow, false);

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

ItemViewPaintPairs ExpandedTreeView::Private::draggablePaintPairs(const QModelIndexList& indexes, QRect& rect) const
{
    ItemViewPaintPairs ret;

    const QRect viewportRect = m_self->viewport()->rect();

    for(const auto& index : indexes) {
        if(!index.isValid() || m_self->isSpanning(index.column())) {
            continue;
        }

        const QRect currentRect = m_self->visualRect(index);
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

void ExpandedTreeView::Private::adjustViewOptionsForIndex(QStyleOptionViewItem* option,
                                                          const QModelIndex& currentIndex) const
{
    const int row = viewIndex(currentIndex);
    option->state = option->state | QStyle::State_Open
                  | (m_viewItems.at(row).hasChildren ? QStyle::State_Children : QStyle::State_None)
                  | (m_viewItems.at(row).hasMoreSiblings ? QStyle::State_Sibling : QStyle::State_None);

    std::vector<int> logicalIndexes;
    std::vector<QStyleOptionViewItem::ViewItemPosition> viewItemPosList;

    const bool isSpanning = m_self->isSpanning(currentIndex.column());
    const int left        = (isSpanning ? m_header->visualIndex(0) : 0);
    const int right       = (isSpanning ? m_header->visualIndex(0) : m_header->count() - 1);

    calcLogicalIndexes(logicalIndexes, viewItemPosList, left, right);

    const int visualIndex = static_cast<int>(
        std::ranges::distance(logicalIndexes.cbegin(), std::ranges::find(logicalIndexes, currentIndex.column())));
    option->viewItemPosition = viewItemPosList.at(visualIndex);
}

QPixmap ExpandedTreeView::Private::renderToPixmap(const QModelIndexList& indexes, QRect& rect) const
{
    const ItemViewPaintPairs paintPairs = draggablePaintPairs(indexes, rect);
    if(paintPairs.empty()) {
        return {};
    }

    QWindow* window    = m_self->windowHandle();
    const double scale = window ? window->devicePixelRatio() : 1.0;

    QPixmap pixmap(rect.size() * scale);
    pixmap.setDevicePixelRatio(scale);

    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);

    QStyleOptionViewItem opt;
    m_self->initViewItemOption(&opt);

    opt.state |= QStyle::State_Selected;

    int lastRow{0};
    bool paintedBg{false};
    for(const auto& [paintRect, index] : paintPairs) {
        if(m_self->isSpanning(index.column())) {
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

            opt.rect.setRect(0, opt.rect.top(), m_header->length(), cellHeight);

            const auto bg = index.data(Qt::BackgroundRole).value<QBrush>();
            if(bg != Qt::NoBrush) {
                painter.fillRect(opt.rect, bg);
            }
            m_self->style()->drawControl(QStyle::CE_ItemViewItem, &opt, &painter, m_self);

            opt.rect = cellRect;
        }

        m_self->itemDelegateForIndex(index)->paint(&painter, opt, index);
    }

    return pixmap;
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
        m_dropIndicatorPos = m_self->dropPosition(pos, visualRect(index, RectRule::FullRow, false), index);
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

QRect ExpandedTreeView::Private::visualRect(const QModelIndex& index, RectRule rule, bool includePadding) const
{
    if(!isIndexValid(index)) {
        return {};
    }

    if(m_self->isIndexHidden(index) && rule != RectRule::FullRow) {
        return {};
    }

    layoutItems();

    const int viewIndex = this->viewIndex(index);
    if(viewIndex < 0) {
        return {};
    }

    const int column = index.column();
    int x            = m_header->sectionViewportPosition(column);
    int width        = m_header->sectionSize(column);

    if(rule == RectRule::FullRow) {
        x     = 0;
        width = m_header->length();
    }

    const int y = coordinateForItem(viewIndex);
    int height  = indexSizeHint(index);

    if(includePadding) {
        height += itemPadding(viewIndex);
    }

    return {x, y, width, height};
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

void ExpandedTreeView::Private::drawAndClipSpans(QPainter* painter, const QStyleOptionViewItem& option,
                                                 int firstVisibleItem, int firstVisibleItemOffset) const
{
    QStyleOptionViewItem opt{option};

    const QRect rect = m_self->viewport()->rect();
    QRegion region{rect};
    const int count = itemCount();

    int i{firstVisibleItem};
    int y{firstVisibleItemOffset};

    for(; i < count; ++i) {
        const int itemHeight = this->itemHeight(i) + itemPadding(i);
        if(y + itemHeight > rect.top()) {
            break;
        }
        y += itemHeight;
    }

    for(; i < count && y <= rect.bottom(); ++i) {
        const auto& item  = m_viewItems.at(i);
        QModelIndex index = item.index;

        if(!index.isValid() || m_model->hasChildren(index)) {
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
            const bool spanning     = m_self->isSpanning(headerSection);

            if(!spanning) {
                continue;
            }

            const int width = m_header->sectionSize(headerSection);
            position        = m_header->sectionViewportPosition(headerSection);

            modelIndex = m_model->index(0, headerSection, parent);
            if(!modelIndex.isValid()) {
                continue;
            }

            opt.rect = {position, y, width, indexSizeHint(modelIndex, true)};
            m_self->itemDelegateForIndex(modelIndex)->paint(painter, opt, modelIndex);
            region -= opt.rect;
        }

        y += itemHeight(i) + item.padding;
    }

    painter->setClipRegion(region);
}

bool ExpandedTreeView::Private::isIndexValid(const QModelIndex& index) const
{
    return (index.row() >= 0) && (index.column() >= 0) && (index.model() == m_model);
}

int ExpandedTreeView::Private::firstVisibleItem(int* offset) const
{
    const int value = m_self->verticalScrollBar()->value();

    int y{0};
    for(int i{0}; i < itemCount(); ++i) {
        const int itemHeight = this->itemHeight(i) + itemPadding(i);
        y += itemHeight;
        if(y > value) {
            if(offset) {
                *offset = y - value - itemHeight;
            }
            return i;
        }
    }
    return -1;
}

int ExpandedTreeView::Private::lastVisibleItem(int firstVisual, int offset) const
{
    if(firstVisual < 0 || offset < 0) {
        firstVisual = firstVisibleItem(&offset);
        if(firstVisual < 0) {
            return -1;
        }
    }

    int y{-offset};
    const int value = m_self->viewport()->height();

    const int count = itemCount();
    for(int i{firstVisual}; i < count; ++i) {
        const int itemHeight = this->itemHeight(i) + itemPadding(i);
        y += itemHeight;
        if(y > value) {
            return i;
        }
    }

    return count - 1;
}

std::pair<int, int> ExpandedTreeView::Private::startAndEndColumns(const QRect& rect) const
{
    const int start = std::min(m_header->visualIndexAt(rect.left()), 0);
    int end         = m_header->visualIndexAt(rect.right());

    if(end == -1) {
        end = m_header->count() - 1;
    }

    return {std::min(start, end), std::max(start, end)};
}

ExpandedTreeView::ExpandedTreeView(QWidget* parent)
    : QAbstractItemView{parent}
    , p{std::make_unique<Private>(this)}
{
    setObjectName(QStringLiteral("ExpandedTreeView"));

    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setMouseTracking(true);
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

    QObject::connect(p->m_header, &QHeaderView::sectionResized, this, [this]() {
        if(p->m_columnResizeTimerId == 0) {
            p->m_columnResizeTimerId = startTimer(20);
        }
    });
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

QRect ExpandedTreeView::visualRect(const QModelIndex& index) const
{
    return p->visualRect(index, RectRule::SingleSection);
}

int ExpandedTreeView::sizeHintForColumn(int column) const
{
    p->layoutItems();

    if(p->m_viewItems.empty()) {
        return -1;
    }

    ensurePolished();

    QStyleOptionViewItem option;
    initViewItemOption(&option);
    const auto viewItems = p->m_viewItems;
    const int itemCount  = p->itemCount();

    const int maximumProcessRows = p->m_header->resizeContentsPrecision();

    int offset{0};
    int start = p->firstVisibleItem(&offset);
    int end   = p->lastVisibleItem(start, offset);
    if(start < 0 || end < 0 || end == itemCount - 1) {
        end = itemCount - 1;
        if(maximumProcessRows < 0) {
            start = 0;
        }
        else if(maximumProcessRows == 0) {
            start               = std::max(0, end - 1);
            int remainingHeight = viewport()->height();
            while(start > 0 && remainingHeight > 0) {
                remainingHeight -= p->itemHeight(start);
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
        if(viewItems.at(i).hasChildren) {
            continue;
        }
        QModelIndex index = viewItems.at(i).index;
        index             = index.sibling(index.row(), column);
        width             = p->indexWidthHint(index, width, option);
        ++rowsProcessed;
        if(rowsProcessed == maximumProcessRows) {
            break;
        }
    }

    --end;

    const int actualBottom = itemCount - 1;

    if(maximumProcessRows == 0) {
        rowsProcessed = 0;
    }

    while(rowsProcessed != maximumProcessRows && (start > 0 || end < actualBottom)) {
        int index{-1};

        if((rowsProcessed % 2 && start > 0) || end == actualBottom) {
            while(start > 0) {
                --start;
                if(viewItems.at(start).hasChildren) {
                    continue;
                }
                index = start;
                break;
            }
        }
        else {
            while(end < actualBottom) {
                ++end;
                if(viewItems.at(end).hasChildren) {
                    continue;
                }
                index = end;
                break;
            }
        }
        if(index < 0) {
            continue;
        }

        QModelIndex viewIndex = viewItems.at(index).index;
        viewIndex             = viewIndex.sibling(viewIndex.row(), column);
        width                 = p->indexWidthHint(viewIndex, width, option);
        ++rowsProcessed;
    }

    return width;
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
    p->updateScrollBars();

    const int item = p->viewIndex(index);
    if(item < 0) {
        return;
    }

    const QRect area = viewport()->rect();
    const QRect rect{p->m_header->sectionViewportPosition(index.column()), p->coordinateForItem(item),
                     p->m_header->sectionSize(index.column()), p->itemHeight(item)};

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

    const int visualIndex = p->itemAtCoordinate(point.y(), includePadding);
    QModelIndex index     = p->modelIndex(visualIndex);
    if(!index.isValid()) {
        return {};
    }

    const int column = p->m_header->logicalIndexAt(point.x());

    if(!includeSpans && !p->m_model->hasChildren(index) && isSpanning(column)) {
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
        p->layout(-1);
        p->recalculatePadding();
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
    p->updateScrollBars();
    p->m_updatingGeometry = false;

    QAbstractItemView::updateGeometries();
}

void ExpandedTreeView::dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles)
{
    bool sizeChanged{false};
    const int topViewIndex = p->viewIndex(topLeft);

    if(topViewIndex == 0) {
        const int defaultHeight = p->indexRowSizeHint(topLeft);
        sizeChanged             = p->m_defaultItemHeight != defaultHeight;
        p->m_defaultItemHeight  = defaultHeight;
    }

    if(topViewIndex != -1) {
        // Single row
        if(topLeft.row() == bottomRight.row()) {
            const int oldHeight = p->itemHeight(topViewIndex);
            p->invalidateHeightCache(topViewIndex);
            sizeChanged |= (oldHeight != p->itemHeight(topViewIndex));
            if(topLeft.column() == 0) {
                p->m_viewItems[topViewIndex].hasChildren = p->hasVisibleChildren(topLeft);
            }
        }
        else {
            const int bottomViewIndex = p->viewIndex(bottomRight);
            for(int i = topViewIndex; i <= bottomViewIndex; ++i) {
                const int oldHeight = p->itemHeight(i);
                p->invalidateHeightCache(i);
                sizeChanged |= (oldHeight != p->itemHeight(i));
                if(topLeft.column() == 0) {
                    p->m_viewItems[i].hasChildren = p->hasVisibleChildren(p->m_viewItems.at(i).index);
                }
            }
        }
    }

    if(sizeChanged) {
        p->updateScrollBars();
        viewport()->update();
    }

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
    }

    if(index.isValid() && showDropIndicator()) {
        const QRect rect      = p->visualRect(index, RectRule::FullRow, false);
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

    QPainter painter{viewport()};

    drawTree(&painter, event->region());

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
        if(p->m_hidingScrollbar) {
            p->m_hidingScrollbar = false;
            return;
        }
        p->recalculatePadding();
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
    const QPixmap pixmap = p->renderToPixmap(p->selectedDraggableIndexes(), rect);
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

std::vector<ExpandedTreeViewItem> ExpandedTreeView::viewItems() const
{
    return p->m_viewItems;
}

int ExpandedTreeView::itemCount() const
{
    return p->itemCount();
}

std::set<int> ExpandedTreeView::spans() const
{
    return p->m_spans;
}

void ExpandedTreeView::drawTree(QPainter* painter, const QRegion& region) const
{
    QStyleOptionViewItem opt;
    initViewItemOption(&opt);

    if(p->m_viewItems.empty() || p->m_header->count() == 0) {
        return;
    }

    int firstVisibleItemOffset{0};
    const int firstVisibleItem = p->firstVisibleItem(&firstVisibleItemOffset);
    if(firstVisibleItem < 0) {
        return;
    }

    if(!p->m_spans.empty()) {
        p->drawAndClipSpans(painter, opt, firstVisibleItem, firstVisibleItemOffset);
    }

    const int count          = itemCount();
    const int viewportWidth  = viewport()->width();
    const bool multipleRects = (region.rectCount() > 1);
    std::set<int> drawn;

    for(const QRect& regionArea : region) {
        const QRect area  = multipleRects ? QRect{0, regionArea.y(), viewportWidth, regionArea.height()} : regionArea;
        p->m_leftAndRight = p->startAndEndColumns(area);

        int i{firstVisibleItem};
        int y{firstVisibleItemOffset};

        for(; i < count; ++i) {
            const int itemHeight = p->itemHeight(i) + p->itemPadding(i);
            if(y + itemHeight > area.top()) {
                break;
            }
            y += itemHeight;
        }

        for(; i < count && y <= area.bottom(); ++i) {
            const auto& item         = p->m_viewItems.at(i);
            const QModelIndex& index = item.index;

            opt.rect = p->visualRect(index, RectRule::FullRow, false);
            opt.rect.setY(y);
            opt.rect.setHeight(p->indexRowSizeHint(index));
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

            y += p->itemHeight(i) + item.padding;
        }
    }
}

void ExpandedTreeView::drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if(!index.isValid()) {
        return;
    }

    QStyleOptionViewItem opt{option};

    const QModelIndex hover = p->m_hoverIndex;
    const bool hoverRow     = index.parent() == hover.parent() && index.row() == hover.row();
    const bool rowFocused   = hasFocus();

    opt.state.setFlag(QStyle::State_MouseOver, hoverRow);

    if(model()->hasChildren(index)) {
        // Span first column of headers/subheaders
        opt.rect.setX(0);
        opt.rect.setWidth(p->m_header->length());
        style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, this);
        itemDelegateForIndex(index)->paint(painter, opt, index);
        return;
    }

    const QPoint offset       = p->m_scrollDelayOffset;
    const int y               = opt.rect.y() + offset.y();
    const int left            = p->m_leftAndRight.first;
    const int right           = p->m_leftAndRight.second;
    const QModelIndex parent  = index.parent();
    const QModelIndex current = currentIndex();

    QModelIndex modelIndex;
    std::vector<int> logicalIndices;
    std::vector<QStyleOptionViewItem::ViewItemPosition> viewItemPosList;

    p->calcLogicalIndexes(logicalIndices, viewItemPosList, left, right);

    const auto count = static_cast<int>(logicalIndices.size());
    bool paintedBg{false};
    bool currentRowHasFocus{false};

    for(int section{0}; section < count; ++section) {
        const int headerSection = logicalIndices.at(section);
        const bool spanning     = isSpanning(headerSection);

        if(spanning) {
            continue;
        }

        const int width    = p->m_header->sectionSize(headerSection);
        const int position = p->m_header->sectionViewportPosition(headerSection) + offset.x();

        modelIndex = model()->index(index.row(), headerSection, parent);

        if(!modelIndex.isValid()) {
            continue;
        }

        const auto icon      = index.data(Qt::DecorationRole).value<QPixmap>();
        opt.icon             = QIcon{icon};
        opt.decorationSize   = icon.deviceIndependentSize().toSize();
        opt.viewItemPosition = viewItemPosList.at(section);

        if(selectionModel()->isSelected(modelIndex)) {
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

        const int cellHeight = p->indexRowSizeHint(modelIndex);
        if(cellHeight > 0) {
            const QRect cellRect{position, y, width, cellHeight};

            if(!paintedBg) {
                // Some styles use a gradient for the selection bg which only covers each individual cell
                // Show this as a single gradient across the entire row
                paintedBg = true;
                drawRowBackground(painter, opt, modelIndex, y);
            }

            opt.rect = cellRect;

            itemDelegateForIndex(modelIndex)->paint(painter, opt, modelIndex);
        }
    }

    if(currentRowHasFocus) {
        drawFocus(painter, opt, modelIndex, y);
    }
}

void ExpandedTreeView::drawRowBackground(QPainter* painter, const QStyleOptionViewItem& option,
                                         const QModelIndex& index, int y) const
{
    QStyleOptionViewItem opt{option};

    const auto bg = index.data(Qt::BackgroundRole).value<QBrush>();

    const auto paintRects = p->rectsToPaint(option, y);
    for(const auto& rect : paintRects) {
        if(rect.width() > 0) {
            opt.rect = rect;
            painter->fillRect(opt.rect, bg);
            style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, this);
        }
    }
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
        const int i     = p->itemBelow(-1);
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
            return p->modelIndex(p->itemBelow(viewIndex), current.column());
        case(MovePrevious):
        case(MoveUp):
            return p->modelIndex(p->itemAbove(viewIndex), current.column());
        case(MovePageUp): {
            return p->modelIndex(p->pageUp(viewIndex), current.column());
        }
        case(MovePageDown): {
            return p->modelIndex(p->pageDown(viewIndex), current.column());
        }
        case(MoveHome): {
            return p->modelIndex(p->itemForHomeKey(), current.column());
        }
        case(MoveEnd): {
            return p->modelIndex(p->itemForEndKey(), current.column());
        }
        case(MoveLeft):
        case(MoveRight):
            break;
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
        viewport()->update(p->visualRect(previous, RectRule::FullRow, false));
    }
    if(current.isValid()) {
        viewport()->update(p->visualRect(current, RectRule::FullRow, false));
    }
}

QRegion ExpandedTreeView::visualRegionForSelection(const QItemSelection& selection) const
{
    QRegion selectionRegion;

    const QRect viewportRect = viewport()->rect();

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
