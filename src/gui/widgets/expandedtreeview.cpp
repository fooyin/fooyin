/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <gui/widgets/expandedtreeview.h>

#include "expandedtreeview_p.h"

#include <gui/contextmenuutils.h>
#include <gui/widgets/widgetbackground.h>
#include <utils/scopeguard.h>
#include <utils/treemodel.h>
#include <utils/utils.h>

#include <QDrag>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStack>
#include <QStylePainter>
#include <QTimer>
#include <QWheelEvent>

#include <deque>
#include <limits>
#include <numeric>
#include <ranges>
#include <set>
#include <span>
#include <unordered_set>

using namespace Qt::StringLiterals;

constexpr auto MinItemSpacing    = 10;
constexpr auto IconRowSpacing    = 10;
constexpr auto RightCaptionWidth = 180;

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

struct ItemViewPaintPair
{
    QRect rect;
    QModelIndex index;
};
using ItemViewPaintPairs = std::vector<ItemViewPaintPair>;

enum class RectRule : uint8_t
{
    FullRow = 0,
    SingleSection,
};

struct FlowLayoutItem
{
    QSize size;
    int children{0};
};

struct FlowLayoutColumn
{
    QRect rect;
    int first{0};
    int last{0};
};

struct FlowLayoutBlock
{
    QRect rect;
    std::vector<FlowLayoutColumn> columns;
};

struct FlowLayout
{
    QSize size;
    std::vector<QRect> rects;
    std::vector<int> columns;
    std::vector<int> blocks;
    std::vector<int> headers;
    std::vector<FlowLayoutBlock> sections;
};

struct FlowLayoutOptions
{
    int width{1};
    int gap{16};
    int minimumRows{1};
    int maximumColumns{0};
    int minimumColumnWidth{0};
    bool stretchColumns{true};
    bool rightToLeft{false};
};

class FlowLayoutBuilder
{
public:
    FlowLayoutBuilder(std::span<const FlowLayoutItem> items, const FlowLayoutOptions& options);

    FlowLayout build();

private:
    int layoutRange(int first, int last, int y);
    int layoutBlock(int first, int last, int y);

    std::span<const FlowLayoutItem> m_items;
    int m_width;
    int m_gap;
    int m_minimumRows;
    bool m_stretchColumns;
    bool m_rightToLeft;
    int m_columnWidth;
    int m_columns{1};
    FlowLayout m_result;
};

FlowLayoutBuilder::FlowLayoutBuilder(std::span<const FlowLayoutItem> items, const FlowLayoutOptions& options)
    : m_items{items}
    , m_width{std::max(1, options.width)}
    , m_gap{std::max(0, options.gap)}
    , m_minimumRows{std::max(1, options.minimumRows)}
    , m_stretchColumns{options.stretchColumns}
    , m_rightToLeft{options.rightToLeft}
    , m_columnWidth{std::max(1, options.minimumColumnWidth)}
{
    int longestBlock{1};
    int run{0};

    for(const auto& item : items) {
        if(item.children == 0) {
            m_columnWidth = std::max(m_columnWidth, item.size.width());
            longestBlock  = std::max(longestBlock, ++run);
        }
        else {
            run = 0;
        }
    }

    const int rowLimit   = (longestBlock + m_minimumRows - 1) / m_minimumRows;
    const int widthLimit = std::max(1, (m_width + m_gap) / (m_columnWidth + m_gap));

    m_columns = std::min(rowLimit, widthLimit);

    if(options.maximumColumns > 0) {
        m_columns = std::min(m_columns, options.maximumColumns);
    }

    m_result.rects.resize(items.size());
    m_result.columns.assign(items.size(), -1);
    m_result.blocks.assign(items.size(), -1);
}

FlowLayout FlowLayoutBuilder::build()
{
    const int height = layoutRange(0, static_cast<int>(m_items.size()), 0);
    m_result.size    = {m_width, height};
    return std::move(m_result);
}

int FlowLayoutBuilder::layoutRange(int first, int last, int y)
{
    while(first < last) {
        const auto& item = m_items[first];
        if(item.children > 0) {
            const int height = std::max(0, item.size.height());

            m_result.rects[first] = {0, y, m_width, height};
            if(height > 0) {
                m_result.headers.push_back(first);
            }

            const int end = std::min(last, first + item.children + 1);
            y             = layoutRange(first + 1, end, y + height);

            if(!m_stretchColumns) {
                int contentWidth = std::max(1, item.size.width());
                for(int child{first + 1}; child < end; ++child) {
                    const auto& rect = m_result.rects[child];
                    contentWidth     = std::max(contentWidth, m_rightToLeft ? m_width - rect.left() : rect.right() + 1);
                }

                const int width = std::min(m_width, contentWidth);
                auto& rect      = m_result.rects[first];
                rect.setRect(m_rightToLeft ? m_width - width : 0, rect.y(), width, height);
            }

            first = end;
        }
        else {
            int end{first + 1};
            while(end < last && m_items[end].children == 0) {
                ++end;
            }
            y     = layoutBlock(first, end, y);
            first = end;
        }
    }

    return y;
}

int FlowLayoutBuilder::layoutBlock(int first, int last, int y)
{
    const int count = last - first;

    std::vector<int> prefixHeights(count + 1);
    std::vector<int> columnCounts(count + 1);
    std::vector<int> columnStarts(count + 1);
    std::deque<int> candidates;

    int minHeight{0};
    for(int i{0}; i < count; ++i) {
        const int height     = std::max(0, m_items[first + i].size.height());
        minHeight            = std::max(minHeight, height);
        prefixHeights[i + 1] = prefixHeights[i] + height;
    }
    int maxHeight     = prefixHeights[count];
    const int columns = std::min(m_columns, (count + m_minimumRows - 1) / m_minimumRows);

    const auto requiredColumns = [&](int limit) {
        candidates.clear();
        int nextStart{0};

        for(int end{1}; end <= count; ++end) {
            // Only the final column may contain fewer than the min number of rows
            const int minimumRows = end == count ? 1 : m_minimumRows;
            while(nextStart <= end - minimumRows) {
                if(columnCounts[nextStart] < columns) {
                    while(!candidates.empty() && columnCounts[candidates.back()] >= columnCounts[nextStart]) {
                        candidates.pop_back();
                    }
                    candidates.push_back(nextStart);
                }
                ++nextStart;
            }

            while(!candidates.empty() && prefixHeights[end] - prefixHeights[candidates.front()] > limit) {
                candidates.pop_front();
            }

            columnCounts[end] = columns + 1;
            if(!candidates.empty()) {
                columnStarts[end] = candidates.front();
                columnCounts[end] = columnCounts[columnStarts[end]] + 1;
            }
        }

        return columnCounts[count];
    };

    while(minHeight < maxHeight) {
        const int limit = std::midpoint(minHeight, maxHeight);
        if(requiredColumns(limit) <= columns) {
            maxHeight = limit;
        }
        else {
            minHeight = limit + 1;
        }
    }

    const int usedColumns = requiredColumns(minHeight);
    std::vector<int> columnEnds(usedColumns);
    int end{count};
    for(int column = usedColumns - 1; column >= 0; --column) {
        columnEnds[column] = first + end;
        end                = columnStarts[end];
    }

    const int block = static_cast<int>(m_result.sections.size());
    m_result.sections.push_back({});

    const int availableWidth = std::max(1, (m_width - ((m_columns - 1) * m_gap)) / m_columns);
    const int columnWidth    = m_stretchColumns ? availableWidth : std::min(availableWidth, m_columnWidth);

    int bottom{y};
    int next{first};
    for(int column{0}; column < usedColumns; ++column) {
        const int logicalX = column * (columnWidth + m_gap);
        const int x        = m_rightToLeft ? m_width - logicalX - columnWidth : logicalX;

        int height{0};
        const int start{next};
        while(next < columnEnds[column]) {
            const int rowHeight = std::max(0, m_items[next].size.height());

            m_result.rects[next]   = {x, y + height, columnWidth, rowHeight};
            m_result.columns[next] = column;
            m_result.blocks[next]  = block;
            height += rowHeight;

            ++next;
        }

        m_result.sections[block].columns.push_back({.rect = {x, y, columnWidth, height}, .first = start, .last = next});
        bottom = std::max(bottom, y + height);
    }

    m_result.sections[block].rect = {0, y, m_width, bottom - y};
    return bottom;
}
} // namespace

namespace Fooyin {
class BaseView
{
public:
    explicit BaseView(ExpandedTreeView* view, ExpandedTreeViewPrivate* viewP)
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
    [[nodiscard]] virtual QModelIndex nearestSelectionIndex(const QPoint& /*point*/) const
    {
        return {};
    }
    [[nodiscard]] virtual QModelIndexList visibleIndexes(int margin) const                                        = 0;
    virtual void dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles) = 0;
    [[nodiscard]] virtual int sizeHintForColumn(int column) const                                                 = 0;
    virtual QSize contentSizeForWidth(int width) const
    {
        return {width, m_contentsSize.height()};
    }

    [[nodiscard]] virtual int itemHeight(int item) const                                     = 0;
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
    [[nodiscard]] int findValidItem(int item, int step) const;

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
    ExpandedTreeViewPrivate* m_p;

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
    const int level = (i >= 0 ? viewItem(i).level + 1 : 0);
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

        m_p->m_viewIndexes.insert_or_assign(currentIndex, last);
        item                  = &viewItem(last);
        item->index           = currentIndex;
        item->parentItem      = i;
        item->level           = level;
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
    int indent{0};

    const int viewIndex = m_p->viewIndex(index);
    if(viewIndex >= 0) {
        indent = viewItem(viewIndex).level * m_p->m_indent;
    }

    const int xHint = delegate(index)->sizeHint(option, index).width() + indent;
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

int BaseView::findValidItem(int item, int step) const
{
    int newItem{item + step};
    while(newItem >= 0 && newItem < itemCount()) {
        if(!m_p->isItemDisabled(newItem) && (!m_view->selectIgnoreParents() || !viewItem(newItem).hasChildren)) {
            return newItem;
        }
        newItem += step;
    }
    return item;
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
    [[nodiscard]] QModelIndexList visibleIndexes(int margin) const override;
    void dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles) override;
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

    [[nodiscard]] int itemHeight(int item) const override;
    [[nodiscard]] int itemPadding(int item) const;
    [[nodiscard]] int itemIdentation(int item) const;

    void drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
    void drawRowBackground(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index,
                           int y) const;

private:
    [[nodiscard]] int indexRowSizeHint(const QModelIndex& index) const;
    [[nodiscard]] int indexSizeHint(const QModelIndex& index, bool span = false) const;
    void ensureItemOffsetCache() const;
    [[nodiscard]] int itemOffset(int item) const;
    [[nodiscard]] int itemAtOffset(int offset, bool includePadding) const;
    void recalculatePadding();
    void drawAndClipSpans(QPainter* painter, const QStyleOptionViewItem& option, int firstVisibleItem,
                          int firstVisibleItemOffset) const;
    void adjustViewOptionsForIndex(QStyleOptionViewItem* option, const QModelIndex& currentIndex) const;
};

void TreeView::invalidate()
{
    m_uniformRowHeight = 0;
    m_p->m_uniformRoleHeights.clear();
    m_p->m_itemOffsetsDirty = true;
}

void TreeView::doItemLayout()
{
    invalidate();
    layout(-1);
    recalculatePadding();
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
    int x{0};
    const int y = coordinateForItem(viewIndex).y();
    int width   = header()->sectionSize(column);
    int height{0};

    if(rule == RectRule::FullRow) {
        x      = 0;
        width  = header()->length();
        height = indexRowSizeHint(index);
    }
    else {
        x      = header()->sectionViewportPosition(column);
        height = indexSizeHint(index);
    }

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

QModelIndexList TreeView::visibleIndexes(int margin) const
{
    QModelIndexList indexes;

    int offset{0};
    const int firstVisible = firstVisibleItem(&offset);
    if(firstVisible < 0) {
        return indexes;
    }

    const int marginRows  = margin <= 0 ? 0 : std::max(1, margin / std::max(1, itemHeight(firstVisible)));
    const int lastVisible = lastVisibleItem(firstVisible, offset - margin);
    const int firstItem   = std::max(0, firstVisible - marginRows);
    for(int item = firstItem; item <= lastVisible && item < itemCount(); ++item) {
        indexes.push_back(viewItem(item).index);
    }

    return indexes;
}

void TreeView::dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& /*roles*/)
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
        m_p->m_itemOffsetsDirty = true;
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

    if(m_view->verticalScrollMode() == QAbstractItemView::ScrollPerItem) {
        if(offset) {
            *offset = 0;
        }
        return (value < 0 || value >= count) ? -1 : value;
    }

    if(m_p->m_uniformRowHeights) {
        if(m_uniformRowHeight <= 0) {
            return -1;
        }

        if(offset) {
            *offset = -(value % m_uniformRowHeight);
        }
        return value / m_uniformRowHeight;
    }

    const int item = itemAtOffset(value, false);
    if(item < 0) {
        return -1;
    }

    if(offset) {
        *offset = itemOffset(item) - value;
    }
    return item;
}

int TreeView::lastVisibleItem(int firstVisual, int offset) const
{
    if(firstVisual < 0 || offset < 0) {
        firstVisual = firstVisibleItem(&offset);
        if(firstVisual < 0) {
            return -1;
        }
    }

    int y{offset};
    const int value = viewport()->height();

    const int count = itemCount();
    for(int i{firstVisual}; i < count; ++i) {
        if(y >= value) {
            return std::max(firstVisual, i - 1);
        }

        y += itemHeight(i) + itemPadding(i);
        if(y >= value) {
            return i;
        }
    }

    return count - 1;
}

QPoint TreeView::coordinateForItem(int item) const
{
    const int vertScrollValue = verticalScrollBar()->value();

    if(m_view->verticalScrollMode() == QAbstractItemView::ScrollPerPixel) {
        if(m_p->m_uniformRowHeights) {
            return {0, (item * m_uniformRowHeight) - vertScrollValue};
        }
        return {0, itemOffset(item) - vertScrollValue};
    }

    const int topViewItemIndex{vertScrollValue};
    if(m_p->m_uniformRowHeights) {
        return {0, m_uniformRowHeight * (item - topViewItemIndex)};
    }

    return {0, itemOffset(item) - itemOffset(topViewItemIndex)};
}

int TreeView::itemAtCoordinate(QPoint coordinate, bool includePadding) const
{
    const int count = itemCount();
    if(count == 0) {
        return -1;
    }
    if(m_p->m_uniformRowHeights && m_uniformRowHeight <= 0) {
        return -1;
    }

    const int vertScrollValue = verticalScrollBar()->value();

    if(m_view->verticalScrollMode() == QAbstractItemView::ScrollPerPixel) {
        if(m_p->m_uniformRowHeights) {
            const int viewItemIndex = (coordinate.y() + vertScrollValue) / m_uniformRowHeight;
            return ((viewItemIndex >= count || viewItemIndex < 0) ? -1 : viewItemIndex);
        }

        return itemAtOffset(coordinate.y() + vertScrollValue, includePadding);
    }

    const int topViewItemIndex{vertScrollValue};
    if(m_p->m_uniformRowHeights) {
        if(coordinate.y() < 0) {
            coordinate.ry() -= m_uniformRowHeight - 1;
        }
        const int viewItemIndex = topViewItemIndex + (coordinate.y() / m_uniformRowHeight);
        return ((viewItemIndex >= itemCount() || viewItemIndex < 0) ? -1 : viewItemIndex);
    }

    return itemAtOffset(itemOffset(topViewItemIndex) + coordinate.y(), includePadding);
}

