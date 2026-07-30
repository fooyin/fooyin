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

#include "fysplitter.h"
#include "splitterstate.h"

#include <QEvent>
#include <QLayout>
#include <QLoggingCategory>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QScopedValueRollback>
#include <QStyle>
#include <QStyleOption>
#include <QWidgetItem>

#if QT_CONFIG(rubberband)
#include <QRubberBand>
#include <algorithm>
#endif

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(FY_SPLITTER, "fy.splitter")

namespace Fooyin {
namespace {
enum class Collapsibility : quint8
{
    NotCollapsible,
    Collapsible,
    UseDefault
};

enum class MoveDirection : quint8
{
    Forward,
    Backward
};

enum class NegativeSizeMode : quint8
{
    Preserve,
    ClampToZero
};

int& userResizeDepth()
{
    thread_local int depth{0};
    return depth;
}

bool isUserResizeActive()
{
    return userResizeDepth() > 0;
}

struct LayoutStruct
{
    // Parameters
    int stretch{0};
    int sizeHint{0};
    int maximumSize{QLAYOUTSIZE_MAX};
    int minimumSize{0};
    int spacing{0};
    bool expansive{false};
    bool empty{false};

    // Temp storage
    bool done{false};

    // Result
    int pos{0};
    int size{0};

    void init(int stretchFactor = 0, int minSize = 0)
    {
        stretch     = stretchFactor;
        sizeHint    = minSize;
        minimumSize = minSize;
        maximumSize = QLAYOUTSIZE_MAX;
        spacing     = 0;
        expansive   = false;
        empty       = true;
    }

    int smartSizeHint()
    {
        return (stretch > 0) ? minimumSize : sizeHint;
    }

