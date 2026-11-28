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

#include "itemviewstyle.h"

#include "styleutils.h"
#include "theme/itemviewappearance.h"

#include <gui/widgets/expandedtreeview.h>

#include <QAbstractItemView>
#include <QCursor>
#include <QHeaderView>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionViewItem>
#include <QTableView>
#include <QTreeView>

#include <array>
#include <ranges>

constexpr auto HoverAttributeProperty = "_fooyin_item_view_hover";

namespace {
QColor stateColour(const QPalette& palette, QPalette::ColorGroup group, const Fooyin::ColourSource& source, int opacity,
                   int lightness = 100)
{
    auto colour = Fooyin::resolveBrush(source, palette, group).color().lighter(lightness);
    colour.setAlphaF(colour.alphaF() * static_cast<float>(opacity / 100.0));
    return colour;
}

QBrush stateBrush(const QStyleOptionViewItem& option, const Fooyin::ItemViewAppearance::State& state,
                  const QRectF& bounds)
{
    const auto group = Fooyin::Style::colourGroup(option);
    const auto brush = Fooyin::resolveBrush(state.backgroundColour, option.palette, group, bounds, Qt::Horizontal,
                                            option.direction == Qt::RightToLeft);
    if(const auto* source = brush.gradient()) {
        auto gradient{*source};
        auto stops = gradient.stops();
        for(auto& colour : stops | std::views::values) {
            colour = stateColour(option.palette, group, colour, state.backgroundOpacity, state.backgroundLightness);
        }
        gradient.setStops(stops);
        return QBrush{gradient};
    }
    return QBrush{
        stateColour(option.palette, group, brush.color(), state.backgroundOpacity, state.backgroundLightness)};
}

double outlineInset(const Fooyin::ItemViewAppearance& appearance)
{
    // Every state uses the same path, including delegate-supplied playing backgrounds
    return std::max({appearance.normal.borderWidth, appearance.alternate.borderWidth, appearance.hovered.borderWidth,
                     appearance.selected.borderWidth, appearance.selectedHovered.borderWidth,
                     appearance.customBorderWidth, appearance.focus.width})
         / 2.0;
}

QWidget* itemViewport(QWidget* widget)
{
    if(const auto* view = qobject_cast<QAbstractItemView*>(widget)) {
        return view->viewport();
    }
    if(const auto* view = qobject_cast<QAbstractItemView*>(widget->parentWidget());
       view && view->viewport() == widget) {
        return widget;
    }
    return nullptr;
}

bool branchRowHovered(const QStyleOption& option, const QWidget* widget)
{
    const auto* view = qobject_cast<const QTreeView*>(widget);
    if(!view || view->selectionBehavior() != QAbstractItemView::SelectRows) {
        return false;
    }

    const auto* viewport = view->viewport();
    if(!viewport->underMouse()) {
        return false;
    }

    // Qt omits row hover from branch options when decoration selection is disabled
    const QPoint position = viewport->mapFromGlobal(QCursor::pos());
    return viewport->rect().contains(position) && position.y() >= option.rect.top()
        && position.y() <= option.rect.bottom();
}
} // namespace