int TreeView::itemAbove(int item) const
{
    return findValidItem(item, -1);
}

int TreeView::itemBelow(int item) const
{
    return findValidItem(item, 1);
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
            m_view->style()->drawPrimitive(QStyle::PE_PanelItemViewRow, &opt, painter, m_view);
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

    bool uniform{false};
    if(m_p->m_uniformRowHeights) {
        if(m_uniformRowHeight <= 0) {
            m_uniformRowHeight = itemHeight(0);
        }
        uniform = m_uniformRowHeight > 0;
    }

    if(uniform) {
        itemsInViewport = viewportSize.height() / m_uniformRowHeight;
    }
    else {
        for(int height{0}, item = count - 1; item >= 0; --item) {
            height += itemHeight(item);
            if(height > viewportHeight) {
                break;
            }
            ++itemsInViewport;
        }
    }

    auto* verticalBar = verticalScrollBar();

    if(m_view->verticalScrollMode() == QAbstractItemView::ScrollPerItem) {
        if(!empty()) {
            itemsInViewport = std::max(1, itemsInViewport);
        }
        verticalBar->setRange(0, count - itemsInViewport);
        verticalBar->setPageStep(itemsInViewport);
        verticalBar->setSingleStep(1);
    }
    else {
        int contentsHeight{0};
        for(int i{0}; i < count; ++i) {
            contentsHeight += itemHeight(i) + itemPadding(i);
        }

        const int vMax = m_view->adjustVerticalScrollMaximum(std::max(0, contentsHeight - viewportHeight));
        if(verticalBar->isVisible() && vMax <= 0) {
            m_p->m_hidingScrollbar = 2;
        }

        verticalBar->setRange(0, vMax);
        verticalBar->setPageStep(viewportHeight);
        verticalBar->setSingleStep(std::max(viewportHeight / (itemsInViewport + 1), 2));
    }

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
    if(item < 0 || std::cmp_greater_equal(item, m_p->m_viewItems.size())) {
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
    if(item < 0 || std::cmp_greater_equal(item, m_p->m_viewItems.size())) {
        return 0;
    }

    return viewItem(item).padding;
}

int TreeView::itemIdentation(int item) const
{
    if(item < 0 || std::cmp_greater_equal(item, m_p->m_viewItems.size())) {
        return 0;
    }

    const int level = viewItem(item).level;
    return level * m_p->m_indent;
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
    const bool isParent     = model()->hasChildren(index);

    opt.state.setFlag(QStyle::State_MouseOver, hoverRow);

    if(isParent) {
        // Span first column of parents
        opt.rect.setX(0);
        opt.rect.setWidth(header()->length());

        const auto bg = index.data(Qt::BackgroundRole).value<QBrush>();
        if(WidgetBackground::hasTransparentRows(m_view) && !opt.state.testFlag(QStyle::State_Selected)
           && bg.style() == Qt::NoBrush) {
            WidgetBackground::makeRowPaletteTransparent(opt.palette);
        }

        m_view->style()->drawPrimitive(QStyle::PE_PanelItemViewRow, &opt, painter, m_view);
        m_view->style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, m_view);
        delegate(index)->paint(painter, opt, index);
        return;
    }

    if(m_view->alternatingRowColors()) {
        if(index.parent().isValid()) {
            // For child rows, apply alternating colour starting from index 0
            opt.features.setFlag(QStyleOptionViewItem::Alternate, (index.row() % 2) == 0);
        }
        else if(isParent) {
            // Ensure parent rows always use the base colour (no alternate)
            opt.features.setFlag(QStyleOptionViewItem::Alternate, false);
        }
        else {
            opt.features.setFlag(QStyleOptionViewItem::Alternate, index.row() & 1);
        }
    }

    const QPoint offset       = m_p->m_scrollDelayOffset;
    const int y               = opt.rect.y() + offset.y();
    const int left            = m_leftAndRight.first;
    const int right           = m_leftAndRight.second;
    const QModelIndex parent  = index.parent();
    const QModelIndex current = m_view->currentIndex();

    const auto& item = viewItem(m_p->viewIndex(index));

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

        modelIndex = model()->index(index.row(), headerSection, parent);
        if(!modelIndex.isValid()) {
            continue;
        }

        opt.viewItemPosition = viewItemPosList.at(section);

        if(m_view->selectionModel()->isSelected(modelIndex)) {
            opt.state |= QStyle::State_Selected;
            opt.features.setFlag(QStyleOptionViewItem::Alternate, false);
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

        const int width    = header()->sectionSize(headerSection);
        const int position = header()->sectionViewportPosition(headerSection) + offset.x();

        const int cellHeight = indexRowSizeHint(modelIndex);
        if(cellHeight > 0) {
            opt.rect = {position, y, width, cellHeight};

            if(section == 0) {
                const int indent = item.level * m_p->m_indent;
                opt.rect.adjust(indent, 0, 0, 0);
            }

            if(!paintedBg) {
                // Some styles use a gradient for the selection bg which only covers each individual cell
                // Show this as a single gradient across the entire row
                paintedBg = true;
                drawRowBackground(painter, opt, modelIndex, y);
            }

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
    const auto bg                      = index.data(Qt::BackgroundRole).value<QBrush>();
    const bool transparentBase         = WidgetBackground::hasTransparentRows(m_view);
    const bool preserveStyleBackground = option.state.testFlag(QStyle::State_Selected) || bg.style() != Qt::NoBrush;

    const auto paintRects = m_p->rectsToPaint(index, option, y);
    for(const auto& rect : paintRects) {
        if(rect.width() > 0) {
            QStyleOptionViewItem opt{option};
            opt.rect = rect;

            if(transparentBase && !preserveStyleBackground) {
                WidgetBackground::makeRowPaletteTransparent(opt.palette);
            }

            m_view->style()->drawPrimitive(QStyle::PE_PanelItemViewRow, &opt, painter, m_view);
            m_view->style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, m_view);
            painter->fillRect(opt.rect, bg);
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

    int uniformData{-1};
    if(m_p->m_uniformHeightRole >= 0) {
        uniformData = index.data(m_p->m_uniformHeightRole).toInt();
        if(m_p->m_uniformRoleHeights.contains(uniformData)) {
            return m_p->m_uniformRoleHeights.at(uniformData);
        }
    }

    int start{-1};
    int end{-1};
    const int indexRow       = index.row();
    const QModelIndex parent = index.parent();
    int count                = header()->count();
    const bool headerIsEmpty = (count == 0);

    if(count > 0 && m_view->isVisible()) {
        start = std::min(header()->visualIndexAt(0), 0);
    }
    else {
        count = model()->columnCount(parent);
    }

    if(m_view->isRightToLeft()) {
        start = (start == -1 ? count - 1 : start);
        end   = 0;
    }
    else {
        start = (start == -1 ? 0 : start);
        end   = count - 1;
    }

    if(end < start) {
        std::swap(end, start);
    }

    QStyleOptionViewItem opt;
    m_view->initViewItemOption(&opt);

    opt.rect.setWidth(-1);
    int height{-1};

    for(int column{start}; column <= end; ++column) {
        const int logical = headerIsEmpty ? column : header()->logicalIndex(column);
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

    if(m_p->m_uniformHeightRole >= 0) {
        if(!m_p->m_uniformRoleHeights.contains(uniformData)) {
            m_p->m_uniformRoleHeights.emplace(uniformData, height);
        }
    }

    return height;
}

int TreeView::indexSizeHint(const QModelIndex& index, bool span) const
{
    if(!isIndexValid(index)) {
        return 0;
    }

    int height{0};
    if(span && m_view->isSpanning(index.column())) {
        height = m_view->spanHeight(index);
    }

    if(m_p->m_uniformRowHeights && m_uniformRowHeight > 0) {
        return std::max(m_uniformRowHeight, height);
    }

    int uniformData{-1};
    if(m_p->m_uniformHeightRole >= 0) {
        uniformData = index.data(m_p->m_uniformHeightRole).toInt();
        if(m_p->m_uniformRoleHeights.contains(uniformData)) {
            return std::max(m_p->m_uniformRoleHeights.at(uniformData), height);
        }
    }

    QStyleOptionViewItem opt;
    m_view->initViewItemOption(&opt);

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

void TreeView::ensureItemOffsetCache() const
{
    const int count = itemCount();
    if(!m_p->m_itemOffsetsDirty && static_cast<int>(m_p->m_itemOffsets.size()) == count + 1) {
        return;
    }

    m_p->m_itemOffsets.resize(count + 1);
    m_p->m_itemOffsets[0] = 0;

    for(int i{0}; i < count; ++i) {
        m_p->m_itemOffsets[i + 1] = m_p->m_itemOffsets[i] + itemHeight(i) + itemPadding(i);
    }

    m_p->m_itemOffsetsDirty = false;
}

int TreeView::itemOffset(int item) const
{
    if(item <= 0) {
        return 0;
    }

    ensureItemOffsetCache();
    if(std::cmp_greater_equal(item, m_p->m_itemOffsets.size())) {
        return m_p->m_itemOffsets.empty() ? 0 : m_p->m_itemOffsets.back();
    }

    return m_p->m_itemOffsets.at(item);
}

int TreeView::itemAtOffset(int offset, bool includePadding) const
{
    if(offset < 0 || itemCount() == 0) {
        return -1;
    }

    ensureItemOffsetCache();

    const auto begin = m_p->m_itemOffsets.cbegin();
    const auto end   = m_p->m_itemOffsets.cend();
    const auto it    = std::ranges::upper_bound(begin, end, offset);
    if(it == begin || it == end) {
        return -1;
    }

    const int item = static_cast<int>(std::ranges::distance(begin, it)) - 1;
    if(includePadding && offset >= m_p->m_itemOffsets.at(item) + itemHeight(item)) {
        return -1;
    }

    return item;
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
                max = std::max(max, m_view->spanHeight(columnIndex));
            }
        }
    }

    int rowHeight{0};
    auto& items = viewItems();

    for(auto& item : items) {
        item.padding = 0;

        if(item.hasChildren) {
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

    m_p->m_itemOffsetsDirty = true;
}

void TreeView::drawAndClipSpans(QPainter* painter, const QStyleOptionViewItem& option, int firstVisibleItem,
                                int firstVisibleItemOffset) const
{
    QStyleOptionViewItem opt{option};

    const auto spanTopInset = [this](const QModelIndex& index) {
        if(!m_view->extendSpansIntoParents() || !index.parent().isValid()) {
            return 0;
        }

        QModelIndex topLevelContainer{index.parent()};
        while(topLevelContainer.parent().isValid()) {
            topLevelContainer = topLevelContainer.parent();
        }

        QModelIndex firstLeaf{topLevelContainer};
        while(model()->hasChildren(firstLeaf)) {
            firstLeaf = model()->index(0, 0, firstLeaf);
        }

        if(index.row() != firstLeaf.row() || index.parent() != firstLeaf.parent()) {
            return 0;
        }

        int inset{0};
        for(QModelIndex ancestor = index.parent(); ancestor.parent().isValid(); ancestor = ancestor.parent()) {
            inset += indexRowSizeHint(ancestor);
        }

        return inset;
    };

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

            const int topInset = spanTopInset(index);

            opt.rect = {position, y - topInset, width, indexSizeHint(modelIndex, true) + topInset};
            region -= m_view->drawSpan(painter, opt, modelIndex);
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
    void doItemLayout(bool reuseMeasurements);
    void drawView(QPainter* painter, const QRegion& region) const override;

    [[nodiscard]] bool canReuseMeasurements() const;

    [[nodiscard]] QRect visualRect(const QModelIndex& index, RectRule rule, bool includePadding) const override;
    [[nodiscard]] ExpandedTreeViewItem indexToViewItem(const QModelIndex& index) const override;
    [[nodiscard]] QModelIndex findIndexAt(const QPoint& point, bool includeSpans, bool includePadding) const override;
    [[nodiscard]] QModelIndexList visibleIndexes(int margin) const override;
    void dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles) override;
    [[nodiscard]] int sizeHintForColumn(int column) const override;

    [[nodiscard]] int itemHeight(int item) const override;
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
    void resetGrid();
    void prepareItemLayout();
    void drawItem(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
    void drawFocus(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
    [[nodiscard]] QModelIndex iconItemIndex(const QModelIndex& index) const;

    void setupDecorationProps(QStyleOptionViewItem* opt) const;
    [[nodiscard]] std::vector<ExpandedTreeViewItem> itemsOnRow(int y, int x) const;
    [[nodiscard]] QSize indexSizeHint(const QModelIndex& index) const;
    [[nodiscard]] int itemWidth(int item) const;
    [[nodiscard]] int rowHeight(int rowStart) const;
    [[nodiscard]] int spacing() const;
    [[nodiscard]] QSize iconSize() const;
    [[nodiscard]] bool haveSideCaptions() const;
    [[nodiscard]] bool haveBottomCaptions() const;
    [[nodiscard]] bool useIconGaps() const;

    QRect m_layoutBounds;
    int m_segmentSize{0};
    int m_itemSpacing{MinItemSpacing};
    int m_rowSpacing{IconRowSpacing};
    mutable int m_uniformRowWidth{0};
    mutable int m_uniformRowHeight{0};
};

void IconView::invalidate()
{
    m_uniformRowWidth  = 0;
    m_uniformRowHeight = 0;
    resetGrid();
}

void IconView::doItemLayout()
{
    doItemLayout(false);
}

void IconView::doItemLayout(bool reuseMeasurements)
{
    m_p->m_expansionRect = {};
    if(reuseMeasurements) {
        resetGrid();
        viewItem(0).width = m_uniformRowWidth;
    }
    else {
        invalidate();
        layout(-1);
    }

    if(empty()) {
        return;
    }

    prepareItemLayout();

    const QPoint topLeft{m_layoutBounds.x(), m_layoutBounds.y() + m_rowSpacing};

    int segStartPosition{m_layoutBounds.left()};
    int segEndPosition{m_layoutBounds.right()};

    int deltaSegPosition{0};
    int segPosition{topLeft.y()};

    const bool useConfiguredGridSpacing = useIconGaps();
    const bool fixedGridSpacing         = useConfiguredGridSpacing && m_p->m_iconHorizontalGap >= 0;

    if(useConfiguredGridSpacing && !fixedGridSpacing) {
        segStartPosition += m_itemSpacing;
        segEndPosition -= m_itemSpacing;
    }

    // Determine the number of items per row
    const int count = itemCount();
    for(int i{1}; i <= count; ++i) {
        const int spacingCount  = fixedGridSpacing ? std::max(0, i - 1) : i - 1;
        const int requiredWidth = (i * itemWidth(0)) + (spacingCount * m_itemSpacing);
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
    const int itmWidth            = useConfiguredGridSpacing ? itemWidth(0) : totalWidthAvailable / m_segmentSize;
    const int maxPadding          = static_cast<int>(totalWidthAvailable * maxPaddingRatio);

    int gridStartPosition{segStartPosition};
    if(fixedGridSpacing) {
        const int nextRowWidth = ((m_segmentSize + 1) * itmWidth) + (m_segmentSize * m_itemSpacing);
        if(nextRowWidth > totalWidthAvailable) {
            const int gridWidth      = (m_segmentSize * itmWidth) + (std::max(0, m_segmentSize - 1) * m_itemSpacing);
            const int remainingWidth = totalWidthAvailable - gridWidth;
            gridStartPosition += std::max(0, remainingWidth / 2);
        }
        else {
            gridStartPosition += MinItemSpacing;
        }
    }

    if(!fixedGridSpacing) {
        const int totalPadding = totalWidthAvailable - totalItemWidth;
        m_itemSpacing          = std::max(0, totalPadding / (m_segmentSize + 1));

        if(maxPadding > 0 && m_itemSpacing > maxPadding) {
            m_itemSpacing = MinItemSpacing;
        }
    }

    QRect rect{{}, topLeft};
    const bool animating   = m_p->m_expansionAnimationHeight >= 0;
    const auto expanded    = animating ? m_p->m_expansionAnimationIndex : m_p->m_expandedIndex;
    const int expandedItem = m_p->m_expansionWidget ? m_p->viewIndex(expanded) : -1;
    const int expandedRow  = expandedItem >= 0 ? expandedItem / m_segmentSize : -1;

    for(int i{0}; i < count; ++i) {
        auto& item = viewItem(i);

        const int segColumn = i % m_segmentSize;
        if(segColumn == 0 && i > 0) {
            segPosition += deltaSegPosition;
            deltaSegPosition = 0;
        }

        if(!useConfiguredGridSpacing) {
            item.x = segStartPosition + (segColumn * itmWidth);
        }
        else if(fixedGridSpacing) {
            item.x = gridStartPosition + (segColumn * (itmWidth + m_itemSpacing));
        }
        else {
            item.x = segStartPosition + m_itemSpacing + (segColumn * (itmWidth + m_itemSpacing));
        }

        item.y     = segPosition;
        item.width = itmWidth;

        deltaSegPosition = std::max(deltaSegPosition, itemHeight(i) + spacing());
        rect |= item.rect();

        if(i / m_segmentSize == expandedRow && (segColumn == m_segmentSize - 1 || i == count - 1)) {
            const int expansionWidth = viewport()->width();
            const int height     = animating ? m_p->m_expansionAnimationHeight
                                             : std::max(1, m_p->m_expansionWidget->hasHeightForWidth()
                                                               ? m_p->m_expansionWidget->heightForWidth(expansionWidth)
                                                               : m_p->m_expansionWidget->sizeHint().height());
            m_p->m_expansionRect = {0, segPosition + deltaSegPosition, viewport()->width(), height};
            int gap              = std::max(spacing(), IconRowSpacing);
            if(animating) {
                const int fullHeight = std::max(1, m_p->animationSnapshot().deviceIndependentSize().toSize().height());
                gap                  = static_cast<int>(std::lround(static_cast<double>(gap) * height / fullHeight));
            }
            deltaSegPosition += height + gap;
            rect |= m_p->m_expansionRect;
        }
    }

    m_contentsSize = rect.size();
    m_contentsSize.rheight() += m_rowSpacing;
}

void IconView::drawView(QPainter* painter, const QRegion& region) const
{
    QStyleOptionViewItem opt;
    m_view->initViewItemOption(&opt);

    if(m_p->m_viewItems.empty() || header()->count() == 0) {
        return;
    }

    const QRect area       = region.boundingRect();
    m_leftAndRight         = startAndEndColumns(area);
    const int firstVisible = firstVisibleItem();
    if(firstVisible < 0) {
        return;
    }

    for(int rowStart{firstVisible}; rowStart < itemCount(); rowStart += m_segmentSize) {
        const auto& item = viewItem(rowStart);
        const int y      = item.y - verticalScrollBar()->value();
        if(y > area.bottom()) {
            break;
        }
        if(y + rowHeight(rowStart) < area.top()) {
            continue;
        }
        for(const auto& rowItem : itemsOnRow(item.y, area.left())) {
            drawItem(painter, opt, rowItem.index);
        }
    }
}

bool IconView::canReuseMeasurements() const
{
    return !empty() && m_uniformRowWidth > 0 && m_layoutBounds.width() == viewport()->width();
}

QRect IconView::visualRect(const QModelIndex& index, RectRule /*rule*/, bool /*includePadding*/) const
{
    if(!isIndexValid(index)) {
        return {};
    }

    m_p->layoutItems();

    const int viewIndex = m_p->viewIndex(index);
    if(viewIndex < 0) {
        return {};
    }

    return mapToViewport(viewItem(viewIndex).rect());
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
            return iconItemIndex(item.index);
        }
    }

    return {};
}

QModelIndexList IconView::visibleIndexes(int margin) const
{
    QModelIndexList indexes;

    if(empty() || m_segmentSize <= 0) {
        return indexes;
    }

    int firstVisibleOffset{0};
    const int firstVisible = firstVisibleItem(&firstVisibleOffset);
    if(firstVisible < 0) {
        return indexes;
    }

    const QRect viewportRect = viewport()->rect().adjusted(-margin, -margin, margin, margin);

    const int count         = itemCount();
    const int firstRowStart = std::max(0, firstVisible - m_segmentSize);
    for(int rowStart = firstRowStart; rowStart < count; rowStart += m_segmentSize) {
        if(mapToViewport(viewItem(rowStart).rect()).top() > viewportRect.bottom()) {
            break;
        }

        const int rowEnd = std::min(rowStart + m_segmentSize, count);
        for(int item = rowStart; item < rowEnd; ++item) {
            const auto& viewItem = this->viewItem(item);
            if(mapToViewport(viewItem.rect()).intersects(viewportRect)) {
                indexes.push_back(viewItem.index);
            }
        }
    }

    return indexes;
}

void IconView::dataChanged(const QModelIndex& /*topLeft*/, const QModelIndex& /*bottomRight*/,
                           const QList<int>& /*roles*/)
{
    // Keep the current geometry, but rebuild measurements on the next expansion refresh
    m_uniformRowWidth  = 0;
    m_uniformRowHeight = 0;
}

int IconView::sizeHintForColumn(int /*column*/) const
{
    return -1;
}

int IconView::itemHeight(int item) const
{
    if(m_p->m_viewItems.empty()) {
        return 0;
    }

    const QModelIndex& index = viewItem(item).index;
    if(!index.isValid()) {
        return 0;
    }

    int height = viewItem(item).height;

    if(height <= 0) {
        if(m_p->m_uniformRowHeights) {
            if(m_uniformRowHeight == 0) {
                const QSize hint   = indexSizeHint(index);
                m_uniformRowHeight = hint.height();
            }
            height = m_uniformRowHeight;
        }
        else {
            const QSize hint = indexSizeHint(index);
            height           = hint.height();
        }

        viewItem(item).height = height;
    }

    return std::max(height, 0);
}

int IconView::firstVisibleItem(int* offset) const
{
    const int value = verticalScrollBar()->value();
    const int count = itemCount();

    for(int i{0}; i < count; i += m_segmentSize) {
        const int y = viewItem(i).y;
        if(y + rowHeight(i) + spacing() > value) {
            if(offset) {
                *offset = y - value;
            }
            return i;
        }
    }

    return -1;
}

int IconView::lastVisibleItem(int firstVisual, int /*offset*/) const
{
    if(firstVisual < 0) {
        firstVisual = firstVisibleItem();
    }
    int last{firstVisual};
    for(int i{std::max(0, firstVisual)}; i < itemCount(); ++i) {
        if(viewItem(i).y > verticalScrollBar()->value() + viewport()->height()) {
            break;
        }
        last = i;
    }
    return last;
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

    coordinate += {horizontalScrollBar()->value(), verticalScrollBar()->value()};
    for(int i{0}; i < count; ++i) {
        if(viewItem(i).rect().contains(coordinate)) {
            return i;
        }
    }

    if(includePadding) {
        int nearest{0};
        int distance = std::numeric_limits<int>::max();
        for(int i{0}; i < count; ++i) {
            const QPoint delta  = viewItem(i).rect().center() - coordinate;
            const int candidate = delta.manhattanLength();
            if(candidate < distance) {
                nearest  = i;
                distance = candidate;
            }
        }
        return nearest;
    }

    return -1;
}

int IconView::itemAbove(int item) const
{
    return findValidItem(item, -m_segmentSize);
}

int IconView::itemBelow(int item) const
{
    return findValidItem(item, m_segmentSize);
}

int IconView::itemLeft(int item) const
{
    if(item % m_segmentSize == 0) {
        return item;
    }

    --item;
    while(m_p->isItemDisabled(item)) {
        --item;
    }
    return item;
}

int IconView::itemRight(int item) const
{
    if((item + 1) % m_segmentSize == 0) {
        return item;
    }

    ++item;
    while(m_p->isItemDisabled(item)) {
        ++item;
    }
    return item;
}

int IconView::pageUp(int item) const
{
    auto coord = coordinateForItem(item);
    coord.ry() -= viewport()->height();

    const int target = itemAtCoordinate(coord, true);
    return m_p->m_expansionRect.isValid() && target >= item ? itemAbove(item) : target;
}

int IconView::pageDown(int item) const
{
    auto coord = coordinateForItem(item);
    coord.ry() += viewport()->height();

    const int target = itemAtCoordinate(coord, true);
    return m_p->m_expansionRect.isValid() && target <= item ? itemBelow(item) : target;
}

void IconView::renderToPixmap(QPainter* painter, const ItemViewPaintPairs& paintPairs, QRect& rect) const
{
    QStyleOptionViewItem opt;
    m_view->initViewItemOption(&opt);

    opt.state |= QStyle::State_Selected;

    for(const auto& [paintRect, index] : paintPairs) {
        const QModelIndex iconIndex = iconItemIndex(index);
        if(!iconIndex.isValid()) {
            continue;
        }

        QStyleOptionViewItem paintOpt{opt};
        paintOpt.decorationSize         = iconSize();
        paintOpt.showDecorationSelected = true;
        setupDecorationProps(&paintOpt);

        paintOpt.rect = paintRect.translated(-rect.topLeft());

        m_view->style()->drawPrimitive(QStyle::PE_PanelItemViewRow, &paintOpt, painter, m_view);
        m_view->style()->drawControl(QStyle::CE_ItemViewItem, &paintOpt, painter, m_view);
        delegate(iconIndex)->paint(painter, paintOpt, iconIndex);
    }
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
    bool verticalWantsToShow{false};

    if(horizontalWantsToShow) {
        verticalWantsToShow = m_contentsSize.height() > vpSize.height() - m_view->horizontalScrollBar()->height();
    }
    else {
        verticalWantsToShow = m_contentsSize.height() > vpSize.height();
    }

    if(m_p->m_expansionAnimationHeight >= 0) {
        // Keep the viewport width stable while the expansion changes the scroll range
        verticalScrollBar()->setRange(
            0, std::max(m_p->m_expansionAnimationScrollMax, m_contentsSize.height() - viewport()->height()));
    }
    else if(bothScrollBarsAuto && !verticalWantsToShow) {
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

void IconView::resetGrid()
{
    m_segmentSize = 0;

    const bool useConfiguredGridSpacing = useIconGaps();
    m_itemSpacing
        = useConfiguredGridSpacing && m_p->m_iconHorizontalGap >= 0 ? m_p->m_iconHorizontalGap : MinItemSpacing;
    m_rowSpacing = useConfiguredGridSpacing ? m_p->m_iconVerticalGap : 0;
}

void IconView::prepareItemLayout()
{
    // Layout in content coordinates using the actual visible viewport. Scrollbars are already
    // excluded from the viewport geometry, which avoids style-specific drift after resize.
    m_layoutBounds = QRect{{}, viewport()->size()};
}

void IconView::drawItem(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if(!index.isValid()) {
        return;
    }

    const QModelIndex iconIndex = iconItemIndex(index);
    if(!iconIndex.isValid()) {
        return;
    }

    QStyleOptionViewItem opt{option};

    const QModelIndex hover = m_p->m_hoverIndex;
    const bool hoverRow     = index.parent() == hover.parent() && index.row() == hover.row();
    const bool rowFocused   = m_view->hasFocus();

    opt.state.setFlag(QStyle::State_MouseOver, hoverRow);

    if(m_view->alternatingRowColors()
       && (m_p->m_viewMode == ExpandedTreeView::ViewMode::Tree
           || m_p->m_captionDisplay == ExpandedTreeView::CaptionDisplay::Right)) {
        opt.features.setFlag(QStyleOptionViewItem::Alternate, index.row() & 1);
    }

    const QModelIndex current = m_view->currentIndex();

    bool currentRowHasFocus{false};

    if(m_view->selectionModel()->isSelected(index)) {
        opt.state |= QStyle::State_Selected;
        opt.features.setFlag(QStyleOptionViewItem::Alternate, false);
    }
    if(rowFocused && current.parent() == index.parent() && current.row() == index.row()) {
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

    opt.rect = mapToViewport(indexToViewItem(index).rect());

    const auto bg = iconIndex.data(Qt::BackgroundRole).value<QBrush>();
    if(WidgetBackground::hasTransparentRows(m_view) && !opt.state.testFlag(QStyle::State_Selected)
       && bg.style() == Qt::NoBrush) {
        WidgetBackground::makeRowPaletteTransparent(opt.palette);
    }

    QStyleOptionViewItem mainOpt{opt};
    mainOpt.decorationSize         = iconSize();
    mainOpt.showDecorationSelected = true;
    setupDecorationProps(&mainOpt);

    m_view->style()->drawPrimitive(QStyle::PE_PanelItemViewRow, &opt, painter, m_view);
    m_view->style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, m_view);
    delegate(iconIndex)->paint(painter, mainOpt, iconIndex);

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

QModelIndex IconView::iconItemIndex(const QModelIndex& index) const
{
    return m_p->iconItemIndex(index);
}

void IconView::setupDecorationProps(QStyleOptionViewItem* opt) const
{
    if(!opt) {
        return;
    }

    switch(m_p->m_captionDisplay) {
        case ExpandedTreeView::CaptionDisplay::Right:
            opt->decorationPosition = QStyleOptionViewItem::Left;
            opt->displayAlignment   = Qt::AlignLeft;
            break;
        case ExpandedTreeView::CaptionDisplay::None:
        case ExpandedTreeView::CaptionDisplay::Bottom:
            opt->decorationPosition = QStyleOptionViewItem::Top;
            opt->displayAlignment   = Qt::AlignCenter;
            break;
    }
}

std::vector<ExpandedTreeViewItem> IconView::itemsOnRow(int y, int x) const
{
    std::vector<ExpandedTreeViewItem> result;

    bool foundAjacant{false};
    for(const auto& item : m_p->m_viewItems) {
        if(item.y == y && item.x + item.width >= x) {
            foundAjacant = true;
            result.push_back(item);
        }
        else if(foundAjacant) {
            break;
        }
    }

    return result;
}

QSize IconView::indexSizeHint(const QModelIndex& index) const
{
    if(!m_p->isIndexValid(index) || !m_view->itemDelegate()) {
        return {};
    }

    const QModelIndex iconIndex = iconItemIndex(index);
    if(!iconIndex.isValid()) {
        return {};
    }

    QStyleOptionViewItem opt;
    m_view->initViewItemOption(&opt);
    opt.showDecorationSelected = true;
    setupDecorationProps(&opt);
    opt.decorationSize = iconSize();

    QSize size = delegate(iconIndex)->sizeHint(opt, iconIndex);

    if(m_p->m_captionDisplay == ExpandedTreeView::CaptionDisplay::Bottom) {
        size.rwidth() = std::max(size.width(), iconSize().width() + (2 * MinItemSpacing));
    }
    else if(m_p->m_captionDisplay == ExpandedTreeView::CaptionDisplay::Right) {
        size.rwidth() = iconSize().width() + (2 * MinItemSpacing) + RightCaptionWidth;
    }
    else {
        size.rheight() = iconSize().height();
    }

    return size;
}

int IconView::itemWidth(int item) const
{
    if(empty()) {
        return 0;
    }

    const QModelIndex& index = viewItem(item).index;
    if(!index.isValid()) {
        return 0;
    }

    int width = viewItem(item).width;

    if(width <= 0) {
        if(m_uniformRowWidth == 0) {
            const QSize hint  = indexSizeHint(index);
            m_uniformRowWidth = hint.width();
        }
        width                = m_uniformRowWidth;
        viewItem(item).width = width;
    }

    return std::max(width, 0);
}

int IconView::rowHeight(int rowStart) const
{
    int height{0};
    const int rowEnd = std::min(rowStart + m_segmentSize, itemCount());
    for(int item{rowStart}; item < rowEnd; ++item) {
        height = std::max(height, itemHeight(item));
    }

    return height;
}

int IconView::spacing() const
{
    return m_rowSpacing;
}

QSize IconView::iconSize() const
{
    return m_view->iconSize();
}

bool IconView::haveSideCaptions() const
{
    return m_p->m_captionDisplay == ExpandedTreeView::CaptionDisplay::Right;
}

bool IconView::haveBottomCaptions() const
{
    return m_p->m_captionDisplay == ExpandedTreeView::CaptionDisplay::Bottom;
}

bool IconView::useIconGaps() const
{
    return haveBottomCaptions() || !haveSideCaptions() || m_p->m_useIconGapsForSideCaptions;
}

class FlowView : public BaseView
{
public:
    using BaseView::BaseView;

    void invalidate() override;

    void doItemLayout() override;
    void drawView(QPainter* painter, const QRegion& region) const override;

    QRect visualRect(const QModelIndex& index, RectRule rule, bool includePadding) const override;
    ExpandedTreeViewItem indexToViewItem(const QModelIndex& index) const override;
    QModelIndex findIndexAt(const QPoint& point, bool incSpans, bool incPadding) const override;
    QModelIndex nearestSelectionIndex(const QPoint& point) const override;
    QModelIndexList visibleIndexes(int margin) const override;
    void dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles) override;
    int sizeHintForColumn(int column) const override;
    QSize contentSizeForWidth(int width) const override;

    int firstVisibleItem(int* offset) const override;
    int lastVisibleItem(int first, int offset) const override;
    QPoint coordinateForItem(int item) const override;
    int itemAtCoordinate(QPoint point, bool includePadding) const override;
    int itemAbove(int item) const override;
    int itemBelow(int item) const override;
    int itemLeft(int item) const override;
    int itemRight(int item) const override;
    int pageUp(int item) const override;
    int pageDown(int item) const override;

    void renderToPixmap(QPainter* painter, const ItemViewPaintPairs& pairs, QRect& rect) const override;

    void updateScrollBars() override;

    int itemHeight(int item) const override;

private:
    const FlowLayout& measuredLayout(int width) const;
    void measure(const QModelIndex& parent, QStyleOptionViewItem& option) const;
    void drawItem(QPainter* painter, QStyleOptionViewItem option, const QModelIndex& index, const QRect& rect) const;

    [[nodiscard]] std::vector<int> itemsInRect(const QRect& rect) const;
    int adjacentColumn(int item, int direction) const;
    int pageItem(int item, int direction) const;

    mutable bool m_measured{false};
    mutable std::vector<FlowLayoutItem> m_measurements;
    mutable FlowLayout m_layout;
    FlowLayout m_applied;
};

void FlowView::measure(const QModelIndex& parent, QStyleOptionViewItem& option) const
{
    const auto rowCount = model()->rowCount(parent);
    for(int row{0}; row < rowCount; ++row) {
        const auto index  = model()->index(row, 0, parent);
        const size_t item = m_measurements.size();
        const QSize size  = delegate(index)->sizeHint(option, index);

        m_measurements.emplace_back(size, 0);

        if(model()->hasChildren(index)) {
            measure(index, option);
            m_measurements[item].children = static_cast<int>(m_measurements.size() - item - 1);
        }
    }
}

const FlowLayout& FlowView::measuredLayout(int width) const
{
    if(!m_measured) {
        QStyleOptionViewItem opt;
        m_view->initViewItemOption(&opt);
        measure(m_view->rootIndex(), opt);
        m_measured = true;
    }

    width = std::max(1, width);

    if(m_layout.size.width() != width) {
        const FlowLayoutOptions options{
            .width              = width,
            .gap                = m_p->m_flowColumnGap,
            .minimumRows        = m_p->m_flowMinimumRows,
            .maximumColumns     = m_p->m_flowMaximumColumns,
            .minimumColumnWidth = m_p->m_flowMinimumColumnWidth,
            .stretchColumns     = m_p->m_flowStretchColumns,
            .rightToLeft        = m_view->isRightToLeft(),
        };
        m_layout = FlowLayoutBuilder{m_measurements, options}.build();
    }

    return m_layout;
}

void FlowView::invalidate()
{
    m_measurements.clear();
    m_measured = false;
    m_layout   = {};
    m_applied  = {};
}

void FlowView::doItemLayout()
{
    layout(-1);
    m_applied      = measuredLayout(viewport()->width());
    m_contentsSize = m_applied.size;

    const int count = itemCount();
    for(int i{0}; i < count; ++i) {
        const auto& rect = m_applied.rects.at(i);
        auto& item       = viewItem(i);
        item.x           = rect.x();
        item.y           = rect.y();
        item.width       = rect.width();
        item.height      = rect.height();
    }
}

ExpandedTreeViewItem FlowView::indexToViewItem(const QModelIndex& index) const
{
    const int item = m_p->viewIndex(index);
    return item >= 0 ? viewItem(item) : ExpandedTreeViewItem{};
}

QRect FlowView::visualRect(const QModelIndex& index, RectRule /*rule*/, bool /*includePadding*/) const
{
    if(!isIndexValid(index)) {
        return {};
    }

    m_p->layoutItems();
    return mapToViewport(indexToViewItem(index).rect());
}

std::vector<int> FlowView::itemsInRect(const QRect& rect) const
{
    std::vector<int> found;
    auto header = std::ranges::lower_bound(m_applied.headers, rect.top(),
                                           [this](int item, int y) { return m_applied.rects[item].bottom() < y; });
    for(; header != m_applied.headers.cend() && m_applied.rects[*header].top() <= rect.bottom(); ++header) {
        if(m_applied.rects[*header].intersects(rect)) {
            found.push_back(*header);
        }
    }

    auto section = std::lower_bound(m_applied.sections.cbegin(), m_applied.sections.cend(), rect.top(),
                                    [](const auto& block, int y) { return block.rect.bottom() < y; });
    for(; section != m_applied.sections.cend() && section->rect.top() <= rect.bottom(); ++section) {
        for(const auto& column : section->columns) {
            if(!column.rect.intersects(rect)) {
                continue;
            }

            const auto begin = m_applied.rects.cbegin() + column.first;
            const auto end   = m_applied.rects.cbegin() + column.last;
            auto row = std::lower_bound(begin, end, rect.top(), [](const QRect& r, int y) { return r.bottom() < y; });
            for(; row != end && row->top() <= rect.bottom(); ++row) {
                if(row->intersects(rect)) {
                    found.push_back(static_cast<int>(row - m_applied.rects.cbegin()));
                }
            }
        }
    }

    std::ranges::sort(found);
    return found;
}

int FlowView::itemAtCoordinate(QPoint point, bool /*includePadding*/) const
{
    const auto found = itemsInRect({point, QSize{1, 1}});
    return found.empty() ? -1 : found.front();
}

int FlowView::itemAbove(int item) const
{
    return findValidItem(item, -1);
}

int FlowView::itemBelow(int item) const
{
    return findValidItem(item, 1);
}

int FlowView::itemLeft(int item) const
{
    return adjacentColumn(item, m_view->isRightToLeft() ? 1 : -1);
}

int FlowView::itemRight(int item) const
{
    return adjacentColumn(item, m_view->isRightToLeft() ? -1 : 1);
}

int FlowView::pageUp(int item) const
{
    return pageItem(item, -1);
}

int FlowView::pageDown(int item) const
{
    return pageItem(item, 1);
}

QModelIndex FlowView::findIndexAt(const QPoint& point, bool /*incSpans*/, bool /*incPadding*/) const
{
    const int item = itemAtCoordinate(point + QPoint{m_view->horizontalOffset(), m_view->verticalOffset()}, false);
    return item >= 0 ? viewItem(item).index : QModelIndex{};
}

QModelIndex FlowView::nearestSelectionIndex(const QPoint& point) const
{
    QModelIndex nearest;
    std::pair nearestDistance{std::numeric_limits<int>::max(), std::numeric_limits<int>::max()};

    const int count = itemCount();
    for(int i{0}; i < count; ++i) {
        const auto& item = viewItem(i);
        const QRect rect = mapToViewport(item.rect());
        if(!rect.isValid() || m_p->isItemDisabled(i) || (m_view->selectIgnoreParents() && item.hasChildren)) {
            continue;
        }

        // Prefer the same flow column before choosing the nearest row
        const std::pair distance{std::max({rect.left() - point.x(), 0, point.x() - rect.right()}),
                                 std::max({rect.top() - point.y(), 0, point.y() - rect.bottom()})};
        if(distance < nearestDistance) {
            nearestDistance = distance;
            nearest         = item.index;
        }
    }

    return nearest;
}

QModelIndexList FlowView::visibleIndexes(int margin) const
{
    QModelIndexList indexes;

    const auto area  = viewport()
                           ->rect()
                           .adjusted(-margin, -margin, margin, margin)
                           .translated(m_view->horizontalOffset(), m_view->verticalOffset());
    const auto items = itemsInRect(area);
    for(const int item : items) {
        indexes.emplace_back(viewItem(item).index);
    }

    return indexes;
}

void FlowView::drawItem(QPainter* painter, QStyleOptionViewItem option, const QModelIndex& index,
                        const QRect& rect) const
{
    option.rect                   = rect;
    option.showDecorationSelected = true;

    option.state.setFlag(QStyle::State_Selected, m_view->selectionModel()->isSelected(index));
    option.state.setFlag(QStyle::State_MouseOver, index == m_p->m_hoverIndex);
    option.state.setFlag(QStyle::State_HasFocus, m_view->hasFocus() && index == m_view->currentIndex());
    option.state.setFlag(QStyle::State_Enabled, m_view->isEnabled() && (index.flags() & Qt::ItemIsEnabled));

    option.features.setFlag(QStyleOptionViewItem::Alternate, m_view->alternatingRowColors() && index.row() % 2);

    option.palette.setCurrentColorGroup(!(option.state & QStyle::State_Enabled) ? QPalette::Disabled
                                        : option.state & QStyle::State_Active   ? QPalette::Active
                                                                                : QPalette::Inactive);

    delegate(index)->paint(painter, option, index);
}

void FlowView::drawView(QPainter* painter, const QRegion& region) const
{
    QStyleOptionViewItem opt;
    m_view->initViewItemOption(&opt);

    const auto area = region.boundingRect().translated(m_view->horizontalOffset(), m_view->verticalOffset());
    for(const int item : itemsInRect(area)) {
        const auto rect = mapToViewport(viewItem(item).rect());

        if(region.intersects(rect)) {
            drawItem(painter, opt, viewItem(item).index, rect);
        }
    }
}

void FlowView::renderToPixmap(QPainter* painter, const ItemViewPaintPairs& pairs, QRect& rect) const
{
    QStyleOptionViewItem opt;
    m_view->initViewItemOption(&opt);

    for(const auto& pair : pairs) {
        drawItem(painter, opt, pair.index, pair.rect.translated(-rect.topLeft()));
    }
}

void FlowView::dataChanged(const QModelIndex& /*topLeft*/, const QModelIndex& /*bottomRight*/, const QList<int>& roles)
{
    if(roles.empty() || roles.contains(Qt::SizeHintRole)) {
        invalidate();
        m_p->doDelayedItemsLayout();
    }
}

int FlowView::sizeHintForColumn(int /*column*/) const
{
    return m_contentsSize.width();
}

void FlowView::updateScrollBars()
{
    verticalScrollBar()->setRange(0, std::max(0, m_contentsSize.height() - viewport()->height()));
    verticalScrollBar()->setPageStep(viewport()->height());
    verticalScrollBar()->setSingleStep(std::max(1, m_view->fontMetrics().height()));

    horizontalScrollBar()->setRange(0, 0);
}

QSize FlowView::contentSizeForWidth(int width) const
{
    return measuredLayout(width).size;
}

int FlowView::itemHeight(int item) const
{
    return viewItem(item).height;
}

int FlowView::firstVisibleItem(int* offset) const
{
    for(int i{0}; i < itemCount(); ++i) {
        const auto rect = mapToViewport(viewItem(i).rect());
        if(rect.isValid() && rect.intersects(viewport()->rect())) {
            if(offset) {
                *offset = rect.top();
            }
            return i;
        }
    }
    return -1;
}

int FlowView::lastVisibleItem(int /*first*/, int /*offset*/) const
{
    for(int i{itemCount() - 1}; i >= 0; --i) {
        if(mapToViewport(viewItem(i).rect()).intersects(viewport()->rect())) {
            return i;
        }
    }
    return -1;
}

QPoint FlowView::coordinateForItem(int item) const
{
    return viewItem(item).rect().topLeft();
}

int FlowView::adjacentColumn(int item, int direction) const
{
    const int column = m_applied.columns.at(item) + direction;
    const int block  = m_applied.blocks.at(item);

    int nearest{item};

    int distance = std::numeric_limits<int>::max();
    for(int i{0}; i < itemCount(); ++i) {
        if(m_applied.blocks[i] == block && m_applied.columns[i] == column && !m_p->isItemDisabled(i)) {
            const int delta = std::abs(viewItem(i).rect().center().y() - viewItem(item).rect().center().y());
            if(delta < distance) {
                nearest  = i;
                distance = delta;
            }
        }
    }

    return nearest;
}

int FlowView::pageItem(int item, int direction) const
{
    const auto rect = viewItem(item).rect();
    const int y     = rect.center().y() + (direction * viewport()->height());

    int nearest{item};

    int distance = std::numeric_limits<int>::max();
    for(int i{0}; i < itemCount(); ++i) {
        if(viewItem(i).hasChildren || m_p->isItemDisabled(i)) {
            continue;
        }

        const auto target = viewItem(i).rect();
        if(target.left() != rect.left()) {
            continue;
        }

        const int delta = std::abs(target.center().y() - y);
        if(delta < distance) {
            nearest  = i;
            distance = delta;
        }
    }

    return nearest;
}

ExpandedTreeViewPrivate::ExpandedTreeViewPrivate(ExpandedTreeView* self)
    : m_self{self}
    , m_header{new QHeaderView(Qt::Horizontal, m_self)}
    , m_expansionAnimation{this}
{
    QObject::connect(&m_expansionAnimation, &QVariantAnimation::valueChanged, this,
                     &ExpandedTreeViewPrivate::updateExpansionAnimation);
    QObject::connect(&m_expansionAnimation, &QVariantAnimation::finished, this, [this]() {
        stopExpansionAnimation();
        m_self->refreshExpansion();
    });
}

ExpandedTreeViewPrivate::~ExpandedTreeViewPrivate() = default;

void ExpandedTreeViewPrivate::setHeader(QHeaderView* header)
{
    m_header = header;
    m_header->setParent(m_self);

    if(!m_header->model()) {
        m_header->setModel(m_self->model());
        if(m_self->selectionModel()) {
            m_header->setSelectionModel(m_self->selectionModel());
        }
    }

    QObject::connect(m_header, &QHeaderView::sectionResized, this, &ExpandedTreeViewPrivate::columnResized);
    QObject::connect(m_header, &QHeaderView::sectionMoved, this, [this]() { m_self->viewport()->update(); });
    QObject::connect(m_header, &QHeaderView::sectionCountChanged, this, &ExpandedTreeViewPrivate::columnCountChanged);
    QObject::connect(m_header, &QHeaderView::sectionHandleDoubleClicked, this,
                     &ExpandedTreeViewPrivate::resizeColumnToContents);
    QObject::connect(m_header, &QHeaderView::geometriesChanged, this, [this]() { m_self->updateGeometries(); });
    QObject::connect(m_header, &QHeaderView::sortIndicatorChanged, this, [this](int section, Qt::SortOrder order) {
        if(m_sortingEnabled && m_model && section >= 0) {
            m_model->sort(section, order);
        }
    });

    m_header->setSectionsClickable(m_sortingEnabled);
    m_header->setSortIndicatorShown(m_sortingEnabled);

    m_self->updateGeometries();
}

int ExpandedTreeViewPrivate::itemCount() const
{
    return static_cast<int>(m_viewItems.size());
}

QModelIndex ExpandedTreeViewPrivate::modelIndex(int i, int column) const
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

void ExpandedTreeViewPrivate::select(const QModelIndex& topIndex, const QModelIndex& bottomIndex,
                                     QItemSelectionModel::SelectionFlags command) const
{
    QItemSelection selection;
    const int top    = viewIndex(topIndex);
    const int bottom = viewIndex(bottomIndex);

    const auto colRanges = columnRanges(topIndex, bottomIndex);

    for(const auto& [left, right] : colRanges) {
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
    }

    m_self->selectionModel()->select(selection, command);
}

void ExpandedTreeViewPrivate::selectIncludingChildren(const QModelIndex& firstIndex, const QModelIndex& secondIndex,
                                                      QItemSelectionModel::SelectionFlags command) const
{
    const int first  = viewIndex(firstIndex);
    const int second = viewIndex(secondIndex);

    if(first < 0 || second < 0) {
        return;
    }

    const int firstEnd  = first + m_viewItems.at(first).childCount;
    const int secondEnd = second + m_viewItems.at(second).childCount;

    const int top    = std::min(first, second);
    const int bottom = std::max(firstEnd, secondEnd);

    select(modelIndex(top), modelIndex(bottom), command);
}

void ExpandedTreeViewPrivate::resizeColumnToContents(int column) const
{
    layoutItems();

    if(column < 0 || column >= m_header->count()) {
        return;
    }

    const int contents = m_self->sizeHintForColumn(column);
    const int header   = m_header->isHidden() ? 0 : m_header->sectionSizeHint(column);
    m_header->resizeSection(column, std::max(contents, header));
}

void ExpandedTreeViewPrivate::columnCountChanged(int oldCount, int newCount) const
{
    if(oldCount == 0 && newCount > 0) {
        layoutItems();
    }

    if(m_self->isVisible()) {
        m_self->updateGeometries();
    }

    m_self->viewport()->update();
}

void ExpandedTreeViewPrivate::columnResized(int logical, int oldSize, int newSize)
{
    if(m_delayedPendingLayout) {
        return;
    }

    if(m_viewMode == ExpandedTreeView::ViewMode::Tree) {
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

void ExpandedTreeViewPrivate::doDelayedItemsLayout(int delay) const
{
    m_delayedPendingLayout = true;
    m_delayedLayout.start(delay, m_self);
}

void ExpandedTreeViewPrivate::interruptDelayedItemsLayout() const
{
    m_delayedLayout.stop();
    m_delayedPendingLayout = false;
}

void ExpandedTreeViewPrivate::layoutItems() const
{
    if(m_delayedPendingLayout) {
        interruptDelayedItemsLayout();
        m_self->doItemsLayout();
    }
}

ViewportAnchor ExpandedTreeViewPrivate::viewportAnchor() const
{
    if(m_viewMode != ViewMode::Icon || m_viewItems.empty()) {
        return {};
    }

    const int scroll     = m_self->verticalScrollBar()->value();
    const auto isVisible = [this, scroll](const QRect& rect) {
        return rect.isValid() && rect.bottom() >= scroll && rect.top() < scroll + m_self->viewport()->height();
    };

    if(m_expansionWidget && m_expandedIndex.isValid() && isVisible(m_expansionRect)) {
        return {.index = m_expandedIndex, .top = m_expansionRect.top() - scroll, .expansion = true};
    }

    const QModelIndex current = m_self->currentIndex();
    const int currentItem     = viewIndex(current);
    if(currentItem >= 0 && isRowSelected(current) && isVisible(m_viewItems.at(currentItem).rect())) {
        return {.index = current, .top = m_viewItems.at(currentItem).y - scroll};
    }

    const auto visible = m_view->visibleIndexes(0);
    if(visible.isEmpty()) {
        return {};
    }

    QModelIndex anchor{visible.front()};
    for(const auto& index : visible) {
        if(isRowSelected(index)) {
            anchor = index;
            break;
        }
    }

    return {.index = anchor, .top = m_viewItems.at(viewIndex(anchor)).y - scroll};
}

ExpandedTreeViewItem ExpandedTreeViewPrivate::indexToListViewItem(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return {};
    }

    return m_view->indexToViewItem(index);
}

int ExpandedTreeViewPrivate::viewIndex(const QModelIndex& index) const
{
    if(!index.isValid() || m_viewItems.empty()) {
        return -1;
    }

    const QModelIndex firstIndex = index.sibling(index.row(), 0);
    const auto item              = m_viewIndexes.find(firstIndex);
    return item != m_viewIndexes.cend() ? item->second : -1;
}

void ExpandedTreeViewPrivate::clearViewItems()
{
    m_viewIndexes.clear();
    m_viewItems.clear();
    if(m_view) {
        m_view->invalidate();
    }
}

void ExpandedTreeViewPrivate::insertViewItems(int pos, int count, const ExpandedTreeViewItem& viewItem)
{
    m_viewItems.insert(m_viewItems.begin() + pos, count, viewItem);

    const int itmCount = itemCount();
    for(int i{pos + count}; i < itmCount; i++) {
        m_viewIndexes.insert_or_assign(m_viewItems.at(i).index, i);
        if(m_viewItems.at(i).parentItem >= pos) {
            m_viewItems.at(i).parentItem += count;
        }
    }
}

bool ExpandedTreeViewPrivate::hasVisibleChildren(const QModelIndex& parent) const
{
    if(parent.flags() & Qt::ItemNeverHasChildren) {
        return false;
    }

    return m_model->hasChildren(parent);
}

bool ExpandedTreeViewPrivate::isIndexEnabled(const QModelIndex& index) const
{
    return m_model->flags(index) & Qt::ItemIsEnabled;
}

bool ExpandedTreeViewPrivate::isItemDisabled(int i) const
{
    if(i < 0 || i >= itemCount()) {
        return false;
    }

    const QModelIndex index = m_viewItems.at(i).index;
    return !isIndexEnabled(index);
}

bool ExpandedTreeViewPrivate::itemHasChildren(int i) const
{
    if(i < 0 || i >= itemCount()) {
        return false;
    }

    return m_viewItems.at(i).hasChildren;
}

void ExpandedTreeViewPrivate::invalidateHeightCache(int item) const
{
    m_viewItems[item].height = 0;
}

int ExpandedTreeViewPrivate::itemForHomeKey() const
{
    int index{0};
    while(isItemDisabled(index) || itemHasChildren(index)) {
        index++;
    }
    return index >= itemCount() ? 0 : index;
}

int ExpandedTreeViewPrivate::itemForEndKey() const
{
    int index = itemCount() - 1;
    while(isItemDisabled(index)) {
        index--;
    }
    return index == -1 ? itemCount() - 1 : index;
}

void ExpandedTreeViewPrivate::setHoverIndex(const QPersistentModelIndex& index)
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

int ExpandedTreeViewPrivate::firstVisibleColumn(const QModelIndex& parent) const
{
    if(!m_header) {
        return -1;
    }

    const int columnCount = m_model ? m_model->columnCount(parent) : 0;
    const int headerCount = std::min(m_header->count(), columnCount);

    for(int visual{0}; visual < headerCount; ++visual) {
        const int logical = m_header->logicalIndex(visual);
        if(logical >= 0 && logical < columnCount && !m_header->isSectionHidden(logical)) {
            return logical;
        }
    }

    return -1;
}

QModelIndex ExpandedTreeViewPrivate::iconItemIndex(const QModelIndex& index) const
{
    if(!isIndexValid(index)) {
        return {};
    }

    const QModelIndex parent = index.parent();
    const int columnCount    = m_model ? m_model->columnCount(parent) : 0;

    if(m_iconItemColumn >= 0 && m_iconItemColumn < columnCount) {
        return index.siblingAtColumn(m_iconItemColumn);
    }

    const int firstVisible = firstVisibleColumn(parent);
    if(firstVisible >= 0) {
        return index.siblingAtColumn(firstVisible);
    }

    return index.siblingAtColumn(0);
}

bool ExpandedTreeViewPrivate::isIndexDropEnabled(const QModelIndex& index) const
{
    return m_model->flags(index) & Qt::ItemIsDropEnabled;
}

bool ExpandedTreeViewPrivate::isRowSelected(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return false;
    }

    const QModelIndex parent = index.parent();
    const int row            = index.row();

    const auto selection = m_self->selectionModel()->selection();
    return std::ranges::any_of(selection, [&](const auto& range) {
        return range.isValid() && range.parent() == parent && row >= range.top() && row <= range.bottom();
    });
}

QModelIndexList ExpandedTreeViewPrivate::selectedDraggableIndexes(bool fullRow) const
{
    QModelIndexList indexes;

    if(fullRow) {
        const auto selection = m_self->selectionModel()->selection();

        qsizetype indexCount{0};
        for(const QItemSelectionRange& range : selection) {
            if(range.isValid()) {
                indexCount += range.bottom() - range.top() + 1;
            }
        }
        indexes.reserve(indexCount);

        const bool mayOverlap = selection.size() > 1;
        std::unordered_set<QModelIndex, ModelIndexHash> seenIndexes;
        if(mayOverlap) {
            seenIndexes.reserve(indexCount);
        }

        for(const QItemSelectionRange& range : selection) {
            if(!range.isValid()) {
                continue;
            }

            const QModelIndex parent = range.parent();
            const int column         = std::max(0, firstVisibleColumn(parent));

            for(int row{range.top()}; row <= range.bottom(); ++row) {
                const QModelIndex index = m_model->index(row, column, parent);
                if(index.isValid() && (m_model->flags(index) & Qt::ItemIsDragEnabled)
                   && (!mayOverlap || seenIndexes.emplace(index).second)) {
                    indexes.append(index);
                }
            }
        }
    }
    else {
        const QModelIndexList visibleIndexes = m_view->visibleIndexes(0);
        indexes.reserve(visibleIndexes.size() * m_header->count());

        for(const QModelIndex& visibleIndex : visibleIndexes) {
            const QModelIndex parent = visibleIndex.parent();
            const int columnCount    = m_model->columnCount(parent);

            for(int column{0}; column < columnCount; ++column) {
                const QModelIndex index = visibleIndex.siblingAtColumn(column);
                if(!m_self->isIndexHidden(index) && m_self->selectionModel()->isSelected(index)
                   && (m_model->flags(index) & Qt::ItemIsDragEnabled)) {
                    indexes.append(index);
                }
            }
        }
    }

    return indexes;
}

bool ExpandedTreeViewPrivate::shouldAutoScroll() const
{
    const QRect area       = m_self->viewport()->rect();
    const int scrollMargin = m_self->autoScrollMargin();

    return (m_dragPos.y() - area.top() < scrollMargin) || (area.bottom() - m_dragPos.y() < scrollMargin)
        || (m_dragPos.x() - area.left() < scrollMargin) || (area.right() - m_dragPos.x() < scrollMargin);
}

void ExpandedTreeViewPrivate::startAutoScroll()
{
    m_autoScrollTimer.start(50, m_self);
    m_autoScrollCount = 0;
}

void ExpandedTreeViewPrivate::stopAutoScroll()
{
    m_autoScrollTimer.stop();
    m_autoScrollCount = 0;
}

void ExpandedTreeViewPrivate::doAutoScroll()
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
        m_dropIndicatorPos  = ExpandedTreeView::OnViewport;
    }
}

QModelIndex ExpandedTreeViewPrivate::indexAtDropPosition(const QPoint& pos) const
{
    QModelIndex index = m_self->findIndexAt(pos, true);
    if(index.isValid() || m_viewMode != ViewMode::Icon || m_viewItems.empty()) {
        return index;
    }

    layoutItems();

    qint64 nearestDistance               = std::numeric_limits<qint64>::max();
    const QModelIndexList visibleIndexes = m_view->visibleIndexes(0);
    for(const QModelIndex& visibleIndex : visibleIndexes) {
        const QRect rect = m_view->visualRect(visibleIndex, RectRule::FullRow, false);
        if(!rect.isValid()) {
            continue;
        }

        const int dx          = std::max({rect.left() - pos.x(), 0, pos.x() - rect.right()});
        const int dy          = std::max({rect.top() - pos.y(), 0, pos.y() - rect.bottom()});
        const qint64 distance = (static_cast<qint64>(dx) * dx) + (static_cast<qint64>(dy) * dy);
        if(distance < nearestDistance) {
            nearestDistance = distance;
            index           = iconItemIndex(visibleIndex);
        }
    }

    return index;
}

bool ExpandedTreeViewPrivate::dropOn(QDropEvent* event, int& dropRow, int& dropCol, QModelIndex& dropIndex)
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
            case ExpandedTreeView::AboveItem:
                row   = index.row();
                col   = index.column();
                index = index.parent();
                break;
            case ExpandedTreeView::BelowItem:
                row   = index.row() + 1;
                col   = index.column();
                index = index.parent();
                break;
            case ExpandedTreeView::OnItem:
            case ExpandedTreeView::OnViewport:
                break;
        }
    }
    else {
        m_dropIndicatorPos = ExpandedTreeView::OnViewport;
        row                = m_model->rowCount({});
    }

    dropIndex = index;
    dropRow   = row;
    dropCol   = col;

    return true;
}

std::vector<std::pair<int, int>> ExpandedTreeViewPrivate::columnRanges(const QModelIndex& topIndex,
                                                                       const QModelIndex& bottomIndex) const
{
    const int topVisual    = m_header->visualIndex(topIndex.column());
    const int bottomVisual = m_header->visualIndex(bottomIndex.column());

    const int start = qMin(topVisual, bottomVisual);
    const int end   = qMax(topVisual, bottomVisual);

    std::vector<int> logicalIndexes;

    for(int c{start}; c <= end; c++) {
        const int logical = m_header->logicalIndex(c);
        if(!m_header->isSectionHidden(logical)) {
            logicalIndexes.emplace_back(logical);
        }
    }
    std::ranges::sort(logicalIndexes);

    std::vector<std::pair<int, int>> ret;
    std::pair<int, int> current{-2, -2};
    const auto count = static_cast<int>(logicalIndexes.size());

    for(int i{0}; i < count; ++i) {
        const int logicalColumn = logicalIndexes.at(i);
        if(current.second + 1 != logicalColumn) {
            if(current.first != -2) {
                ret.emplace_back(current);
            }
            current.first = current.second = logicalColumn;
        }
        else {
            current.second++;
        }
    }

    if(current.first != -2) {
        ret.emplace_back(current);
    }

    return ret;
}

std::vector<QRect> ExpandedTreeViewPrivate::rectsToPaint(const QModelIndex& index, const QStyleOptionViewItem& option,
                                                         int y) const
{
    std::vector<QRect> rects;

    QRect currRect{0, y, 0, option.rect.height()};

    const int offset = m_header->offset();
    const int count  = m_header->count();
    const int first  = Utils::firstVisualIndex(m_header);

    int level{0};
    const int viewIdx = viewIndex(index);
    if(viewIdx >= 0) {
        level = m_viewItems.at(viewIdx).level;
    }

    for(int section{0}; section < count; ++section) {
        const int logical = m_header->logicalIndex(section);
        int position      = m_header->sectionPosition(logical) - offset;

        if(m_header->isSectionHidden(logical)) {
            continue;
        }

        int indent{0};
        if(section == first) {
            indent = level * m_indent;
            position += indent;
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
            currRect.setWidth(position + m_header->sectionSize(logical) - currRect.x());
        }
    }

    if(currRect.width() > 0) {
        rects.emplace_back(currRect);
    }

    return rects;
}

void ExpandedTreeViewPrivate::updateExpansionGeometry()
{
    if(!m_expansionWidget || m_layingOutItems) {
        return;
    }

    const bool animating = m_expansionAnimationHeight >= 0;
    const auto index     = animating ? m_expansionAnimationIndex : m_expandedIndex;
    const bool expanded  = m_viewMode == ViewMode::Icon && index.isValid() && m_expansionRect.isValid();
    if(expanded) {
        const int item = viewIndex(index);
        if(item >= 0) {
            const QRect anchor = m_view->mapToViewport(m_viewItems.at(item).rect());
            m_expansionWidget->setProperty("expansionAnchor", anchor.center().x());
        }
        if(!animating) {
            m_expansionWidget->setGeometry(m_expansionRect.translated(0, -m_self->verticalScrollBar()->value()));
            m_expansionWidget->update();
            m_expansionWidget->raise();
        }
    }

    m_expansionWidget->setVisible(expanded && !animating);
}

void ExpandedTreeViewPrivate::updateExpansionAnimation(const QVariant& value)
{
    if(!m_expansionAnimationIndex.isValid()) {
        stopExpansionAnimation();
        m_self->refreshExpansion();
        return;
    }

    const QRect anchor = m_self->visualRect(m_expandedIndex);

    if(m_expansionAnimationNextIndex.isValid()
       && m_expansionAnimation.currentTime() >= m_expansionAnimation.duration() / 2) {
        m_expansionAnimationIndex = m_expansionAnimationNextIndex;
    }

    m_expansionAnimationHeight = value.toInt();
    m_self->refreshExpansion();

    auto* bar = m_self->verticalScrollBar();
    if(m_expansionAnimationNextIndex.isValid() && anchor.isValid()) {
        const QRect updated = m_self->visualRect(m_expandedIndex);
        bar->setValue(bar->value() + updated.top() - anchor.top());
    }

    if(m_expandedIndex.isValid()) {
        // Artwork and prepared backgrounds can arrive after the opening animation starts
        m_expansionSnapshot = captureExpansion();
        const QRect icon    = m_self->visualRect(m_expandedIndex);
        const int scroll = m_expansionAnimationIndex == m_expandedIndex
                             ? std::clamp(m_expansionRect.bottom() + 1 - bar->value() - m_self->viewport()->height(), 0,
                                          std::max(0, icon.top()))
                             : 0;
        bar->setValue(bar->value() + scroll);
    }
    else {
        bar->setValue(
            std::min(bar->value(), std::max(0, m_view->m_contentsSize.height() - m_self->viewport()->height())));
    }
}

QPixmap ExpandedTreeViewPrivate::captureExpansion() const
{
    const qreal dpr = m_expansionWidget->devicePixelRatioF();
    QPixmap snapshot{m_expansionWidget->size() * dpr};
    snapshot.setDevicePixelRatio(dpr);
    snapshot.fill(Qt::transparent);
    m_expansionWidget->render(&snapshot, {}, {}, QWidget::DrawChildren);
    return snapshot;
}

const QPixmap& ExpandedTreeViewPrivate::animationSnapshot() const
{
    return m_expansionAnimationNextIndex.isValid() && m_expansionAnimationIndex != m_expansionAnimationNextIndex
             ? m_expansionPreviousSnapshot
             : m_expansionSnapshot;
}

void ExpandedTreeViewPrivate::stopExpansionAnimation()
{
    m_expansionAnimation.stop();
    m_expansionAnimationIndex     = QModelIndex{};
    m_expansionAnimationNextIndex = QModelIndex{};
    m_expansionSnapshot           = {};
    m_expansionPreviousSnapshot   = {};
    m_expansionAnimationHeight    = -1;
}

QPoint ExpandedTreeViewPrivate::offset() const
{
    return {m_self->isRightToLeft() ? -m_self->horizontalOffset() : m_self->horizontalOffset(),
            m_self->verticalOffset()};
}

bool ExpandedTreeViewPrivate::isIndexValid(const QModelIndex& index) const
{
    return (index.row() >= 0) && (index.column() >= 0) && (index.model() == m_model);
}

ExpandedTreeView::ExpandedTreeView(QWidget* parent)
    : QAbstractItemView{parent}
    , p{std::make_unique<ExpandedTreeViewPrivate>(this)}
{
    setObjectName(u"ExpandedTreeView"_s);

    viewport()->setAttribute(Qt::WA_Hover, true);

    p->setHeader(p->m_header);
    setViewMode(ViewMode::Tree);

    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
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

    p->setHeader(header);
}

bool ExpandedTreeView::isHeaderHidden() const
{
    return p->m_header->isHidden();
}

void ExpandedTreeView::setHeaderHidden(bool hide)
{
    if(p->m_header->isHidden() == hide) {
        return;
    }

    p->m_header->setHidden(hide);
    updateGeometries();
    viewport()->update();
}

void ExpandedTreeView::setModel(QAbstractItemModel* model)
{
    if(!model || p->m_model == model) {
        return;
    }

    if(p->m_model) {
        QObject::disconnect(p->m_model, nullptr, this, nullptr);
    }

    p->clearViewItems();
    p->m_updatingGeometry = true;
    p->m_header->setModel(model);
    p->m_updatingGeometry = false;
    p->m_model            = model;
    QAbstractItemView::setModel(model);

    QObject::disconnect(p->m_model, &QAbstractItemModel::rowsRemoved, this, nullptr);
    // Do header layout after the tree
    QObject::disconnect(p->m_model, &QAbstractItemModel::layoutChanged, p->m_header, nullptr);

    QObject::connect(p->m_model, &QAbstractItemModel::modelAboutToBeReset, this, [this]() {
        p->interruptDelayedItemsLayout();
        p->clearViewItems();
    });
    QObject::connect(p->m_model, &QAbstractItemModel::rowsRemoved, this, &ExpandedTreeView::rowsRemoved);

    if(p->m_sortingEnabled) {
        sortByColumn(p->m_header->sortIndicatorSection(), p->m_header->sortIndicatorOrder());
    }
}

bool ExpandedTreeView::isSortingEnabled() const
{
    return p->m_sortingEnabled;
}

void ExpandedTreeView::setSortingEnabled(bool enabled)
{
    if(std::exchange(p->m_sortingEnabled, enabled) == enabled) {
        return;
    }

    p->m_header->setSectionsClickable(enabled);
    p->m_header->setSortIndicatorShown(enabled);

    if(enabled) {
        sortByColumn(p->m_header->sortIndicatorSection(), p->m_header->sortIndicatorOrder());
    }
}

void ExpandedTreeView::sortByColumn(int column, Qt::SortOrder order)
{
    p->m_header->setSortIndicator(column, order);

    if(p->m_sortingEnabled && p->m_model && column >= 0) {
        p->m_model->sort(column, order);
    }
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

bool ExpandedTreeView::extendSpansIntoParents() const
{
    return p->m_extendSpansIntoParents;
}

void ExpandedTreeView::setExtendSpansIntoParents(bool enabled)
{
    if(std::exchange(p->m_extendSpansIntoParents, enabled) != enabled) {
        viewport()->update();
    }
}

ExpandedTreeView::ViewMode ExpandedTreeView::viewMode() const
{
    return p->m_viewMode;
}

void ExpandedTreeView::setViewMode(ViewMode mode)
{
    if(p->m_view && p->m_viewMode == mode) {
        return;
    }

    p->stopExpansionAnimation();
    p->interruptDelayedItemsLayout();
    p->clearViewItems();
    p->m_viewMode      = mode;
    p->m_expansionRect = {};
    p->updateExpansionGeometry();

    p->m_view.reset();

    if(mode == ViewMode::Tree) {
        p->m_view = std::make_unique<TreeView>(this, p.get());
    }
    else if(mode == ViewMode::Icon) {
        p->m_view = std::make_unique<IconView>(this, p.get());
    }
    else {
        p->m_view = std::make_unique<FlowView>(this, p.get());
        setVerticalScrollMode(ScrollPerPixel);
    }

    p->doDelayedItemsLayout();

    Q_EMIT viewModeChanged(mode);
}

void ExpandedTreeView::setFlowColumnGap(int gap)
{
    gap = std::max(0, gap);
    if(std::exchange(p->m_flowColumnGap, gap) != gap && p->m_viewMode == ViewMode::Flow) {
        p->m_view->invalidate();
        p->doDelayedItemsLayout();
    }
}

void ExpandedTreeView::setFlowMinimumRows(int rows)
{
    rows = std::max(1, rows);
    if(std::exchange(p->m_flowMinimumRows, rows) != rows && p->m_viewMode == ViewMode::Flow) {
        p->m_view->invalidate();
        p->doDelayedItemsLayout();
    }
}

void ExpandedTreeView::setFlowMaximumColumns(int columns)
{
    columns = std::max(0, columns);
    if(std::exchange(p->m_flowMaximumColumns, columns) != columns && p->m_viewMode == ViewMode::Flow) {
        p->m_view->invalidate();
        p->doDelayedItemsLayout();
    }
}

void ExpandedTreeView::setFlowMinimumColumnWidth(int width)
{
    width = std::max(0, width);
    if(std::exchange(p->m_flowMinimumColumnWidth, width) != width && p->m_viewMode == ViewMode::Flow) {
        p->m_view->invalidate();
        p->doDelayedItemsLayout();
    }
}

void ExpandedTreeView::setFlowStretchColumns(bool stretch)
{
    if(std::exchange(p->m_flowStretchColumns, stretch) != stretch && p->m_viewMode == ViewMode::Flow) {
        p->m_view->invalidate();
        p->doDelayedItemsLayout();
    }
}

QSize ExpandedTreeView::contentSizeForWidth(int width) const
{
    return p->m_view->contentSizeForWidth(std::max(1, width));
}

ExpandedTreeView::CaptionDisplay ExpandedTreeView::captionDisplay() const
{
    return p->m_captionDisplay;
}

void ExpandedTreeView::setCaptionDisplay(CaptionDisplay display)
{
    if(std::exchange(p->m_captionDisplay, display) == display) {
        return;
    }

    p->doDelayedItemsLayout();
    viewport()->update();
}

int ExpandedTreeView::iconItemColumn() const
{
    return p->m_iconItemColumn;
}

void ExpandedTreeView::setIconItemColumn(int column)
{
    if(p->m_iconItemColumn == column) {
        return;
    }

    p->m_iconItemColumn = column;
    p->doDelayedItemsLayout();
    viewport()->update();
}

bool ExpandedTreeView::uniformRowHeights() const
{
    return p->m_uniformRowHeights;
}

void ExpandedTreeView::setUniformRowHeights(bool enabled)
{
    p->m_uniformRowHeights = enabled;
}

int ExpandedTreeView::uniformHeightRole() const
{
    return p->m_uniformHeightRole;
}

void ExpandedTreeView::setUniformHeightRole(int role)
{
    p->m_uniformHeightRole = role;
}

bool ExpandedTreeView::selectIgnoreParents() const
{
    return p->m_selectIgnoreParents;
}

void ExpandedTreeView::setSelectIgnoreParents(bool enabled)
{
    p->m_selectIgnoreParents = enabled;
}

int ExpandedTreeView::indentation() const
{
    return p->m_indent;
}

void ExpandedTreeView::setIndentation(int indent)
{
    if(std::exchange(p->m_indent, indent) != indent) {
        viewport()->update();
    }
}

void ExpandedTreeView::resetIndentation()
{
    p->m_indent = 0;
}

bool ExpandedTreeView::selectBeforeDrag() const
{
    return p->m_selectBeforeDrag;
}

void ExpandedTreeView::setSelectBeforeDrag(bool enabled)
{
    p->m_selectBeforeDrag = enabled;
}

int ExpandedTreeView::iconHorizontalGap() const
{
    return p->m_iconHorizontalGap;
}

void ExpandedTreeView::setIconHorizontalGap(int gap)
{
    gap = std::max(gap, -1);

    if(std::exchange(p->m_iconHorizontalGap, gap) == gap) {
        return;
    }

    p->doDelayedItemsLayout();
    viewport()->update();
}

int ExpandedTreeView::iconVerticalGap() const
{
    return p->m_iconVerticalGap;
}

void ExpandedTreeView::setIconVerticalGap(int gap)
{
    gap = std::max(gap, 0);

    if(std::exchange(p->m_iconVerticalGap, gap) == gap) {
        return;
    }

    p->doDelayedItemsLayout();
    viewport()->update();
}

bool ExpandedTreeView::useIconGapsForSideCaptions() const
{
    return p->m_useIconGapsForSideCaptions;
}

void ExpandedTreeView::setUseIconGapsForSideCaptions(bool enabled)
{
    if(std::exchange(p->m_useIconGapsForSideCaptions, enabled) == enabled) {
        return;
    }

    p->doDelayedItemsLayout();
    viewport()->update();
}

void ExpandedTreeView::changeIconSize(const QSize& size)
{
    if(iconSize() != size) {
        p->m_preserveLayoutPosition = true;
        setIconSize(size);
        p->doDelayedItemsLayout(100);
    }
}

QModelIndex ExpandedTreeView::expandedIndex() const
{
    return p->m_expandedIndex;
}

void ExpandedTreeView::prepareExpansionChange()
{
    if(!p->m_expansionAnimated || !p->m_expandedIndex.isValid() || !p->m_expansionWidget) {
        return;
    }
    // Preserve the old contents before switching
    if(p->m_expansionAnimationNextIndex.isValid() && p->m_expansionAnimationIndex != p->m_expansionAnimationNextIndex) {
        return;
    }
    p->m_expansionPreviousSnapshot = p->captureExpansion();
}

void ExpandedTreeView::setExpandedIndex(const QModelIndex& index)
{
    const QModelIndex expanded = index.isValid() && index.model() == model() ? index.siblingAtColumn(0) : QModelIndex{};
    if(p->m_expandedIndex == expanded) {
        return;
    }

    const QRect anchor         = visualRect(expanded);
    const QModelIndex previous = p->m_expansionAnimationHeight >= 0 ? p->m_expansionAnimationIndex : p->m_expandedIndex;
    const int previousHeight   = std::max(0, p->m_expansionRect.height());
    const bool opening         = expanded.isValid() && !p->m_expandedIndex.isValid();
    const bool closing         = !expanded.isValid() && previous.isValid() && previousHeight > 0;
    const bool relocating      = expanded.isValid() && p->m_expandedIndex.isValid() && anchor.isValid()
                              && visualRect(p->m_expandedIndex).top() != anchor.top();
    const int duration = std::clamp(style()->styleHint(QStyle::SH_Widget_Animation_Duration, nullptr, this), 0, 200);
    const bool animate = p->m_expansionAnimated && isVisible() && p->m_viewMode == ViewMode::Icon
                      && p->m_expansionWidget && duration > 0
                      && style()->styleHint(QStyle::SH_Widget_Animate, nullptr, this)
                      && (opening || closing || relocating);

    QPixmap snapshot;
    QPixmap previousSnapshot;
    if(animate && relocating) {
        previousSnapshot
            = p->m_expansionPreviousSnapshot.isNull() ? p->captureExpansion() : p->m_expansionPreviousSnapshot;
    }
    if(animate && !expanded.isValid()) {
        snapshot = p->m_expansionAnimationHeight >= 0 ? p->animationSnapshot() : p->captureExpansion();
    }

    p->stopExpansionAnimation();
    p->m_expandedIndex = expanded;
    if(expanded.isValid() || !animate) {
        refreshExpansion();
    }

    const int targetHeight = expanded.isValid() ? std::max(0, p->m_expansionRect.height()) : 0;
    if(animate && expanded.isValid() && targetHeight > 0) {
        snapshot = p->captureExpansion();
    }

    if(animate && !snapshot.isNull()) {
        p->m_expansionSnapshot           = std::move(snapshot);
        p->m_expansionPreviousSnapshot   = std::move(previousSnapshot);
        p->m_expansionAnimationViewSize  = size();
        p->m_expansionAnimationScrollMax = verticalScrollBar()->maximum();
        p->m_expansionAnimationIndex     = relocating || !expanded.isValid() ? previous : expanded;
        p->m_expansionAnimationNextIndex = relocating ? expanded : QModelIndex{};
        p->m_expansionAnimationHeight = relocating || !expanded.isValid() || expanded == previous ? previousHeight : 0;

        {
            const QSignalBlocker blocker{&p->m_expansionAnimation};
            p->m_expansionAnimation.setDuration(duration);
            p->m_expansionAnimation.setEasingCurve(relocating           ? QEasingCurve::InOutCubic
                                                   : expanded.isValid() ? QEasingCurve::OutCubic
                                                                        : QEasingCurve::InCubic);
            p->m_expansionAnimation.setKeyValues({{0.0, p->m_expansionAnimationHeight}, {1.0, targetHeight}});
            if(relocating) {
                p->m_expansionAnimation.setKeyValueAt(0.5, 0);
            }
            p->m_expansionAnimation.setCurrentTime(0);
        }
        p->m_expansionAnimation.start();
    }

    refreshExpansion();

    if(anchor.isValid()) {
        const QRect updated = visualRect(expanded);
        verticalScrollBar()->setValue(verticalScrollBar()->value() + updated.top() - anchor.top());
    }
}

void ExpandedTreeView::setExpansionWidget(QWidget* widget)
{
    if(p->m_expansionWidget == widget) {
        return;
    }

    p->stopExpansionAnimation();
    if(p->m_expansionWidget) {
        p->m_expansionWidget->hide();
    }
    p->m_expansionWidget = widget;

    if(widget) {
        widget->setParent(viewport());
        widget->hide();
    }

    if(p->m_expandedIndex.isValid() || p->m_expansionRect.isValid()) {
        refreshExpansion();
    }
}

void ExpandedTreeView::setExpansionAnimated(bool enabled)
{
    p->m_expansionAnimated = enabled;
    if(!enabled && p->m_expansionAnimationHeight >= 0) {
        p->stopExpansionAnimation();
        refreshExpansion();
    }
}

void ExpandedTreeView::refreshExpansion()
{
    executeDelayedItemsLayout();
    const bool reuseMeasurements = !p->m_delayedPendingLayout;
    p->interruptDelayedItemsLayout();
    p->doItemsLayout(reuseMeasurements);
    viewport()->update();
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

    if(const auto* treeModel = qobject_cast<TreeModelBase*>(p->m_model); treeModel && (treeModel->isDirty())) {
        // Prevent scrolling when invalidating model
        return;
    }

    if(state() == DraggingState || state() == DragSelectingState) {
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

    int item = p->viewIndex(index);
    if(item < 0) {
        return;
    }

    const QRect area = viewport()->rect();

    if(p->m_viewMode == ViewMode::Tree && verticalScrollMode() == ScrollPerItem) {
        const int top    = verticalScrollBar()->value();
        const int bottom = top + verticalScrollBar()->pageStep();
        if(hint == EnsureVisible && item >= top && item < bottom) {
            // Already visible
        }
        else if(hint == PositionAtTop || (hint == EnsureVisible && item < top)) {
            verticalScrollBar()->setValue(item);
        }
        else { // PositionAtBottom or PositionAtCenter
            const int currentItemHeight = p->m_view->itemHeight(item);
            int y = (hint == PositionAtCenter ? (area.height() / 2) + currentItemHeight - 1 : area.height());
            if(y > currentItemHeight) {
                while(item >= 0) {
                    y -= p->m_view->itemHeight(item);
                    if(y < 0) {
                        item++;
                        break;
                    }
                    item--;
                }
            }
            verticalScrollBar()->setValue(item);
        }
    }
    else if(!rect.isEmpty()) {
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

QModelIndexList ExpandedTreeView::visibleIndexes(int margin) const
{
    p->layoutItems();

    return p->m_view->visibleIndexes(margin);
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
    if(i >= p->itemCount()) {
        return {};
    }

    const QModelIndex firstIndex = p->m_viewItems.at(i).index;
    return firstIndex.sibling(firstIndex.row(), index.column());
}

void ExpandedTreeView::doItemsLayout()
{
    p->doItemsLayout(false);
}

void ExpandedTreeViewPrivate::doItemsLayout(bool reuseIconMeasurements)
{
    if(m_layingOutItems) {
        return;
    }

    auto* iconView        = m_viewMode == ViewMode::Icon ? static_cast<IconView*>(m_view.get()) : nullptr;
    reuseIconMeasurements = reuseIconMeasurements && iconView && iconView->canReuseMeasurements();

    const auto anchor        = std::exchange(m_preserveLayoutPosition, false) ? viewportAnchor() : ViewportAnchor{};
    const QSize previousSize = m_view->m_contentsSize;
    m_layingOutItems         = true;
    if(!reuseIconMeasurements) {
        clearViewItems();
    }

    if(reuseIconMeasurements) {
        iconView->doItemLayout(true);
    }
    else if(m_model && m_model->hasChildren(m_self->rootIndex())) {
        m_view->doItemLayout();
    }
    else {
        m_view->m_contentsSize = {};
    }

    m_self->QAbstractItemView::doItemsLayout();

    m_header->doItemsLayout();
    m_layingOutItems = false;

    if(anchor.index.isValid()) {
        const int item = viewIndex(anchor.index);
        if(item >= 0) {
            const int top = anchor.expansion && m_expandedIndex == anchor.index && m_expansionRect.isValid()
                              ? m_expansionRect.top()
                              : m_viewItems.at(item).y;
            m_self->verticalScrollBar()->setValue(top - anchor.top);
        }
    }

    updateExpansionGeometry();

    if(previousSize != m_view->m_contentsSize) {
        Q_EMIT m_self->contentSizeChanged();
    }
}

void ExpandedTreeView::reset()
{
    p->stopExpansionAnimation();
    p->m_preserveLayoutPosition = false;
    p->interruptDelayedItemsLayout();
    p->clearViewItems();
    QAbstractItemView::reset();
    p->doDelayedItemsLayout();
}

void ExpandedTreeView::selectAll()
{
    if(!selectionModel()) {
        return;
    }

    p->layoutItems();

    const SelectionMode mode = selectionMode();
    if(mode != SingleSelection && mode != NoSelection && !p->m_viewItems.empty()) {
        const QModelIndex& first        = p->m_viewItems.front().index;
        const QModelIndex& last         = p->m_viewItems.back().index;
        const QModelIndex lastItemIndex = last.sibling(last.row(), p->m_model->columnCount(last.parent()) - 1);
        static constexpr auto command   = QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows;

        if(first.parent() == last.parent()) {
            selectionModel()->select({first, lastItemIndex}, command);
        }
        else {
            p->select(first, lastItemIndex, command);
        }
    }
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

    if(!p->m_layingOutItems && p->m_hidingScrollbar > 0) {
        --p->m_hidingScrollbar;
    }
    else if(p->m_view) {
        p->m_view->updateScrollBars();
    }

    QAbstractItemView::updateGeometries();

    const QRect geometry = viewport()->geometry();
    const QRect headerGeometry{geometry.left(), geometry.top() - height, geometry.width(), height};

    p->m_header->setGeometry(headerGeometry);
    QMetaObject::invokeMethod(p->m_header, "updateGeometries");

    if(p->m_view) {
        p->m_view->updateScrollBars();
    }

    p->m_updatingGeometry = false;
    p->updateExpansionGeometry();
}

void ExpandedTreeView::dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles)
{
    p->m_view->dataChanged(topLeft, bottomRight, roles);

    QAbstractItemView::dataChanged(topLeft, bottomRight, roles);

    if(!p->m_extendSpansIntoParents) {
        return;
    }

    const int firstColumn = std::min(topLeft.column(), bottomRight.column());
    const int lastColumn  = std::max(topLeft.column(), bottomRight.column());

    const auto updateExtendedSpanParents = [this](const QModelIndex& index) {
        if(!index.isValid() || !index.parent().isValid()) {
            return;
        }

        QModelIndex topLevelContainer = index.parent();
        while(topLevelContainer.parent().isValid()) {
            topLevelContainer = topLevelContainer.parent();
        }

        QModelIndex firstLeaf = topLevelContainer;
        while(model()->hasChildren(firstLeaf)) {
            firstLeaf = model()->index(0, 0, firstLeaf);
        }

        if(index.row() != firstLeaf.row() || index.parent() != firstLeaf.parent()) {
            return;
        }

        for(QModelIndex ancestor = index.parent(); ancestor.parent().isValid(); ancestor = ancestor.parent()) {
            viewport()->update(p->m_view->visualRect(ancestor, RectRule::FullRow, false));
        }
    };

    for(int column = firstColumn; column <= lastColumn; ++column) {
        if(isSpanning(column)) {
            updateExtendedSpanParents(topLeft);
            return;
        }
    }
}

void ExpandedTreeView::changeEvent(QEvent* event)
{
    QAbstractItemView::changeEvent(event);
    if(p->m_viewMode == ViewMode::Flow
       && (event->type() == QEvent::LayoutDirectionChange || event->type() == QEvent::FontChange
           || event->type() == QEvent::StyleChange)) {
        p->m_view->invalidate();
        p->doDelayedItemsLayout();
    }
}

bool ExpandedTreeView::viewportEvent(QEvent* event)
{
    switch(event->type()) {
        case QEvent::HoverEnter:
        case QEvent::HoverMove: {
            if(const auto* hoverEvent = static_cast<QHoverEvent*>(event)) {
                p->setHoverIndex(indexAt(hoverEvent->position().toPoint()));
            }
            break;
        }
        case QEvent::HoverLeave:
        case QEvent::Leave:
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

    bool changedAction{false};

    const auto acceptAction = [&changedAction, event]() {
        if(changedAction) {
            event->accept();
        }
        else {
            event->acceptProposedAction();
        }
    };

    if(event->source() != this || event->modifiers() & Qt::ControlModifier) {
        changedAction = true;
        event->setDropAction(Qt::CopyAction);
    }

    QModelIndex index = p->indexAtDropPosition(pos);
    p->m_hoverIndex   = index;

    if(!index.isValid() && p->itemCount() > 0) {
        index = p->m_viewItems.back().index;
        index = index.siblingAtColumn(Utils::firstVisualIndex(p->m_header));
    }

    if(index.isValid() && showDropIndicator()) {
        const QRect rect      = p->m_view->visualRect(index, RectRule::FullRow, false);
        p->m_dropIndicatorPos = dropPosition(pos, rect, index);

        switch(p->m_dropIndicatorPos) {
            case AboveItem: {
                if(p->isIndexDropEnabled(index.parent())) {
                    p->m_dropIndicatorRect = p->m_viewMode != ViewMode::Tree
                                               ? QRect{rect.left(), rect.top(), 0, rect.height()}
                                               : QRect{rect.left(), rect.top(), rect.width(), 0};
                    acceptAction();
                }
                else {
                    p->m_dropIndicatorRect = {};
                }
                break;
            }
            case BelowItem: {
                if(p->isIndexDropEnabled(index.parent())) {
                    p->m_dropIndicatorRect = p->m_viewMode != ViewMode::Tree
                                               ? QRect{rect.right(), rect.top(), 0, rect.height()}
                                               : QRect{rect.left(), rect.bottom(), rect.width(), 0};
                    acceptAction();
                }
                else {
                    p->m_dropIndicatorRect = {};
                }
                break;
            }
            case OnItem: {
                p->m_dropIndicatorRect = {};
                event->ignore();
                break;
            }
            case OnViewport: {
                p->m_dropIndicatorRect = {};

                if(p->isIndexDropEnabled({})) {
                    acceptAction();
                }
                break;
            }
        }
    }
    else {
        p->m_dropIndicatorRect = {};
        p->m_dropIndicatorPos  = OnViewport;

        if(p->isIndexDropEnabled({})) {
            acceptAction();
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
    p->stopAutoScroll();
    p->m_hoverIndex        = QModelIndex{};
    p->m_dropIndicatorRect = {};
    p->m_dropIndicatorPos  = OnViewport;
    viewport()->update();
}

void ExpandedTreeView::dropEvent(QDropEvent* event)
{
    if(dragDropMode() == InternalMove && (event->source() != this || !(event->possibleActions() & Qt::MoveAction))) {
        return;
    }

    int col{-1};
    int row{-1};
    QModelIndex index = p->indexAtDropPosition(event->position().toPoint());

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

void ExpandedTreeView::mousePressEvent(QMouseEvent* event)
{
    const auto modifierGuard = ContextMenuUtils::normaliseContextMenuModifiers(event);

    if(event->button() > Qt::MiddleButton) {
        event->ignore();
        return;
    }

    const QPoint pos            = event->position().toPoint();
    const QModelIndex index     = indexAt(pos);
    const QModelIndex prevIndex = currentIndex().siblingAtColumn(0);

    if(!index.isValid()) {
        QAbstractItemView::mousePressEvent(event);
        clearSelection();
        return;
    }

    auto* selectModel            = selectionModel();
    const QModelIndex modelIndex = index.siblingAtColumn(0);

    p->m_pressedPos   = pos + p->offset();
    p->m_canStartDrag = true;

    if(event->button() == Qt::MiddleButton) {
        QMetaObject::invokeMethod(this, [this, index]() { Q_EMIT middleClicked(index); }, Qt::QueuedConnection);
    }

    if(p->m_selectBeforeDrag && !model()->hasChildren(modelIndex)) {
        p->m_canStartDrag = p->isRowSelected(modelIndex);
        QAbstractItemView::mousePressEvent(event);
        return;
    }

    const auto command = selectionCommand(index, event);

    QAbstractItemView::mousePressEvent(event);

    if(p->m_model->hasChildren(modelIndex)) {
        if(event->modifiers().testFlag(Qt::ShiftModifier) && prevIndex.isValid()) {
            p->selectIncludingChildren(prevIndex, modelIndex, command);
            return;
        }

        const QItemSelection selection = selectRecursively(p->m_model, {modelIndex, modelIndex});
        selectModel->select(selection, command);
    }
}

void ExpandedTreeView::mouseMoveEvent(QMouseEvent* event)
{
    if(p->m_canStartDrag) {
        QAbstractItemView::mouseMoveEvent(event);
        return;
    }

    const bool enabled     = dragEnabled();
    const auto restoreDrag = scopeGuard([this, enabled]() { setDragEnabled(enabled); });
    setDragEnabled(false);
    QAbstractItemView::mouseMoveEvent(event);
}

void ExpandedTreeView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if(event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    QAbstractItemView::mouseDoubleClickEvent(event);
}

void ExpandedTreeView::wheelEvent(QWheelEvent* event)
{
    if(!(event->modifiers() & Qt::ControlModifier)) {
        QAbstractItemView::wheelEvent(event);
        return;
    }

    const int delta     = event->angleDelta().y();
    const int increment = (delta > 0) ? 1 : -1;
    int newSize         = iconSize().width() + (increment * 2);
    newSize             = std::clamp(newSize, 16, 1024);
    changeIconSize({newSize, newSize});

    event->accept();
}

void ExpandedTreeView::resizeEvent(QResizeEvent* event)
{
    const QSize delta = event->size() - event->oldSize();
    if(delta.isNull()) {
        return;
    }

    if(p->m_expansionAnimationHeight >= 0 && size() != p->m_expansionAnimationViewSize) {
        p->stopExpansionAnimation();
        refreshExpansion();
    }

    if(p->m_viewMode == ViewMode::Icon && (delta.width() != 0 || p->m_expansionWidget)) {
        p->m_preserveLayoutPosition = true;
    }
    if(p->m_delayedPendingLayout) {
        return;
    }

    if(p->m_viewMode == ViewMode::Flow && delta.width() != 0) {
        p->doDelayedItemsLayout();
    }
    else if(p->m_viewMode == ViewMode::Icon && (delta.width() != 0 || p->m_expansionWidget)) {
        p->doDelayedItemsLayout(100);
    }
    else {
        QAbstractItemView::resizeEvent(event);
    }
}

void ExpandedTreeView::paintEvent(QPaintEvent* event)
{
    p->layoutItems();

    QStylePainter painter{viewport()};

    p->m_view->drawView(&painter, event->region());

    if(p->m_expansionAnimationHeight >= 0 && p->m_expansionRect.isValid()) {
        const QRect rect = p->m_expansionRect.translated(0, -verticalScrollBar()->value());
        painter.save();
        painter.setClipRect(rect);
        painter.drawPixmap(rect.topLeft(), p->animationSnapshot());
        painter.restore();
    }

    if(!p->m_viewItems.empty() && state() == DraggingState) {
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
        doItemsLayout();
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
    p->updateExpansionGeometry();

    if(dx) {
        const int oldOffset = p->m_header->offset();
        p->m_header->setOffset(horizontalScrollBar()->value());
        if(horizontalScrollMode() == ScrollPerItem) {
            const int newOffset = p->m_header->offset();
            dx                  = isRightToLeft() ? newOffset - oldOffset : oldOffset - newOffset;
        }
    }

    const int itemHeight = p->m_defaultItemHeight <= 0 ? sizeHintForRow(0) : p->m_defaultItemHeight;
    if(p->m_viewItems.empty() || itemHeight == 0) {
        updateEditorGeometries();
        return;
    }

    // Guess the number of items in the viewport
    const int viewCount = viewport()->height() / itemHeight;
    const int maxDeltaY = std::min(p->itemCount(), viewCount);

    if(std::abs(dy) > std::abs(maxDeltaY)) {
        verticalScrollBar()->update();
        viewport()->update();
        updateEditorGeometries();
        return;
    }

    if(dy && p->m_viewMode == ViewMode::Tree && verticalScrollMode() == ScrollPerItem) {
        const int currentScrollbarValue  = verticalScrollBar()->value();
        const int previousScrollbarValue = currentScrollbarValue + dy;
        const int currentViewIndex       = currentScrollbarValue; // First visible item
        const int previousViewIndex      = previousScrollbarValue;
        dy                               = 0;

        if(previousViewIndex < currentViewIndex) { // Scrolling down
            for(int i = previousViewIndex; i < currentViewIndex; ++i) {
                if(i < itemCount()) {
                    dy -= p->m_view->itemHeight(i);
                }
            }
        }
        else if(previousViewIndex > currentViewIndex) { // Scrolling up
            for(int i{previousViewIndex - 1}; i >= currentViewIndex; --i) {
                if(i < itemCount()) {
                    dy += p->m_view->itemHeight(i);
                }
            }
        }
    }

    p->m_scrollDelayOffset = {-dx, -dy};
    viewport()->update();
    p->m_scrollDelayOffset = {0, 0};
    updateEditorGeometries();
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
    p->clearViewItems();
    QAbstractItemView::rowsAboutToBeRemoved(parent, start, end);
}

void ExpandedTreeView::rowsRemoved(const QModelIndex& /*parent*/, int /*first*/, int /*last*/)
{
    p->clearViewItems();
    p->doDelayedItemsLayout();

    setState(NoState);
    updateGeometry();
}

void ExpandedTreeView::startDrag(Qt::DropActions supportedActions)
{
    if(!dragEnabled() || !p->m_canStartDrag) {
        return;
    }

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
    {
        QPainter painter{&pixmap};
        p->m_view->renderToPixmap(&painter, paintPairs, rect);
    }

    rect.adjust(horizontalOffset(), verticalOffset(), 0, 0);

    auto* drag = new QDrag(this);
    drag->setPixmap(pixmap);
    drag->setMimeData(mimeData);
    drag->setHotSpot(p->m_pressedPos - rect.topLeft());

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

    if(state() == DraggingState) {
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
    if(p->m_viewMode == ViewMode::Icon) {
        if(pos.y() < rect.top()) {
            return AboveItem;
        }
        if(pos.y() > rect.bottom()) {
            return BelowItem;
        }
        return pos.x() < rect.center().x() ? AboveItem : BelowItem;
    }

    DropIndicatorPosition dropPos{OnViewport};
    const int margin = std::clamp(static_cast<int>(std::round(static_cast<double>(rect.height()) / 5.5)), 2, 12);

    if(pos.y() - rect.top() < margin) {
        dropPos = AboveItem;
    }
    else if(rect.bottom() - pos.y() < margin) {
        dropPos = BelowItem;
    }
    else if(rect.contains(pos, true)) {
        dropPos = OnItem;
    }

    if(dropPos == OnItem && (!(p->m_model->flags(index) & Qt::ItemIsDropEnabled))) {
        dropPos = pos.y() < rect.center().y() ? AboveItem : BelowItem;
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

    const auto paintRects = p->rectsToPaint(index, option, y);

    for(const auto& rect : paintRects) {
        if(rect.width() > 0) {
            focusOpt.rect = QStyle::visualRect(layoutDirection(), viewport()->rect(), rect);
            style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focusOpt, painter);
        }
    }
}

QRect ExpandedTreeView::drawSpan(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    itemDelegateForIndex(index)->paint(painter, option, index);
    return option.rect;
}

int ExpandedTreeView::spanHeight(const QModelIndex& index) const
{
    return header()->sectionSize(index.column());
}

int ExpandedTreeView::adjustVerticalScrollMaximum(int maximum) const
{
    return maximum;
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
        case MoveNext:
        case MoveDown:
            return p->modelIndex(p->m_view->itemBelow(viewIndex), current.column());
        case MovePrevious:
        case MoveUp:
            return p->modelIndex(p->m_view->itemAbove(viewIndex), current.column());
        case MovePageUp:
            return p->modelIndex(p->m_view->pageUp(viewIndex), current.column());
        case MovePageDown:
            return p->modelIndex(p->m_view->pageDown(viewIndex), current.column());
        case MoveHome:
            return p->modelIndex(p->itemForHomeKey(), current.column());
        case MoveEnd:
            return p->modelIndex(p->itemForEndKey(), current.column());
        case MoveLeft:
            return p->modelIndex(p->m_view->itemLeft(viewIndex), current.column());
        case MoveRight:
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
    if(p->m_viewMode == ViewMode::Tree && verticalScrollMode() == ScrollPerItem) {
        if(p->m_uniformRowHeights) {
            return verticalScrollBar()->value() * p->m_view->m_uniformRowHeight;
        }

        p->layoutItems();

        int offset{0};
        const int count = std::min(itemCount(), verticalScrollBar()->value());
        for(int i{0}; i < count; ++i) {
            offset += p->m_view->itemHeight(i);
        }
        return offset;
    }

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

    // Icon ranges can wrap onto another row, so keep the original points
    const bool iconMode = (p->m_viewMode != ViewMode::Tree);
    const QPoint tl
        = iconMode ? rect.topLeft() : QPoint{std::min(rect.left(), rect.right()), std::min(rect.top(), rect.bottom())};
    const QPoint br = iconMode ? rect.bottomRight()
                               : QPoint{std::max(rect.left(), rect.right()), std::max(rect.top(), rect.bottom())};

    QModelIndex topLeft     = findIndexAt(tl, false);
    QModelIndex bottomRight = findIndexAt(br, false);

    if(iconMode && p->m_expansionRect.isValid()) {
        if(p->m_expansionRect.contains(tl + p->offset())) {
            topLeft = p->modelIndex(p->m_view->itemAtCoordinate(tl, true));
        }
        if(p->m_expansionRect.contains(br + p->offset())) {
            bottomRight = p->modelIndex(p->m_view->itemAtCoordinate(br, true));
        }
    }

    if(!topLeft.isValid() && !bottomRight.isValid()) {
        if(command & QItemSelectionModel::Clear) {
            selectionModel()->clear();
        }
        return;
    }
    if(!topLeft.isValid()) {
        topLeft = p->m_view->nearestSelectionIndex(tl);
    }
    if(!bottomRight.isValid()) {
        bottomRight = p->m_view->nearestSelectionIndex(br);
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

    if(iconMode && p->viewIndex(topLeft) > p->viewIndex(bottomRight)) {
        std::swap(topLeft, bottomRight);
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

    if(p->m_viewMode != ViewMode::Tree) {
        const auto indexes = selection.indexes();
        for(const auto& index : indexes) {
            const auto rect = visualRect(index);
            if(viewportRect.intersects(rect)) {
                selectionRegion += rect;
            }
        }
        return selectionRegion;
    }

    p->layoutItems();

    int firstOffset{0};
    const int firstVisible = p->m_view->firstVisibleItem(&firstOffset);
    if(firstVisible < 0) {
        return selectionRegion;
    }

    const int lastVisible = p->m_view->lastVisibleItem(firstVisible, firstOffset);
    if(lastVisible < firstVisible) {
        return selectionRegion;
    }

    int y{firstOffset};
    for(int item{firstVisible}; item <= lastVisible && item < p->itemCount(); ++item) {
        const auto& viewItem     = p->m_viewItems.at(item);
        const QModelIndex& index = viewItem.index;
        const int height         = p->m_view->itemHeight(item) + viewItem.padding;

        if(!index.isValid()) {
            y += height;
            continue;
        }

        const QModelIndex parent = index.parent();
        const int row            = index.row();

        for(const auto& range : selection) {
            if(!range.isValid() || range.parent() != parent || row < range.top() || row > range.bottom()) {
                continue;
            }

            if(p->m_header->sectionsMoved()) {
                for(int column{range.left()}; column <= range.right(); ++column) {
                    if(isIndexHidden(p->m_model->index(row, column, parent))) {
                        continue;
                    }

                    const QRect rangeRect{p->m_header->sectionViewportPosition(column), y,
                                          p->m_header->sectionSize(column), height};
                    if(viewportRect.intersects(rangeRect)) {
                        selectionRegion += rangeRect;
                    }
                }
            }
            else {
                int leftColumn{-1};
                for(int column{range.left()}; column <= range.right(); ++column) {
                    if(!isIndexHidden(p->m_model->index(row, column, parent))) {
                        leftColumn = column;
                        break;
                    }
                }

                if(leftColumn < 0) {
                    continue;
                }

                int rightColumn{-1};
                for(int column{range.right()}; column >= range.left(); --column) {
                    if(!isIndexHidden(p->m_model->index(row, column, parent))) {
                        rightColumn = column;
                        break;
                    }
                }

                const int left = p->m_header->sectionViewportPosition(leftColumn);
                const int right
                    = p->m_header->sectionViewportPosition(rightColumn) + p->m_header->sectionSize(rightColumn);
                const QRect rangeRect{std::min(left, right), y, std::abs(right - left), height};
                if(viewportRect.intersects(rangeRect)) {
                    selectionRegion += rangeRect;
                }
            }
        }

        y += height;
    }

    return selectionRegion;
}

QModelIndexList ExpandedTreeView::selectedIndexes() const
{
    QModelIndexList viewSelected;
    QModelIndexList modelSelected;

    if(selectionModel()) {
        modelSelected = selectionModel()->selectedIndexes();
    }

    const auto count = static_cast<int>(modelSelected.size());
    for(int i{0}; i < count; ++i) {
        QModelIndex index = modelSelected.at(i);
        while(index.isValid() && !isIndexHidden(index)) {
            index = index.parent();
        }
        if(index.isValid()) {
            continue;
        }
        viewSelected.append(modelSelected.at(i));
    }
    return viewSelected;
}
} // namespace Fooyin

#include "gui/widgets/moc_expandedtreeview.cpp"