    [[nodiscard]] int effectiveSpacer(int uniformSpacer) const
    {
        return (uniformSpacer >= 0) ? uniformSpacer : spacing;
    }
};

constexpr qint64 FixedPointScale{256};

constexpr qint64 toFixedPoint(int value)
{
    return static_cast<qint64>(value) * FixedPointScale;
}

constexpr int roundFixedPoint(qint64 value)
{
    return value % FixedPointScale < FixedPointScale / 2 ? static_cast<int>(value / FixedPointScale)
                                                         : static_cast<int>((value / FixedPointScale) + 1);
}

void calculateGeometry(QList<LayoutStruct>& items, int startIndex, int itemCount, int position, int availableSpace,
                       int uniformSpacing)
{
    const int endIndex = startIndex + itemCount;

    int totalSmartSizeHint{0};
    int totalMinimumSize{0};
    int totalStretch{0};
    int totalSpacing{0};
    int expansiveItemCount{0};

    bool allEmptyNonStretching{true};
    int pendingSpacing{-1};
    int spacingCount{0};

    int index{startIndex};
    for(; index < endIndex; index++) {
        LayoutStruct* item = &items[index];
        item->done         = false;

        totalSmartSizeHint += item->smartSizeHint();
        totalMinimumSize += item->minimumSize;
        totalStretch += item->stretch;

        if(!item->empty) {
            // Using pendingSpacing, we ensure that the spacing for the last
            // (non-empty) item is ignored
            if(pendingSpacing >= 0) {
                totalSpacing += pendingSpacing;
                ++spacingCount;
            }
            pendingSpacing = item->effectiveSpacer(uniformSpacing);
        }

        if(item->expansive) {
            expansiveItemCount++;
        }

        allEmptyNonStretching = allEmptyNonStretching && item->empty && !item->expansive && item->stretch <= 0;
    }

    int extraSpace{0};

    if(availableSpace < totalMinimumSize + totalSpacing) {
        // Less space than minimumSize; take from the biggest first
        const int requiredMinimumSpace = totalMinimumSize + totalSpacing;

        // Shrink the spacers proportionally
        if(uniformSpacing >= 0) {
            uniformSpacing = requiredMinimumSpace > 0 ? uniformSpacing * availableSpace / requiredMinimumSpace : 0;
            totalSpacing   = uniformSpacing * spacingCount;
        }

        QVarLengthArray<int, 32> minSizes;
        minSizes.reserve(itemCount);

        for(index = startIndex; index < endIndex; index++) {
            minSizes << items.at(index).minimumSize;
        }

        std::ranges::sort(minSizes);

        int accumulatedMinSize{0};
        int cutoffIndex{0};
        int proposedSpaceUsed{0};
        int currentMinSize{0};

        const int distributableSpace = availableSpace - totalSpacing;
        while(cutoffIndex < itemCount && proposedSpaceUsed < distributableSpace) {
            currentMinSize    = minSizes.at(cutoffIndex);
            proposedSpaceUsed = accumulatedMinSize + (currentMinSize * (itemCount - cutoffIndex));
            accumulatedMinSize += currentMinSize;
            ++cutoffIndex;
        }
        --cutoffIndex;

        const int excessSize     = proposedSpaceUsed - distributableSpace;
        const int truncatedItems = itemCount - cutoffIndex;
        // Truncating every remaining item to currentMinimumSize would use excessSize too many pixels. Remove an equal
        // share from each truncated item and carry the integer-division remainder between items.
        const int reductionPerItem = excessSize / truncatedItems;
        const int remainder        = excessSize % truncatedItems;
        const int maxItemSize      = currentMinSize - reductionPerItem;

        int accumulatedRemainder{0};
        for(index = startIndex; index < endIndex; index++) {
            int adjustedMaxSize = maxItemSize;
            accumulatedRemainder += remainder;
            if(accumulatedRemainder >= truncatedItems) {
                adjustedMaxSize--;
                accumulatedRemainder -= truncatedItems;
            }

            LayoutStruct* item = &items[index];
            item->size         = qMin(item->minimumSize, adjustedMaxSize);
            item->done         = true;
        }
    }
    else if(availableSpace < totalSmartSizeHint + totalSpacing) {
        // Less space than smartSizeHint(), but more than minimumSize. Currently take space equally from each, as in
        // Qt 2.x. Commented-out lines will give more space to stretchier items.
        int remainingItemCount{itemCount};
        int remainingSpace    = availableSpace - totalSpacing;
        int requiredReduction = totalSmartSizeHint - remainingSpace;

        // First give to the fixed ones
        for(index = startIndex; index < endIndex; index++) {
            LayoutStruct* item = &items[index];
            if(!item->done && item->minimumSize >= item->smartSizeHint()) {
                item->size = item->smartSizeHint();
                item->done = true;
                remainingSpace -= item->smartSizeHint();
                remainingItemCount--;
            }
        }

        bool distributionComplete = (remainingItemCount == 0);
        while(!distributionComplete) {
            distributionComplete = true;

            const qint64 fixedPointReduction = toFixedPoint(requiredReduction);
            qint64 fixedPointAllocation{0};

            for(index = startIndex; index < endIndex; index++) {
                LayoutStruct* item = &items[index];
                if(item->done) {
                    continue;
                }

                fixedPointAllocation += fixedPointReduction / remainingItemCount;

                const int sizeReduction = roundFixedPoint(fixedPointAllocation);
                item->size              = item->smartSizeHint() - sizeReduction;
                fixedPointAllocation -= toFixedPoint(sizeReduction); // Give the difference to the next
                if(item->size < item->minimumSize) {
                    item->done           = true;
                    item->size           = item->minimumSize;
                    distributionComplete = false;
                    requiredReduction -= item->smartSizeHint() - item->minimumSize;
                    remainingItemCount--;
                    break;
                }
            }
        }
    }
    else {
        // Extra space
        int remainingItemCount{itemCount};
        int remainingSpace = availableSpace - totalSpacing;
        // First give to the fixed ones, and handle non-expansiveness
        for(index = startIndex; index < endIndex; index++) {
            LayoutStruct* item = &items[index];
            if(!item->done
               && (item->maximumSize <= item->smartSizeHint()
                   || (!allEmptyNonStretching && item->empty && !item->expansive && item->stretch == 0))) {
                item->size = item->smartSizeHint();
                item->done = true;
                remainingSpace -= item->size;
                totalStretch -= item->stretch;
                if(item->expansive) {
                    expansiveItemCount--;
                }
                remainingItemCount--;
            }
        }
        extraSpace = remainingSpace;

        /*
          Do a trial distribution and calculate how much it is off.
          If there are more deficit pixels than surplus pixels, give
          the minimum size items what they need, and repeat.
          Otherwise give to the maximum size items, and repeat.
         */
        int totalSurplus{0};
        int totalDeficit{0};

        while(remainingItemCount > 0) {
            totalSurplus = 0;
            totalDeficit = 0;

            const qint64 fixedPointSpace = toFixedPoint(remainingSpace);
            qint64 fixedPointAllocation{0};

            for(index = startIndex; index < endIndex; index++) {
                LayoutStruct* item = &items[index];
                if(item->done) {
                    continue;
                }

                extraSpace = 0;

                if(totalStretch > 0) {
                    fixedPointAllocation += (fixedPointSpace * item->stretch) / totalStretch;
                }
                else if(expansiveItemCount > 0) {
                    fixedPointAllocation += (fixedPointSpace * (item->expansive ? 1 : 0)) / expansiveItemCount;
                }
                else {
                    fixedPointAllocation += fixedPointSpace / remainingItemCount;
                }

                const int allocatedSize = roundFixedPoint(fixedPointAllocation);
                item->size              = allocatedSize;
                fixedPointAllocation -= toFixedPoint(allocatedSize); // Give the difference to the next

                if(allocatedSize < item->smartSizeHint()) {
                    totalDeficit += item->smartSizeHint() - allocatedSize;
                }
                else if(allocatedSize > item->maximumSize) {
                    totalSurplus += allocatedSize - item->maximumSize;
                }
            }
            if(totalDeficit > 0 && totalSurplus <= totalDeficit) {
                // Give to the ones that have too little
                for(index = startIndex; index < endIndex; index++) {
                    LayoutStruct* item = &items[index];
                    if(!item->done && item->size < item->smartSizeHint()) {
                        item->size = item->smartSizeHint();
                        item->done = true;
                        remainingSpace -= item->smartSizeHint();
                        totalStretch -= item->stretch;
                        if(item->expansive) {
                            expansiveItemCount--;
                        }
                        remainingItemCount--;
                    }
                }
            }
            if(totalSurplus > 0 && totalSurplus >= totalDeficit) {
                // Take from the ones that have too much
                for(index = startIndex; index < endIndex; index++) {
                    LayoutStruct* item = &items[index];
                    if(!item->done && item->size > item->maximumSize) {
                        item->size = item->maximumSize;
                        item->done = true;
                        remainingSpace -= item->maximumSize;
                        totalStretch -= item->stretch;

                        if(item->expansive) {
                            expansiveItemCount--;
                        }
                        remainingItemCount--;
                    }
                }
            }

            if(totalSurplus == totalDeficit) {
                break;
            }
        }

        if(remainingItemCount == 0) {
            extraSpace = remainingSpace;
        }
    }

    /*
      As a last resort, we distribute the unwanted space equally
      among the spacers (counting the start and end of the chain). We
      could, but don't, attempt a sub-pixel allocation of the extra
      space.
    */
    const int extraSpacePerGap = extraSpace / (spacingCount + 2);
    int currentPosition        = position + extraSpacePerGap;

    for(index = startIndex; index < endIndex; index++) {
        LayoutStruct* item = &items[index];
        item->pos          = currentPosition;
        currentPosition += item->size;

        if(!item->empty) {
            currentPosition += item->effectiveSpacer(uniformSpacing) + extraSpacePerGap;
        }
    }
}

QSize smartMinSize(const QSize& sizeHint, const QSize& minSizeHint, const QSize& minSize, const QSize& maxSize,
                   const QSizePolicy& sizePolicy)
{
    QSize size{0, 0};

    if(sizePolicy.horizontalPolicy() != QSizePolicy::Ignored) {
        if(sizePolicy.horizontalPolicy() & QSizePolicy::ShrinkFlag) {
            size.setWidth(minSizeHint.width());
        }
        else {
            size.setWidth(qMax(sizeHint.width(), minSizeHint.width()));
        }
    }

    if(sizePolicy.verticalPolicy() != QSizePolicy::Ignored) {
        if(sizePolicy.verticalPolicy() & QSizePolicy::ShrinkFlag) {
            size.setHeight(minSizeHint.height());
        }
        else {
            size.setHeight(qMax(sizeHint.height(), minSizeHint.height()));
        }
    }

    size = size.boundedTo(maxSize);

    if(minSize.width() > 0) {
        size.setWidth(minSize.width());
    }
    if(minSize.height() > 0) {
        size.setHeight(minSize.height());
    }

    return size.expandedTo({0, 0});
}

QSize smartMinSize(const QWidget* w)
{
    return smartMinSize(w->sizeHint(), w->minimumSizeHint(), w->minimumSize(), w->maximumSize(), w->sizePolicy());
}

QSize smartMaxSize(const QSize& sizeHint, const QSize& minSize, const QSize& maxSize, const QSizePolicy& sizePolicy,
                   Qt::Alignment align = {})
{
    if(align & Qt::AlignHorizontal_Mask && align & Qt::AlignVertical_Mask) {
        return {QLAYOUTSIZE_MAX, QLAYOUTSIZE_MAX};
    }

    QSize size{maxSize};
    const QSize hint = sizeHint.expandedTo(minSize);

    if(size.width() == QWIDGETSIZE_MAX && !(align & Qt::AlignHorizontal_Mask)) {
        if(!(sizePolicy.horizontalPolicy() & QSizePolicy::GrowFlag)) {
            size.setWidth(hint.width());
        }
    }

    if(size.height() == QWIDGETSIZE_MAX && !(align & Qt::AlignVertical_Mask)) {
        if(!(sizePolicy.verticalPolicy() & QSizePolicy::GrowFlag)) {
            size.setHeight(hint.height());
        }
    }

    if(align & Qt::AlignHorizontal_Mask) {
        size.setWidth(QLAYOUTSIZE_MAX);
    }
    if(align & Qt::AlignVertical_Mask) {
        size.setHeight(QLAYOUTSIZE_MAX);
    }

    return size;
}

QSize smartMaxSize(const QWidget* w, Qt::Alignment align = {})
{
    return smartMaxSize(w->sizeHint().expandedTo(w->minimumSizeHint()), w->minimumSize(), w->maximumSize(),
                        w->sizePolicy(), align);
}
} // namespace

class SplitterLayoutStruct
{
public:
    QRect rect;
    int storedSize{-1};
    bool collapsed{false};
    Collapsibility collapsibility{Collapsibility::UseDefault};
    bool locked{false};
    QWidget* widget{nullptr};
    std::unique_ptr<FySplitterHandle> handle;

