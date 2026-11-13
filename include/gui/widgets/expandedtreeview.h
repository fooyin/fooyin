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

#pragma once

#include "fygui_export.h"

#include <gui/coverartworktypes.h>

#include <QAbstractItemView>

#include <set>

class QHeaderView;

namespace Fooyin {
class ExpandedTreeViewPrivate;

struct FYGUI_EXPORT CoverFlowSettings
{
    CoverStyle style{CoverStyle::Cover};
    bool autoFitArtwork{true};
    int sideArtworkTilt{55};
    int centreSpacing{70};
    int sideSpacing{30};
    int itemsPerSide{0};
    int sizeProgression{85};
    int minArtworkSize{0};
    int perspective{100};
    int backgroundBlending{30};
    bool showReflection{true};
    int reflectionHeight{30};
    int reflectionOpacity{40};
    int reflectionGap{2};
    bool showLabels{true};
    bool labelsAboveArtwork{false};
    bool labelsOnArtwork{false};
    bool overlayLabelsOnReflections{false};
    int labelSpacing{10};
    bool animateScrolling{true};
    int animationDuration{200};
    int scrollSensitivity{100};
    int frameWidth{0};

    void normalise();

    [[nodiscard]] QJsonObject toJson() const;
    void load(const QJsonObject& object);

    bool operator==(const CoverFlowSettings& other) const = default;
};

class FYGUI_EXPORT ExpandedTreeView : public QAbstractItemView
{
    Q_OBJECT

public:
    enum class ViewMode : uint8_t
    {
        Tree = 0,
        Icon,
        Flow,
        CoverFlow,
    };
    Q_ENUM(ViewMode)

    enum class CaptionDisplay : uint8_t
    {
        None = 0,
        Bottom,
        Right,
        Overlay
    };
    Q_ENUM(CaptionDisplay)

    enum class ExpansionPosition : uint8_t
    {
        Below = 0,
        Above,
        Automatic,
    };
    Q_ENUM(ExpansionPosition)

    class CoverFlowStyleOption : public QStyleOptionViewItem
    {
    public:
        enum StyleOptionType
        {
            Type = SO_CustomBase + 1
        };

        enum StyleOptionVersion
        {
            Version = 1
        };

        enum class Element : uint8_t
        {
            Cover,
            Reflection,
            Caption
        };

        CoverFlowStyleOption()
        {
            type    = Type;
            version = Version;
        }

        CoverFlowSettings settings;
        Element element{Element::Cover};
        qreal depth{0.0};
        int cornerRadius{0};
    };

    explicit ExpandedTreeView(QWidget* parent = nullptr);
    ~ExpandedTreeView() override;

    [[nodiscard]] QHeaderView* header() const;
    void setHeader(QHeaderView* header);

    [[nodiscard]] bool isHeaderHidden() const;
    void setHeaderHidden(bool hide);

    void setModel(QAbstractItemModel* model) override;

    [[nodiscard]] bool isSortingEnabled() const;
    void setSortingEnabled(bool enabled);
    void sortByColumn(int column, Qt::SortOrder order);

    [[nodiscard]] bool isSpanning(int column) const;
    void setSpan(int column, bool span);
    [[nodiscard]] bool extendSpansIntoParents() const;
    void setExtendSpansIntoParents(bool enabled);

    [[nodiscard]] ViewMode viewMode() const;
    void setViewMode(ViewMode mode);

    [[nodiscard]] CoverFlowSettings coverFlowSettings() const;
    void setCoverFlowSettings(const CoverFlowSettings& settings);

    void setFlowColumnGap(int gap);
    void setFlowMinimumRows(int rows);
    void setFlowMaximumColumns(int columns);
    void setFlowMinimumColumnWidth(int width);
    void setFlowStretchColumns(bool stretch);

    [[nodiscard]] QSize contentSizeForWidth(int width) const;

    [[nodiscard]] CaptionDisplay captionDisplay() const;
    void setCaptionDisplay(CaptionDisplay display);

    [[nodiscard]] bool overlayCaptionsOnInteraction() const;
    void setOverlayCaptionsOnInteraction(bool enabled);

    [[nodiscard]] int iconItemColumn() const;
    void setIconItemColumn(int column);

    [[nodiscard]] bool uniformRowHeights() const;
    void setUniformRowHeights(bool enabled);

    [[nodiscard]] bool wordWrap() const;
    void setWordWrap(bool enabled);

    [[nodiscard]] int uniformHeightRole() const;
    void setUniformHeightRole(int role);

    [[nodiscard]] bool selectIgnoreParents() const;
    void setSelectIgnoreParents(bool enabled);

    [[nodiscard]] int indentation() const;
    void setIndentation(int indent);
    void resetIndentation();