namespace Fooyin::Style::ItemView {
void drawBranch(const QStyleOption& option, QPainter& painter, const QWidget* widget,
                const ItemViewAppearance& appearance)
{
    if(option.rect.isEmpty()
       || !(option.state
            & (QStyle::State_Children | QStyle::State_Item | QStyle::State_Sibling | QStyle::State_Open))) {
        return;
    }

    const auto* item = qstyleoption_cast<const QStyleOptionViewItem*>(&option);
    if(!widget && item) {
        widget = item->widget;
    }

    const bool selected = (option.state & QStyle::State_Selected) && (!item || item->showDecorationSelected);
    const bool hovered  = (option.state & QStyle::State_Enabled)
                       && ((option.state & QStyle::State_MouseOver)
                           || ((option.state & QStyle::State_Children) && branchRowHovered(option, widget)));
    const bool children = option.state & QStyle::State_Children;

    const QRect rect{option.rect};
    const QPoint centre{rect.x() + (rect.width() / 2), rect.y() + (rect.height() / 2)};
    const int extent = std::min({appearance.branch.arrowExtent, option.rect.width(), option.rect.height()});
    const int gap    = children ? (extent + 1) / 2 : 0;

    const auto& branch = appearance.branch;
    const auto lineColour
        = stateColour(option.palette, colourGroup(option), selected ? branch.selectedLineColour : branch.lineColour,
                      selected ? branch.selectedLineOpacity : branch.lineOpacity);
    const double width = branch.lineWidth;

    painter.save();
    painter.setClipRect(option.rect, Qt::IntersectClip);

    const int topHeight = std::max(0, centre.y() - gap - rect.top() + (children ? 0 : 1));
    painter.fillRect(
        QRectF{static_cast<double>(centre.x()), static_cast<double>(rect.top()), width, static_cast<double>(topHeight)},
        lineColour);

    if(option.state & QStyle::State_Sibling) {
        const int top = centre.y() + gap + 1;
        painter.fillRect(QRectF{static_cast<double>(centre.x()), static_cast<double>(top), width,
                                static_cast<double>(std::max(0, rect.bottom() - top + 1))},
                         lineColour);
    }
    if(option.state & QStyle::State_Item) {
        if(option.direction == Qt::RightToLeft) {
            painter.fillRect(QRectF{static_cast<double>(rect.left()), static_cast<double>(centre.y()),
                                    static_cast<double>(std::max(0, centre.x() - gap - rect.left())), width},
                             lineColour);
        }
        else {
            const int left = centre.x() + gap + 1;
            painter.fillRect(QRectF{static_cast<double>(left), static_cast<double>(centre.y()),
                                    static_cast<double>(std::max(0, rect.right() - left + 1)), width},
                             lineColour);
        }
    }
    if(children && extent > 0 && branch.arrowSize > 0 && (branch.filledArrows || branch.arrowWidth > 0)) {
        painter.translate(QRectF{rect}.center());
        if(!(option.state & QStyle::State_Open)) {
            painter.rotate(option.direction == Qt::RightToLeft ? 90.0 : -90.0);
        }
        const double scale = std::min(1.0, extent / std::max(12.0, branch.arrowSize + branch.arrowWidth));
        painter.scale(scale, scale);

        const auto& colour = selected ? branch.selectedArrowColour
                           : hovered  ? branch.hoveredArrowColour
                                      : branch.arrowColour;
        const double half  = branch.arrowSize / 2.0;
        const std::array points{QPointF{-half, -half / 2.0}, QPointF{0.0, half / 2.0}, QPointF{half, -half / 2.0}};
        const auto brush = resolveBrush(colour, option.palette, colourGroup(option));
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(branch.arrowWidth > 0
                           ? QPen{brush, branch.arrowWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin}
                           : QPen{Qt::NoPen});
        if(branch.filledArrows) {
            painter.setBrush(brush);
            painter.drawPolygon(points.data(), static_cast<int>(points.size()));
        }
        else {
            painter.drawPolyline(points.data(), static_cast<int>(points.size()));
        }
    }

    painter.restore();
}

void drawPanel(const QStyleOptionViewItem& option, QPainter& painter, const QWidget* widget, bool row,
               const ItemViewAppearance& appearance)
{
    if(option.rect.isEmpty()) {
        return;
    }

    const auto group            = colourGroup(option);
    const bool selected         = option.state & QStyle::State_Selected;
    const bool hovered          = (option.state & QStyle::State_Enabled) && (option.state & QStyle::State_MouseOver);
    const bool customBackground = !selected && option.backgroundBrush.style() != Qt::NoBrush;

    if(!widget) {
        widget = option.widget;
    }

    const auto* view    = qobject_cast<const QAbstractItemView*>(widget);
    const auto* tree    = qobject_cast<const QTreeView*>(widget);
    const auto* table   = qobject_cast<const QTableView*>(widget);
    const auto* header  = tree ? tree->header() : table ? table->horizontalHeader() : nullptr;
    const bool expanded = qobject_cast<const ExpandedTreeView*>(widget);

    QRectF rowBounds{option.rect};
    if(header) {
        rowBounds.setLeft(option.direction == Qt::RightToLeft
                              ? view->viewport()->width() + header->offset() - header->length()
                              : -header->offset());
        rowBounds.setWidth(header->length());
    }

    painter.save();
    painter.setClipRect(option.rect, Qt::IntersectClip);

    if(row) {
        const auto& base = option.features & QStyleOptionViewItem::Alternate ? appearance.alternate : appearance.normal;
        if(base.backgroundOpacity > 0) {
            painter.fillRect(option.rect, stateBrush(option, base, rowBounds));
        }
        if(!selected || !option.showDecorationSelected) {
            painter.restore();
            return;
        }
    }
    if(customBackground) {
        painter.setBrushOrigin(option.rect.topLeft());
    }
    const auto& state = selected ? (hovered ? appearance.selectedHovered : appearance.selected)
                      : hovered  ? appearance.hovered
                      : option.features & QStyleOptionViewItem::Alternate ? appearance.alternate
                                                                          : appearance.normal;
    if(!selected && !hovered && !customBackground && state.borderWidth <= 0) {
        painter.restore();
        return;
    }

    const bool joined = view && (view->selectionBehavior() == QAbstractItemView::SelectRows || expanded)
                     && (selected || customBackground || !qobject_cast<const QTableView*>(widget));

    const auto position = row && expanded ? QStyleOptionViewItem::OnlyOne : option.viewItemPosition;
    bool roundLeft = !joined || (position != QStyleOptionViewItem::Middle && position != QStyleOptionViewItem::End);
    bool roundRight
        = !joined || (position != QStyleOptionViewItem::Middle && position != QStyleOptionViewItem::Beginning);
    if(option.direction == Qt::RightToLeft) {
        std::swap(roundLeft, roundRight);
    }

    QRectF rect{option.rect};
    if(joined && table) {
        const int left = option.direction == Qt::RightToLeft
                           ? table->viewport()->width() + header->offset() - header->length()
                           : -header->offset();
        // Use visible row edges so hidden and reordered columns still share one outline
        roundLeft  = rect.left() <= left + 1;
        roundRight = rect.right() >= left + header->length() - 1;
    }

    rect.adjust(roundLeft ? appearance.horizontalInset : 0.0, appearance.verticalInset,
                roundRight ? -appearance.horizontalInset : 0.0, -appearance.verticalInset);

    if(!rect.isEmpty()) {
        const double inset = outlineInset(appearance);
        QRectF shape       = rect.adjusted(roundLeft ? inset : 0.0, inset, roundRight ? -inset : 0.0, -inset);
        if(shape.isEmpty()) {
            painter.restore();
            return;
        }
        const double extension = std::max(appearance.radius, inset + 1.0);
        shape.adjust(roundLeft ? 0.0 : -extension, 0.0, roundRight ? 0.0 : extension, 0.0);

        QPainterPath path;
        path.addRoundedRect(shape, appearance.radius, appearance.radius);
        painter.setClipRect(rect, Qt::IntersectClip);
        painter.setRenderHint(QPainter::Antialiasing);

        QBrush background;
        QColor outline;
        double borderWidth{state.borderWidth};
        if(customBackground) {
            background  = option.backgroundBrush;
            borderWidth = appearance.customBorderWidth;
            if(background.style() == Qt::SolidPattern) {
                outline = background.color();
            }
        }
        else {
            if(hovered && !selected) {
                const auto& base
                    = option.features & QStyleOptionViewItem::Alternate ? appearance.alternate : appearance.normal;
                const auto brush = option.palette.brush(group, QPalette::Base);
                if(brush.style() == Qt::SolidPattern && brush.color().alpha() == 255) {
                    painter.fillPath(path, brush);
                    if(base.backgroundOpacity > 0) {
                        painter.fillPath(path, stateBrush(option, base, rowBounds));
                    }
                }
            }
            // Normal and alternate surfaces are painted by PE_PanelItemViewRow
            if(selected || hovered) {
                background = stateBrush(option, state, joined ? rowBounds : shape);
            }
            outline = stateColour(option.palette, group, state.borderColour, state.borderOpacity,
                                  state.backgroundLightness);
        }

        painter.fillPath(path, background);
        if(borderWidth > 0 && outline.isValid() && outline.alpha() > 0) {
            painter.strokePath(path, QPen{outline, borderWidth});
        }
    }

    painter.restore();
}

void drawFocus(const QStyleOption& option, QPainter& painter, const ItemViewAppearance& appearance)
{
    const auto& focus = appearance.focus;
    if(focus.width <= 0 || !(option.state & QStyle::State_Enabled)
       || (focus.keyboardOnly && !(option.state & QStyle::State_KeyboardFocusChange))) {
        return;
    }

    const double horizontal = appearance.horizontalInset + outlineInset(appearance) + focus.inset;
    const double vertical   = appearance.verticalInset + outlineInset(appearance) + focus.inset;
    const QRectF rect       = QRectF{option.rect}.adjusted(horizontal, vertical, -horizontal, -vertical);
    if(rect.isEmpty()) {
        return;
    }

    const auto group = colourGroup(option);
    auto colour      = resolveBrush(focus.colour, option.palette, group).color();
    if(option.state & QStyle::State_Selected) {
        const auto lighter = colour.lighter(focus.selectedLightness);
        colour             = lighter == colour ? option.palette.color(group, QPalette::HighlightedText) : lighter;
        colour.setAlphaF(colour.alphaF() * static_cast<float>(focus.selectedOpacity / 100.0));
    }

    colour.setAlphaF(colour.alphaF() * static_cast<float>(focus.opacity / 100.0));
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen{colour, focus.width});
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(rect, appearance.radius, appearance.radius);
    painter.restore();
}

void polish(QWidget* widget)
{
    if(auto* viewport = itemViewport(widget); viewport && !viewport->testAttribute(Qt::WA_Hover)) {
        viewport->setProperty(HoverAttributeProperty, true);
        viewport->setAttribute(Qt::WA_Hover, true);
    }
}

void unpolish(QWidget* widget)
{
    if(auto* viewport = itemViewport(widget); viewport && viewport->property(HoverAttributeProperty).toBool()) {
        viewport->setAttribute(Qt::WA_Hover, false);
        viewport->setProperty(HoverAttributeProperty, {});
    }
}
} // namespace Fooyin::Style::ItemView