    int getWidgetSize(Qt::Orientation orientation)
    {
        if(storedSize == -1) {
            const QSize widgetSizeHint = widget->sizeHint();
            const int sizeHintLength   = axisLength(widgetSizeHint, orientation);
            const int currentLength    = axisLength(widget->size(), orientation);

            if(!widgetSizeHint.isValid() || (widget->testAttribute(Qt::WA_Resized) && currentLength > sizeHintLength)) {
                storedSize = currentLength;
            }
            else {
                storedSize = sizeHintLength;
            }

            const QSizePolicy sizePolicy = widget->sizePolicy();
            const int stretchFactor
                = (orientation == Qt::Horizontal) ? sizePolicy.horizontalStretch() : sizePolicy.verticalStretch();
            if(stretchFactor > 1) {
                storedSize *= stretchFactor;
            }
        }

        return storedSize;
    }

    [[nodiscard]] int handleSize(Qt::Orientation orientation) const
    {
        return axisLength(handle->sizeHint(), orientation);
    }

    static int axisLength(const QSize& size, Qt::Orientation orientation)
    {
        return (orientation == Qt::Horizontal) ? size.width() : size.height();
    }
};

class FySplitterPrivate
{
public:
    FySplitterPrivate(FySplitter* self, Qt::Orientation orientation);

    void init();
    void recalculate(bool updateImmediately = false);
    void doResize();
    void storeSizes();
    void getRange(int index, int* farMin, int* min, int* max, int* farMax) const;
    void addContribution(int index, int* min, int* max, bool mayCollapse) const;
    int adjustPos(int pos, int index, int* farMin, int* min, int* max, int* farMax) const;
    bool collapsible(SplitterLayoutStruct* s) const;
    bool collapsible(int index) const;
    void setItemGeometry(SplitterLayoutStruct* item, int position, int length, bool updateCollapsedState);
    void calculateMove(MoveDirection direction, int handlePosition, int itemIndex, bool allowCollapse,
                       int* itemPositions, int* itemLengths);
    SplitterLayoutStruct* findWidget(QWidget* widget) const;
    void insertWidget(int requestedIndex, QWidget* widget, bool showWidget);
    void insertWidgetItem(int requestedIndex, QWidget* widget);
    void updateHandles();
    int findNearestVisibleItem(int handleIndex, MoveDirection direction, int& collapsibleLength) const;
    bool shouldShowWidget(const QWidget* widget) const;
    void setSizes(const QList<int>& requestedSizes, NegativeSizeMode negativeSizeMode);

    int size() const
    {
        return static_cast<int>(m_list.size());
    }

    int axisPosition(const QPoint& position) const
    {
        return m_orientation == Qt::Horizontal ? position.x() : position.y();
    }

    int axisLength(const QSize& size) const
    {
        return m_orientation == Qt::Horizontal ? size.width() : size.height();
    }

    int crossAxisLength(const QSize& size) const
    {
        return m_orientation == Qt::Vertical ? size.width() : size.height();
    }

    FySplitter* m_self;
    Qt::Orientation m_orientation;

#if QT_CONFIG(rubberband)
    QPointer<QRubberBand> m_rubberBand;
#endif