    [[nodiscard]] bool selectBeforeDrag() const;
    void setSelectBeforeDrag(bool enabled);

    [[nodiscard]] int iconHorizontalGap() const;
    void setIconHorizontalGap(int gap);
    [[nodiscard]] int iconVerticalGap() const;
    void setIconVerticalGap(int gap);

    //! Item bounds including its share of surrounding gaps in icon mode, in viewport coordinates.
    [[nodiscard]] QRect iconItemPaintRect(const QModelIndex& index) const;

    //! Uses the grid top margin for the first icon row and the row gap for later rows.
    [[nodiscard]] int iconScrollTopMargin(const QModelIndex& index) const;
    //! Overrides the automatic margins around the icon grid.
    void setIconGridMargins(const QMargins& margins);
    void resetIconGridMargins();

    //! Corner radius of icon item backgrounds, as a percentage from 0 to 100.
    [[nodiscard]] int iconItemCornerRadius() const;
    void setIconItemCornerRadius(int radius);
    //! Applies rounding only when artwork margins are zero along the rounded edges.
    void setIconItemCornerRadius(int radius, const QMargins& artworkMargins);

    [[nodiscard]] bool autoFitArtwork() const;
    void setAutoFitArtwork(bool enabled);
    [[nodiscard]] QSize effectiveIconSize() const;

    //! Use grid spacing for side captions. Artwork fitting applies only to other caption layouts.
    [[nodiscard]] bool useIconGapsForSideCaptions() const;
    void setUseIconGapsForSideCaptions(bool enabled);

    [[nodiscard]] QSize minimumIconSize() const;
    void setMinimumIconSize(const QSize& size);
    [[nodiscard]] QSize maximumIconSize() const;
    void setMaximumIconSize(const QSize& size);
    void changeIconSize(const QSize& size);

    [[nodiscard]] QModelIndex expandedIndex() const;
    void prepareExpansionChange();
    void setExpandedIndex(const QModelIndex& index);
    void setExpansionPosition(ExpansionPosition position);
    void setExpansionWidget(QWidget* widget);
    void setExpansionAnimated(bool enabled);
    void setExpansionAnimationDuration(int duration);
    //! Aligns the expanded panel with the outer bounds of the icon grid.
    void setExpansionMatchGridMargins(bool enabled);
    void refreshExpansion();

    [[nodiscard]] QRect visualRect(const QModelIndex& index) const override;
    [[nodiscard]] int sizeHintForColumn(int column) const override;
    void scrollTo(const QModelIndex& index, ScrollHint hint) override;
    [[nodiscard]] QModelIndex indexAt(const QPoint& point) const override;

    [[nodiscard]] QModelIndexList visibleIndexes(int margin = 0) const;
    [[nodiscard]] QModelIndex findIndexAt(const QPoint& point, bool includeSpans, bool includePadding = false) const;
    [[nodiscard]] QModelIndex indexAbove(const QModelIndex& index) const;
    [[nodiscard]] QModelIndex indexBelow(const QModelIndex& index) const;

    void doItemsLayout() override;
    void reset() override;

    void selectAll() override;

Q_SIGNALS:
    void viewModeChanged(Fooyin::ExpandedTreeView::ViewMode mode);
    void middleClicked(const QModelIndex& index);
    void contentSizeChanged();
    void effectiveIconSizeChanged(const QSize& size);

protected:
    friend class ExpandedTreeViewPrivate;
    friend class BaseView;
    friend class TreeView;
    friend class IconView;
    friend class FlowView;
    friend class CoverFlowView;

    void updateGeometries() override;
    void initViewItemOption(QStyleOptionViewItem* option) const override;
    void dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles) override;

    void changeEvent(QEvent* event) override;
    bool viewportEvent(QEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
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
    virtual QRect drawSpan(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
    [[nodiscard]] virtual int spanHeight(const QModelIndex& index) const;

    [[nodiscard]] virtual int adjustVerticalScrollMaximum(int maximum) const;

    QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) override;
    [[nodiscard]] int horizontalOffset() const override;
    [[nodiscard]] int verticalOffset() const override;
    [[nodiscard]] bool isIndexHidden(const QModelIndex& index) const override;
    [[nodiscard]] QItemSelectionModel::SelectionFlags selectionCommand(const QModelIndex& index,
                                                                       const QEvent* event) const override;
    void setSelection(const QRect& rect, QItemSelectionModel::SelectionFlags command) override;
    void currentChanged(const QModelIndex& current, const QModelIndex& previous) override;
    [[nodiscard]] QRegion visualRegionForSelection(const QItemSelection& selection) const override;
    [[nodiscard]] QModelIndexList selectedIndexes() const override;

private:
    std::unique_ptr<ExpandedTreeViewPrivate> p;
};
} // namespace Fooyin