    mutable QList<SplitterLayoutStruct*> m_list;
    bool m_opaque{true};
    bool m_firstShow{true};
    bool m_childrenCollapsible{true};
    bool m_compatMode{false};
    int m_handleWidth{-1};
    bool m_blockChildAdd{false};
    bool m_opaqueResizeSet{false};
    bool m_rebaseSizesOnNextResize{false};
};

FySplitterPrivate::FySplitterPrivate(FySplitter* self, Qt::Orientation orientation)
    : m_self{self}
    , m_orientation{orientation}
{ }

void FySplitterPrivate::init()
{
    QSizePolicy policy{QSizePolicy::Expanding, QSizePolicy::Preferred};
    if(m_orientation == Qt::Vertical) {
        policy.transpose();
    }
    m_self->setSizePolicy(policy);
    m_self->setAttribute(Qt::WA_WState_OwnSizePolicy, false);
}

void FySplitterPrivate::recalculate(bool updateImmediately)
{
    const int itemCount = size();

    // Splitter handles before the first visible widget or right
    // before a hidden widget must be hidden
    bool beforeFirstVisibleWidget{true};
    bool hasExpandedVisibleWidget{false};

    for(int index{0}; index < itemCount; ++index) {
        SplitterLayoutStruct* item = m_list.at(index);
        const bool widgetHidden    = item->widget->isHidden();
        if(!widgetHidden && !item->collapsed) {
            hasExpandedVisibleWidget = true;
        }

        item->handle->setHidden(beforeFirstVisibleWidget || widgetHidden);

        if(!widgetHidden) {
            beforeFirstVisibleWidget = false;
        }
    }

    if(itemCount > 0 && !hasExpandedVisibleWidget) {
        for(int index{0}; index < itemCount; ++index) {
            SplitterLayoutStruct* item = m_list.at(index);
            if(!item->widget->isHidden()) {
                item->collapsed = false;
                break;
            }
        }
    }

    const int totalFrameWidth = 2 * m_self->frameWidth();
    int maxAxisLength         = totalFrameWidth;
    int minAxisLength         = totalFrameWidth;
    int maxCrossAxisLength{QWIDGETSIZE_MAX};
    int minCrossAxisLength{totalFrameWidth};

    // Calculate min/max sizes for the whole splitter
    bool hasVisibleWidget{false};
    for(int index{0}; index < itemCount; index++) {
        SplitterLayoutStruct* item = m_list.at(index);

        if(!item->widget->isHidden()) {
            hasVisibleWidget = true;
            if(!item->handle->isHidden()) {
                minAxisLength += item->handleSize(m_orientation);
                maxAxisLength += item->handleSize(m_orientation);
            }

            const QSize widgetMinSize = smartMinSize(item->widget);
            const QSize widgetMaxSize = smartMaxSize(item->widget);

            minAxisLength += axisLength(widgetMinSize);
            maxAxisLength += axisLength(widgetMaxSize);
            minCrossAxisLength = qMax(minCrossAxisLength, crossAxisLength(widgetMinSize));

            const int widgetMaxCrossAxisLength = crossAxisLength(widgetMaxSize);
            if(widgetMaxCrossAxisLength > 0) {
                maxCrossAxisLength = qMin(maxCrossAxisLength, widgetMaxCrossAxisLength);
            }
        }
    }

    if(!hasVisibleWidget) {
        if(qobject_cast<FySplitter*>(m_self->parent())) {
            // Nested splitters; be nice
            maxAxisLength      = 0;
            maxCrossAxisLength = 0;
        }
        else {
            // Splitter with no children yet
            maxAxisLength = QWIDGETSIZE_MAX;
        }
    }
    else {
        maxAxisLength = qMin(maxAxisLength, QWIDGETSIZE_MAX);
    }

    maxCrossAxisLength = std::max(maxCrossAxisLength, minCrossAxisLength);

    if(updateImmediately) {
        if(m_orientation == Qt::Horizontal) {
            m_self->setMaximumSize(maxAxisLength, maxCrossAxisLength);
            if(m_self->isWindow()) {
                m_self->setMinimumSize(minAxisLength, minCrossAxisLength);
            }
        }
        else {
            m_self->setMaximumSize(maxCrossAxisLength, maxAxisLength);
            if(m_self->isWindow()) {
                m_self->setMinimumSize(minCrossAxisLength, minAxisLength);
            }
        }
        doResize();
        m_self->updateGeometry();
    }
    else {
        m_firstShow = true;
    }
}

void FySplitterPrivate::doResize()
{
    const bool hasVisibleWidget
        = std::ranges::any_of(m_list, [](const SplitterLayoutStruct* item) { return !item->widget->isHidden(); });
    const bool shouldRebaseSizes = m_rebaseSizesOnNextResize && hasVisibleWidget;
    const bool userResizeActive  = isUserResizeActive();
    const bool resizeLockedItems = userResizeActive || shouldRebaseSizes;

    if(shouldRebaseSizes) {
        m_rebaseSizesOnNextResize = false;
    }

    const bool hasStretchFactors = std::ranges::any_of(m_list, [this](const SplitterLayoutStruct* item) {
        const QSizePolicy sizePolicy = item->widget->sizePolicy();
        const int stretchFactor
            = m_orientation == Qt::Horizontal ? sizePolicy.horizontalStretch() : sizePolicy.verticalStretch();
        return stretchFactor != 0;
    });

    const int itemCount{size()};
    QList<LayoutStruct> layoutChain(static_cast<qsizetype>(itemCount) * 2);

    int layoutIndex{0};
    for(int index{0}; index < itemCount; ++index) {
        SplitterLayoutStruct* item = m_list.at(index);

        LayoutStruct& handleLayout = layoutChain[layoutIndex++];
        handleLayout.init();
        if(item->handle->isHidden()) {
            handleLayout.maximumSize = 0;
        }
        else {
            const int handleSize     = item->handleSize(m_orientation);
            handleLayout.sizeHint    = handleSize;
            handleLayout.minimumSize = handleSize;
            handleLayout.maximumSize = handleSize;
            handleLayout.empty       = false;
        }

        LayoutStruct& widgetLayout = layoutChain[layoutIndex++];
        widgetLayout.init();

        if(item->widget->isHidden() || item->collapsed) {
            widgetLayout.maximumSize = 0;
        }
        else {
            widgetLayout.minimumSize = axisLength(smartMinSize(item->widget));
            widgetLayout.maximumSize = axisLength(smartMaxSize(item->widget));
            widgetLayout.empty       = false;

            if(item->locked && !resizeLockedItems) {
                const int lockedSize
                    = qBound(widgetLayout.minimumSize, item->getWidgetSize(m_orientation), widgetLayout.maximumSize);
                widgetLayout.minimumSize = lockedSize;
                widgetLayout.sizeHint    = lockedSize;
                widgetLayout.maximumSize = lockedSize;
            }
            else {
                bool stretchWidget = !hasStretchFactors;
                if(!stretchWidget) {
                    const QSizePolicy sizePolicy = item->widget->sizePolicy();
                    const int stretchFactor      = m_orientation == Qt::Horizontal ? sizePolicy.horizontalStretch()
                                                                                   : sizePolicy.verticalStretch();
                    stretchWidget                = stretchFactor != 0;
                }

                if(stretchWidget) {
                    widgetLayout.stretch   = item->getWidgetSize(m_orientation);
                    widgetLayout.sizeHint  = widgetLayout.minimumSize;
                    widgetLayout.expansive = true;
                }
                else {
                    widgetLayout.sizeHint = qMax(item->getWidgetSize(m_orientation), widgetLayout.minimumSize);
                }
            }
        }
    }

    const QRect rect{m_self->contentsRect()};
    calculateGeometry(layoutChain, 0, itemCount * 2, axisPosition(rect.topLeft()), axisLength(rect.size()), 0);

    for(int index{0}; index < itemCount; ++index) {
        SplitterLayoutStruct* item       = m_list.at(index);
        const LayoutStruct& widgetLayout = layoutChain[(index * 2) + 1];
        setItemGeometry(item, widgetLayout.pos, widgetLayout.size, false);
    }

    if(resizeLockedItems && hasVisibleWidget) {
        storeSizes();
    }
}

void FySplitterPrivate::storeSizes()
{
    for(int i{0}; i < size(); ++i) {
        SplitterLayoutStruct* item = m_list.at(i);
        item->storedSize           = axisLength(item->rect.size());
    }
}

void FySplitterPrivate::getRange(int index, int* farMin, int* min, int* max, int* farMax) const
{
    const int itemCount = size();
    if(index <= 0 || index >= itemCount) {
        return;
    }

    int collapsibleLengthBefore{0};
    const int visibleIndexBefore = findNearestVisibleItem(index, MoveDirection::Backward, collapsibleLengthBefore);

    int collapsibleLengthAfter{0};
    const int visibleIndexAfter = findNearestVisibleItem(index, MoveDirection::Forward, collapsibleLengthAfter);

    int minLengthBefore{0};
    int maxLengthBefore{0};
    for(int itemIndex{0}; itemIndex < index; ++itemIndex) {
        addContribution(itemIndex, &minLengthBefore, &maxLengthBefore, itemIndex == visibleIndexBefore);
    }

    int minLengthAfter{0};
    int maxLengthAfter{0};
    for(int itemIndex{index}; itemIndex < itemCount; ++itemIndex) {
        addContribution(itemIndex, &minLengthAfter, &maxLengthAfter, itemIndex == visibleIndexAfter);
    }

    const QRect contentsRect = m_self->contentsRect();
    const int contentsLength = axisLength(contentsRect.size());
    const int contentsStart  = axisPosition(contentsRect.topLeft());

    const int effectiveMinLengthBefore = qMax(minLengthBefore, contentsLength - maxLengthAfter);
    const int effectiveMaxLengthBefore = qMin(maxLengthBefore, contentsLength - minLengthAfter);
    const int minPosition              = contentsStart + effectiveMinLengthBefore;
    const int maxPosition              = contentsStart + effectiveMaxLengthBefore;

    int farMinPosition{minPosition};
    if(minLengthBefore - collapsibleLengthBefore >= contentsLength - maxLengthAfter) {
        farMinPosition -= collapsibleLengthBefore;
    }

    int farMaxPosition{maxPosition};
    if(contentsLength - (minLengthAfter - collapsibleLengthAfter) <= maxLengthBefore) {
        farMaxPosition += collapsibleLengthAfter;
    }

    if(farMin) {
        *farMin = farMinPosition;
    }
    if(min) {
        *min = minPosition;
    }
    if(max) {
        *max = maxPosition;
    }
    if(farMax) {
        *farMax = farMaxPosition;
    }
}

void FySplitterPrivate::addContribution(int index, int* min, int* max, bool mayCollapse) const
{
    SplitterLayoutStruct* item = m_list.at(index);
    if(!item->widget->isHidden()) {
        if(!item->handle->isHidden()) {
            *min += item->handleSize(m_orientation);
            *max += item->handleSize(m_orientation);
        }
        if(mayCollapse || !item->collapsed) {
            *min += axisLength(smartMinSize(item->widget));
        }
        *max += axisLength(smartMaxSize(item->widget));
    }
}

int FySplitterPrivate::adjustPos(int pos, int index, int* farMin, int* min, int* max, int* farMax) const
{
    static constexpr int Threshold = 40;

    getRange(index, farMin, min, max, farMax);

    if(pos >= *min) {
        if(pos <= *max) {
            return pos;
        }

        const int delta = pos - *max;
        const int width = *farMax - *max;

        if(delta > width / 2 && delta >= qMin(Threshold, width)) {
            return *farMax;
        }

        return *max;
    }

    const int delta = *min - pos;
    const int width = *min - *farMin;

    if(delta > width / 2 && delta >= qMin(Threshold, width)) {
        return *farMin;
    }

    return *min;
}

bool FySplitterPrivate::collapsible(SplitterLayoutStruct* s) const
{
    if(s->collapsibility != Collapsibility::UseDefault) {
        return s->collapsibility == Collapsibility::Collapsible;
    }

    return m_childrenCollapsible;
}

bool FySplitterPrivate::collapsible(int index) const
{
    return (index < 0 || index >= size()) ? true : collapsible(m_list.at(index));
}

void FySplitterPrivate::setItemGeometry(SplitterLayoutStruct* item, int position, int length, bool updateCollapsedState)
{
    const QRect contentsRect = m_self->contentsRect();

    QRect widgetRect;
    if(m_orientation == Qt::Horizontal) {
        widgetRect.setRect(position, contentsRect.y(), length, contentsRect.height());
    }
    else {
        widgetRect.setRect(contentsRect.x(), position, contentsRect.width(), length);
    }
    item->rect = widgetRect;

    if(m_orientation == Qt::Horizontal && m_self->isRightToLeft()) {
        widgetRect.moveRight(contentsRect.width() - widgetRect.left());
    }

    QWidget* widget = item->widget;
    if(updateCollapsedState) {
        const int minWidgetLength = axisLength(smartMinSize(widget));
        item->collapsed           = length <= 0 && minWidgetLength > 0 && !widget->isHidden();
    }

    // Hide the child widget without calling hide(), so the splitter handle remains visible
    if(item->collapsed) {
        widgetRect.moveTopLeft(QPoint(-widgetRect.width() - 1, -widgetRect.height() - 1));
    }

    widget->setGeometry(widgetRect);

    if(!item->handle->isHidden()) {
        FySplitterHandle* handle     = item->handle.get();
        const QSize handleSizeHint   = handle->sizeHint();
        const QMargins handleMargins = handle->contentsMargins();

        int handlePosition{position};
        if(m_orientation == Qt::Horizontal) {
            if(m_self->isRightToLeft()) {
                handlePosition = contentsRect.width() - handlePosition + handleSizeHint.width();
            }
            handle->setGeometry(handlePosition - handleSizeHint.width() - handleMargins.left(), contentsRect.y(),
                                handleSizeHint.width() + handleMargins.left() + handleMargins.right(),
                                contentsRect.height());
        }
        else {
            handle->setGeometry(contentsRect.x(), handlePosition - handleSizeHint.height() - handleMargins.top(),
                                contentsRect.width(),
                                handleSizeHint.height() + handleMargins.top() + handleMargins.bottom());
        }
    }
}

void FySplitterPrivate::calculateMove(MoveDirection direction, int handlePosition, int itemIndex, bool allowCollapse,
                                      int* itemPositions, int* itemLengths)
{
    if(itemIndex < 0 || itemIndex >= size()) {
        return;
    }

    SplitterLayoutStruct* item = m_list.at(itemIndex);
    const bool moveBackward    = direction == MoveDirection::Backward;
    const int nextIndex        = moveBackward ? itemIndex - 1 : itemIndex + 1;

    const QWidget* widget = item->widget;
    if(widget->isHidden()) {
        calculateMove(direction, handlePosition, nextIndex, collapsible(nextIndex), itemPositions, itemLengths);
        return;
    }

    const int handleLength = item->handle->isHidden() ? 0 : item->handleSize(m_orientation);

    int widgetLength = moveBackward ? handlePosition - axisPosition(item->rect.topLeft())
                                    : axisPosition(item->rect.bottomRight()) - handlePosition - handleLength + 1;
    if(widgetLength > 0 || (!item->collapsed && !allowCollapse)) {
        const int maxWidgetLength = axisLength(smartMaxSize(widget));
        const int minWidgetLength = axisLength(smartMinSize(widget));
        widgetLength              = qMin(widgetLength, maxWidgetLength);
        widgetLength              = qMax(widgetLength, minWidgetLength);
    }
    else {
        widgetLength = 0;
    }

    itemPositions[itemIndex] = moveBackward ? handlePosition - widgetLength : handlePosition + handleLength;
    itemLengths[itemIndex]   = widgetLength;

    const int nextHandlePosition
        = moveBackward ? handlePosition - widgetLength - handleLength : handlePosition + handleLength + widgetLength;
    calculateMove(direction, nextHandlePosition, nextIndex, collapsible(nextIndex), itemPositions, itemLengths);
}

SplitterLayoutStruct* FySplitterPrivate::findWidget(QWidget* widget) const
{
    for(int i{0}; i < size(); ++i) {
        if(m_list.at(i)->widget == widget) {
            return m_list.at(i);
        }
    }
    return nullptr;
}

void FySplitterPrivate::insertWidget(int requestedIndex, QWidget* widget, bool showWidget)
{
    const QScopedValueRollback blockChildAdd{m_blockChildAdd, true};

    const bool shouldShow = showWidget && shouldShowWidget(widget);

    if(widget->parentWidget() != m_self) {
        widget->setParent(m_self);
    }
    if(shouldShow) {
        widget->show();
    }

    insertWidgetItem(requestedIndex, widget);
    recalculate(m_self->isVisible());
}

void FySplitterPrivate::insertWidgetItem(int requestedIndex, QWidget* widget)
{
    SplitterLayoutStruct* item{nullptr};
    int currentIndex{-1};
    int maxIndex{size()};

    for(int index{0}; index < size(); ++index) {
        SplitterLayoutStruct* candidate = m_list.at(index);
        if(candidate->widget == widget) {
            item         = candidate;
            currentIndex = index;
            --maxIndex;
            break;
        }
    }

    int insertIndex{requestedIndex};
    if(insertIndex < 0 || insertIndex > maxIndex) {
        insertIndex = maxIndex;
    }

    if(item) {
        m_list.move(currentIndex, insertIndex);
        return;
    }

    item         = new SplitterLayoutStruct;
    item->widget = widget;
    item->handle.reset(m_self->createHandle());
    item->handle->setObjectName("qt_splithandle_"_L1 + widget->objectName());

    widget->lower();
    m_list.insert(insertIndex, item);

    if(m_self->isVisible()) {
        item->handle->show(); // Will trigger sending of post events
    }
}

void FySplitterPrivate::updateHandles()
{
    recalculate(m_self->isVisible());
}

int FySplitterPrivate::findNearestVisibleItem(int handleIndex, MoveDirection direction, int& collapsibleLength) const
{
    const int indexStep = direction == MoveDirection::Backward ? -1 : 1;
    int itemIndex       = direction == MoveDirection::Backward ? handleIndex - 1 : handleIndex;

    while(itemIndex >= 0 && itemIndex < size()) {
        SplitterLayoutStruct* item = m_list.at(itemIndex);
        if(!item->widget->isHidden()) {
            if(collapsible(item)) {
                collapsibleLength = axisLength(smartMinSize(item->widget));
            }
            return itemIndex;
        }

        itemIndex += indexStep;
    }

    return -1;
}

bool FySplitterPrivate::shouldShowWidget(const QWidget* widget) const
{
    const bool explicitlyHidden = widget->isHidden() && widget->testAttribute(Qt::WA_WState_ExplicitShowHide);
    return m_self->isVisible() && !explicitlyHidden;
}

void FySplitterPrivate::setSizes(const QList<int>& requestedSizes, NegativeSizeMode negativeSizeMode)
{
    m_rebaseSizesOnNextResize = false;

    for(int index{0}; index < size(); ++index) {
        SplitterLayoutStruct* item = m_list.at(index);
        item->storedSize           = requestedSizes.value(index);

        if(negativeSizeMode == NegativeSizeMode::ClampToZero && item->storedSize < 0) {
            item->storedSize = 0;
        }

        const int minWidgetLength = axisLength(smartMinSize(item->widget));
        item->collapsed           = item->storedSize == 0 && collapsible(item) && minWidgetLength > 0;

        if(!item->collapsed) {
            item->storedSize = std::max(item->storedSize, minWidgetLength);
        }
    }

    doResize();
}

FySplitter::FySplitter(QWidget* parent)
    : FySplitter{Qt::Vertical, parent}
{ }

FySplitter::FySplitter(Qt::Orientation orientation, QWidget* parent)
    : QFrame{parent}
    , p{std::make_unique<FySplitterPrivate>(this, orientation)}
{
    p->init();
}

FySplitter::~FySplitter()
{
#if QT_CONFIG(rubberband)
    delete p->m_rubberBand;
#endif
    while(!p->m_list.isEmpty()) {
        delete p->m_list.takeFirst();
    }
}

void FySplitter::addWidget(QWidget* widget)
{
    insertWidget(p->size(), widget);
}

void FySplitter::insertWidget(int index, QWidget* widget)
{
    p->insertWidget(index, widget, true);
}

QWidget* FySplitter::replaceWidget(int index, QWidget* replacement)
{
    if(!replacement) {
        qCWarning(FY_SPLITTER) << "Replacement widget cannot be null";
        return nullptr;
    }

    if(index < 0 || index >= p->size()) {
        qCWarning(FY_SPLITTER) << "Replacement index out of range:" << index;
        return nullptr;
    }

    SplitterLayoutStruct* item = p->m_list.at(index);
    QWidget* currentWidget     = item->widget;
    if(currentWidget == replacement) {
        qCWarning(FY_SPLITTER) << "Cannot replace a widget with itself";
        return nullptr;
    }

    if(replacement->parentWidget() == this) {
        qCWarning(FY_SPLITTER) << "Cannot replace a widget with one of its siblings";
        return nullptr;
    }

    const QScopedValueRollback blockChildAdd{p->m_blockChildAdd, true};

    const QRect currentGeometry = currentWidget->geometry();
    const bool currentWasHidden = currentWidget->isHidden();

    item->widget = replacement;
    currentWidget->setParent(nullptr);
    replacement->setParent(this);

    // The layout item's geometry is already set and should not change
    replacement->setGeometry(currentGeometry);
    replacement->lower();

    if(currentWasHidden) {
        replacement->hide();
    }
    else if(p->shouldShowWidget(replacement)) {
        replacement->show();
    }

    return currentWidget;
}

Qt::Orientation FySplitter::orientation() const
{
    return p->m_orientation;
}

void FySplitter::setOrientation(Qt::Orientation orientation)
{
    if(p->m_orientation == orientation) {
        return;
    }

    if(!testAttribute(Qt::WA_WState_OwnSizePolicy)) {
        setSizePolicy(sizePolicy().transposed());
        setAttribute(Qt::WA_WState_OwnSizePolicy, false);
    }

    p->m_orientation             = orientation;
    p->m_rebaseSizesOnNextResize = true;

    for(int i{0}; i < p->size(); ++i) {
        SplitterLayoutStruct* item = p->m_list.at(i);
        item->handle->setOrientation(orientation);
    }

    p->recalculate(isVisible());
}

bool FySplitter::childrenCollapsible() const
{
    return p->m_childrenCollapsible;
}

void FySplitter::setChildrenCollapsible(bool collapse)
{
    p->m_childrenCollapsible = collapse;
}

bool FySplitter::isCollapsible(int index) const
{
    if(index < 0 || index >= p->size()) {
        qCWarning(FY_SPLITTER) << "Cannot query collapsibility; index out of range:" << index;
        return false;
    }
    return p->m_list.at(index)->collapsibility != Collapsibility::NotCollapsible;
}

void FySplitter::setCollapsible(int index, bool collapse)
{
    if(index < 0 || index >= p->size()) {
        qCWarning(FY_SPLITTER) << "Cannot set collapsibility; index out of range:" << index;
        return;
    }
    p->m_list.at(index)->collapsibility = collapse ? Collapsibility::Collapsible : Collapsibility::NotCollapsible;
}

bool FySplitter::isLocked(int index) const
{
    if(index < 0 || index >= p->size()) {
        qCWarning(FY_SPLITTER) << "Cannot query lock state; index out of range:" << index;
        return false;
    }
    return p->m_list.at(index)->locked;
}

bool FySplitter::setLocked(int index, bool locked)
{
    if(index < 0 || index >= p->size()) {
        qCWarning(FY_SPLITTER) << "Cannot set lock state; index out of range:" << index;
        return false;
    }

    SplitterLayoutStruct* item = p->m_list.at(index);
    if(item->locked == locked) {
        return true;
    }

    if(locked) {
        const bool hasUnlockedSibling = std::ranges::any_of(p->m_list, [item](const SplitterLayoutStruct* candidate) {
            return candidate != item && !candidate->locked;
        });
        if(!hasUnlockedSibling) {
            return false;
        }

        item->storedSize = p->axisLength(item->rect.size());
        if(item->storedSize <= 0) {
            item->storedSize = p->axisLength(item->widget->size());
        }
    }

    item->locked = locked;
    updateGeometry();
    return true;
}

bool FySplitter::opaqueResize() const
{
    return p->m_opaqueResizeSet ? p->m_opaque : style()->styleHint(QStyle::SH_Splitter_OpaqueResize, nullptr, this);
}

void FySplitter::setOpaqueResize(bool opaque)
{
    p->m_opaqueResizeSet = true;
    p->m_opaque          = opaque;
}

void FySplitter::refresh()
{
    p->recalculate(true);
}

QSize FySplitter::minimumSizeHint() const
{
    ensurePolished();

    int minAxisLength{0};
    int minCrossAxisLength{0};

    for(int index{0}; index < p->size(); ++index) {
        const SplitterLayoutStruct* item = p->m_list.at(index);
        const QWidget* widget            = item->widget;
        if(widget->isHidden()) {
            continue;
        }

        const QSize minWidgetSize = smartMinSize(widget);
        if(minWidgetSize.isValid()) {
            int minWidgetAxisLength = p->axisLength(minWidgetSize);
            if(item->locked && item->storedSize >= 0) {
                minWidgetAxisLength = qMax(minWidgetAxisLength, item->storedSize);
            }

            minAxisLength += minWidgetAxisLength;
            minCrossAxisLength = qMax(minCrossAxisLength, p->crossAxisLength(minWidgetSize));
        }

        if(!item->handle || item->handle->isHidden()) {
            continue;
        }

        const QSize handleSizeHint = item->handle->sizeHint();
        if(handleSizeHint.isValid()) {
            minAxisLength += p->axisLength(handleSizeHint);
            minCrossAxisLength = qMax(minCrossAxisLength, p->crossAxisLength(handleSizeHint));
        }
    }

    return orientation() == Qt::Horizontal ? QSize{minAxisLength, minCrossAxisLength}
                                           : QSize{minCrossAxisLength, minAxisLength};
}

QSize FySplitter::sizeHint() const
{
    ensurePolished();

    int preferredAxisLength{0};
    int preferredCrossAxisLength{0};

    for(int index{0}; index < p->size(); ++index) {
        const QWidget* widget = p->m_list.at(index)->widget;
        if(widget->isHidden()) {
            continue;
        }

        const QSize widgetSizeHint = widget->sizeHint();
        if(widgetSizeHint.isValid()) {
            preferredAxisLength += p->axisLength(widgetSizeHint);
            preferredCrossAxisLength = qMax(preferredCrossAxisLength, p->crossAxisLength(widgetSizeHint));
        }
    }

    return orientation() == Qt::Horizontal ? QSize{preferredAxisLength, preferredCrossAxisLength}
                                           : QSize{preferredCrossAxisLength, preferredAxisLength};
}

QList<int> FySplitter::sizes() const
{
    ensurePolished();

    const int numSizes = p->size();
    QList<int> list;
    list.reserve(numSizes);

    for(int i{0}; i < numSizes; ++i) {
        const SplitterLayoutStruct* item = p->m_list.at(i);
        list.append(p->axisLength(item->rect.size()));
    }

    return list;
}

void FySplitter::setSizes(const QList<int>& list)
{
    p->setSizes(list, NegativeSizeMode::ClampToZero);
}

QByteArray FySplitter::saveState() const
{
    SplitterState state;
    state.sizes.reserve(p->size());

    for(const SplitterLayoutStruct* item : std::as_const(p->m_list)) {
        state.sizes.append(item->storedSize);
    }

    state.childrenCollapsible = childrenCollapsible();
    state.handleWidth         = p->m_handleWidth;
    state.opaqueResize        = opaqueResize();
    state.orientation         = orientation();
    state.opaqueResizeSet     = p->m_opaqueResizeSet;
    return encodeSplitterState(state);
}

bool FySplitter::restoreState(const QByteArray& state)
{
    const auto decodedState = decodeSplitterState(state);
    if(!decodedState) {
        return false;
    }

    setOrientation(decodedState->orientation);
    setChildrenCollapsible(decodedState->childrenCollapsible);
    setHandleWidth(decodedState->handleWidth);
    setOpaqueResize(decodedState->opaqueResize);

    p->m_opaqueResizeSet = decodedState->opaqueResizeSet;
    p->setSizes(decodedState->sizes, NegativeSizeMode::Preserve);

    p->doResize();
    updateGeometry();

    return true;
}

int FySplitter::handleWidth() const
{
    if(p->m_handleWidth >= 0) {
        return p->m_handleWidth;
    }

    return style()->pixelMetric(QStyle::PM_SplitterWidth, nullptr, this);
}

void FySplitter::setHandleWidth(int width)
{
    p->m_handleWidth = width;
    p->updateHandles();
}

int FySplitter::indexOf(QWidget* widget) const
{
    for(int i{0}; i < p->size(); ++i) {
        const SplitterLayoutStruct* item = p->m_list.at(i);
        if(item->widget == widget || item->handle.get() == widget) {
            return i;
        }
    }
    return -1;
}

QWidget* FySplitter::widget(int index) const
{
    if(index < 0 || index >= p->size()) {
        return nullptr;
    }
    return p->m_list.at(index)->widget;
}

int FySplitter::count() const
{
    return p->size();
}

void FySplitter::getRange(int index, int* /*farMin*/, int* min, int* max, int* /*farMax*/) const
{
    p->getRange(index, min, nullptr, nullptr, max);
}

FySplitterHandle* FySplitter::handle(int index) const
{
    if(index < 0 || index >= p->size()) {
        return nullptr;
    }
    return p->m_list.at(index)->handle.get();
}

void FySplitter::setStretchFactor(int index, int stretch)
{
    if(index <= -1 || index >= p->size()) {
        return;
    }

    QWidget* widget = p->m_list.at(index)->widget;
    QSizePolicy sp  = widget->sizePolicy();
    sp.setHorizontalStretch(stretch);
    sp.setVerticalStretch(stretch);
    widget->setSizePolicy(sp);
}

FySplitterHandle* FySplitter::createHandle()
{
    return new FySplitterHandle(p->m_orientation, this);
}

void FySplitter::childEvent(QChildEvent* event)
{
    if(event->added()) {
        if(!event->child()->isWidgetType()) {
            return;
        }

        auto* child = static_cast<QWidget*>(event->child());
        if(!p->m_blockChildAdd && !child->isWindow() && !p->findWidget(child)) {
            p->insertWidget(p->size(), child, false);
        }
    }
    else if(event->polished()) {
        if(!event->child()->isWidgetType()) {
            return;
        }

        auto* child = static_cast<QWidget*>(event->child());
        if(!p->m_blockChildAdd && !child->isWindow() && p->shouldShowWidget(child)) {
            child->show();
        }
    }
    else if(event->removed()) {
        const QObject* child = event->child();

        for(int i{0}; i < p->size(); ++i) {
            const SplitterLayoutStruct* item = p->m_list.at(i);
            if(item->widget == child) {
                p->m_list.removeAt(i);
                delete item;
                p->recalculate(isVisible());
                return;
            }
        }
    }
}

bool FySplitter::event(QEvent* event)
{
    switch(event->type()) {
        case QEvent::Hide:
            // Reset firstShow to false here since things can be done to the splitter in between
            if(!p->m_firstShow) {
                p->m_firstShow = true;
            }
            break;
        case QEvent::Show:
            if(!p->m_firstShow) {
                break;
            }
            p->m_firstShow = false;
            Q_FALLTHROUGH();
        case QEvent::HideToParent:
        case QEvent::ShowToParent:
        case QEvent::LayoutRequest:
            p->recalculate(isVisible());
            break;
        default:;
    }
    return QFrame::event(event);
}

void FySplitter::resizeEvent(QResizeEvent* /*event*/)
{
    p->doResize();
}

void FySplitter::changeEvent(QEvent* event)
{
    if(event->type() == QEvent::StyleChange) {
        p->updateHandles();
    }
    QFrame::changeEvent(event);
}

void FySplitter::moveSplitter(int requestedPosition, int handleIndex)
{
    SplitterLayoutStruct* forwardItem = p->m_list.at(handleIndex);

    int farMinPosition{0};
    int minPosition{0};
    int maxPosition{0};
    int farMaxPosition{0};
    const int position
        = p->adjustPos(requestedPosition, handleIndex, &farMinPosition, &minPosition, &maxPosition, &farMaxPosition);

    const int previousPosition = p->axisPosition(forwardItem->rect.topLeft());

    const int itemCount = p->size();
    QVarLengthArray<int, 32> itemPositions(itemCount);
    QVarLengthArray<int, 32> itemLengths(itemCount);

    const bool allowForwardCollapse = p->collapsible(forwardItem) && position > maxPosition;
    p->calculateMove(MoveDirection::Forward, position, handleIndex, allowForwardCollapse, itemPositions.data(),
                     itemLengths.data());

    const bool allowBackwardCollapse = p->collapsible(handleIndex - 1) && position < minPosition;
    p->calculateMove(MoveDirection::Backward, position, handleIndex - 1, allowBackwardCollapse, itemPositions.data(),
                     itemLengths.data());

    const bool moveTowardStart = position < previousPosition;
    int itemIndex              = moveTowardStart ? 0 : itemCount - 1;
    const int indexStep        = moveTowardStart ? 1 : -1;

    for(; itemIndex >= 0 && itemIndex < itemCount; itemIndex += indexStep) {
        SplitterLayoutStruct* item = p->m_list.at(itemIndex);
        if(!item->widget->isHidden()) {
            p->setItemGeometry(item, itemPositions[itemIndex], itemLengths[itemIndex], true);
        }
    }

    p->storeSizes();

    Q_EMIT splitterMoved(position, handleIndex);
}

void FySplitter::setRubberBand(int position)
{
#if QT_CONFIG(rubberband)
    if(position < 0) {
        if(p->m_rubberBand) {
            p->m_rubberBand->deleteLater();
        }
        return;
    }

    if(!p->m_rubberBand) {
        const QScopedValueRollback blockChildAdd{p->m_blockChildAdd, true};
        p->m_rubberBand = new QRubberBand(QRubberBand::Line, this);
        p->m_rubberBand->setObjectName("qt_rubberband"_L1);
    }

    static constexpr int RubberBandHalfWidth{3};

    const QRect contentRect{contentsRect()};
    const int handleCenterPosition = position + (handleWidth() / 2);
    const QRect rubberBandGeometry = p->m_orientation == Qt::Horizontal
                                       ? QRect{QPoint{handleCenterPosition - RubberBandHalfWidth, contentRect.y()},
                                               QSize{2 * RubberBandHalfWidth, contentRect.height()}}
                                       : QRect{QPoint{contentRect.x(), handleCenterPosition - RubberBandHalfWidth},
                                               QSize{contentRect.width(), 2 * RubberBandHalfWidth}};

    p->m_rubberBand->setGeometry(rubberBandGeometry);
    p->m_rubberBand->show();
#else
    Q_UNUSED(position);
#endif
}

int FySplitter::closestLegalPosition(int requestedPosition, int handleIndex)
{
    int farMinPosition{0};
    int minPosition{0};
    int maxPosition{0};
    int farMaxPosition{0};
    return p->adjustPos(requestedPosition, handleIndex, &farMinPosition, &minPosition, &maxPosition, &farMaxPosition);
}

class FySplitterHandlePrivate
{
public:
    FySplitterHandlePrivate(FySplitter* splitter, Qt::Orientation orientation)
        : m_splitter{splitter}
        , m_orientation{orientation}
    { }

    [[nodiscard]] int axisPosition(const QPoint& position) const
    {
        return m_orientation == Qt::Horizontal ? position.x() : position.y();
    }

    FySplitter* m_splitter;
    Qt::Orientation m_orientation;
    int m_mouseOffset{0};
    bool m_opaq{false};
    bool m_hover{false};
    bool m_pressed{false};
};

FySplitterHandle::FySplitterHandle(Qt::Orientation orientation, FySplitter* parent)
    : QWidget{parent}
    , p{std::make_unique<FySplitterHandlePrivate>(parent, orientation)}
{
    setOrientation(orientation);
}

FySplitterHandle::~FySplitterHandle() = default;

void FySplitterHandle::setOrientation(Qt::Orientation orientation)
{
    p->m_orientation = orientation;
#ifndef QT_NO_CURSOR
    setCursor(orientation == Qt::Horizontal ? Qt::SplitHCursor : Qt::SplitVCursor);
#endif
}

Qt::Orientation FySplitterHandle::orientation() const
{
    return p->m_orientation;
}

bool FySplitterHandle::opaqueResize() const
{
    return p->m_splitter->opaqueResize();
}

FySplitter* FySplitterHandle::splitter() const
{
    return p->m_splitter;
}

QSize FySplitterHandle::sizeHint() const
{
    QStyleOption opt{0};

    opt.initFrom(p->m_splitter);
    opt.state = QStyle::State_None;

    if(auto* pWidget = parentWidget()) {
        const int width = p->m_splitter->handleWidth();
        return pWidget->style()->sizeFromContents(QStyle::CT_Splitter, &opt, {width, width}, p->m_splitter);
    }

    return {};
}

void FySplitterHandle::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter{this};

    QStyleOption opt{0};
    opt.rect    = contentsRect();
    opt.palette = palette();

    if(orientation() == Qt::Horizontal) {
        opt.state = QStyle::State_Horizontal;
    }
    else {
        opt.state = QStyle::State_None;
    }

    if(p->m_hover) {
        opt.state |= QStyle::State_MouseOver;
    }
    if(p->m_pressed) {
        opt.state |= QStyle::State_Sunken;
    }
    if(isEnabled()) {
        opt.state |= QStyle::State_Enabled;
    }

    parentWidget()->style()->drawControl(QStyle::CE_Splitter, &opt, &painter, p->m_splitter);
}

void FySplitterHandle::mouseMoveEvent(QMouseEvent* event)
{
    if(!p->m_pressed) {
        return;
    }

    const int pos
        = p->axisPosition(parentWidget()->mapFromGlobal(event->globalPosition()).toPoint()) - p->m_mouseOffset;
    if(opaqueResize()) {
        moveSplitter(pos);
    }
    else {
        p->m_splitter->setRubberBand(closestLegalPosition(pos));
    }
}

void FySplitterHandle::mousePressEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton) {
        p->m_mouseOffset = p->axisPosition(event->position().toPoint());
        p->m_pressed     = true;
        update();
    }
}

void FySplitterHandle::mouseReleaseEvent(QMouseEvent* event)
{
    if(!p->m_pressed) {
        return;
    }

    if(!opaqueResize()) {
        const int pos
            = p->axisPosition(parentWidget()->mapFromGlobal(event->globalPosition()).toPoint()) - p->m_mouseOffset;
        p->m_splitter->setRubberBand(-1);
        moveSplitter(pos);
    }

    p->m_pressed = false;
    update();
}

void FySplitterHandle::resizeEvent(QResizeEvent* event)
{
    // Ensure the actual grab area is at least 4 or 5 pixels
    const int handleMargin = (5 - p->m_splitter->handleWidth()) / 2;

    const bool useTinyMode = handleMargin > 0;
    setAttribute(Qt::WA_MouseNoMask, useTinyMode);

    if(useTinyMode) {
        if(orientation() == Qt::Horizontal) {
            setContentsMargins(handleMargin, 0, handleMargin, 0);
        }
        else {
            setContentsMargins(0, handleMargin, 0, handleMargin);
        }
        setMask(QRegion(contentsRect()));
    }
    else {
        setContentsMargins({});
        clearMask();
    }

    QWidget::resizeEvent(event);
}

bool FySplitterHandle::event(QEvent* event)
{
    switch(event->type()) {
        case QEvent::HoverEnter:
            p->m_hover = true;
            update();
            break;
        case QEvent::HoverLeave:
            p->m_hover = false;
            update();
            break;
        default:
            break;
    }
    return QWidget::event(event);
}

void FySplitterHandle::moveSplitter(int pos)
{
    int& resizeDepth = userResizeDepth();
    const QScopedValueRollback userResize{resizeDepth, resizeDepth + 1};

    if(p->m_splitter->isRightToLeft() && p->m_orientation == Qt::Horizontal) {
        pos = p->m_splitter->contentsRect().width() - pos;
    }
    p->m_splitter->moveSplitter(pos, p->m_splitter->indexOf(this));
}

int FySplitterHandle::closestLegalPosition(int pos)
{
    FySplitter* splitter = p->m_splitter;
    if(splitter->isRightToLeft() && p->m_orientation == Qt::Horizontal) {
        const int w = splitter->contentsRect().width();
        return w - splitter->closestLegalPosition(w - pos, splitter->indexOf(this));
    }
    return splitter->closestLegalPosition(pos, splitter->indexOf(this));
}
} // namespace Fooyin

#include "moc_fysplitter.cpp"
