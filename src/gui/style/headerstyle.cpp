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

#include "headerstyle.h"

#include "styleutils.h"
#include "theme/headerappearance.h"

#include <QHeaderView>
#include <QLinearGradient>
#include <QPainter>
#include <QStyleOptionHeader>
#include <QVariant>
#include <QWidget>

#include <array>
#include <cmath>

constexpr auto DragTargetProperty = "_fooyin_header_drag_target";

namespace {
void updateDragTarget(const QStyleOptionHeader& option, const QPainter& painter, const QWidget* widget)
{
    const auto* header = qobject_cast<const QHeaderView*>(widget);
    if(!header || painter.device() != header->viewport()) {
        return;
    }

    auto* viewport       = header->viewport();
    const QRect previous = viewport->property(DragTargetProperty).toRect();
    const auto* extended = qstyleoption_cast<const QStyleOptionHeaderV2*>(&option);
    if(extended && extended->isSectionDragTarget && (option.state & QStyle::State_Enabled)) {
        if(previous != option.rect) {
            viewport->setProperty(DragTargetProperty, option.rect);
            // Qt repaints the new target and its neighbours, which may not include the previous target
            if(!previous.isEmpty()) {
                viewport->update(previous);
            }
        }
    }
    else if(previous == option.rect) {
        viewport->setProperty(DragTargetProperty, {});
    }
}

const Fooyin::HeaderAppearance::State& sectionState(const QStyleOptionHeader& option,
                                                    const Fooyin::HeaderAppearance& appearance)
{
    if(!(option.state & QStyle::State_Enabled)) {
        return appearance.disabled;
    }
    if(const auto* extended = qstyleoption_cast<const QStyleOptionHeaderV2*>(&option);
       extended && extended->isSectionDragTarget) {
        return appearance.dragTarget;
    }
    if(option.state & QStyle::State_Sunken) {
        return appearance.pressed;
    }
    if(option.state & QStyle::State_On) {
        return appearance.selected;
    }
    if(option.state & QStyle::State_MouseOver) {
        return appearance.hovered;
    }
    return option.sortIndicator != QStyleOptionHeader::None ? appearance.sorted : appearance.normal;
}

int contentPadding(int padding, const Fooyin::HeaderAppearance& appearance)
{
    return std::max(padding,
                    static_cast<int>(std::ceil(std::max(appearance.border.width, appearance.separator.width))));
}

void drawEdge(const QStyleOptionHeader& option, QPainter& painter, const Fooyin::HeaderAppearance& appearance)
{
    const auto& border = appearance.border;
    if(border.width <= 0 || border.opacity == 0) {
        return;
    }

    const auto group = Fooyin::Style::colourGroup(option);

    auto colour = Fooyin::resolveBrush(border.colour, option.palette, group).color();
    colour.setAlphaF(colour.alphaF() * static_cast<float>(border.opacity) / 100.0F);

    const double inset = border.width / 2.0;
    const QRectF rect  = QRectF{option.rect}.adjusted(inset, inset, -inset, -inset);
    if(rect.isEmpty()) {
        return;
    }

    QBrush brush{colour};
    if(border.shadingOpacity > 0) {
        const auto shade = [&](QPalette::ColorRole role) {
            const auto tint    = option.palette.color(group, role);
            const float amount = static_cast<float>(border.shadingOpacity) / 100.0F * tint.alphaF();
            return QColor::fromRgbF(std::lerp(colour.redF(), tint.redF(), amount),
                                    std::lerp(colour.greenF(), tint.greenF(), amount),
                                    std::lerp(colour.blueF(), tint.blueF(), amount), colour.alphaF());
        };

        QLinearGradient gradient{rect.topLeft(), rect.bottomRight()};
        gradient.setColorAt(0.0, shade(QPalette::Light));
        gradient.setColorAt(1.0, shade(QPalette::Shadow));
        brush = QBrush{gradient};
    }

    painter.setPen(QPen{brush, border.width});

    if(option.orientation == Qt::Horizontal) {
        painter.drawLine(rect.bottomLeft(), rect.bottomRight());
    }
    else {
        const double x = option.direction == Qt::RightToLeft ? rect.left() : rect.right();
        painter.drawLine(QPointF{x, rect.top()}, QPointF{x, rect.bottom()});
    }
}

QRect contentsRect(const QStyleOptionHeader& option, const Fooyin::HeaderAppearance& appearance)
{
    const int horizontal = contentPadding(appearance.horizontalPadding, appearance);
    const int vertical   = contentPadding(appearance.verticalPadding, appearance);
    return option.rect.adjusted(horizontal, vertical, -horizontal, -vertical);
}

QRect arrowRect(const QStyleOptionHeader& option, const Fooyin::HeaderAppearance& appearance)
{
    const QRect content = contentsRect(option, appearance);
    if(content.isEmpty() || option.sortIndicator == QStyleOptionHeader::None || appearance.arrow.extent == 0) {
        return {};
    }

    const int extent = std::min({appearance.arrow.extent, content.width(), content.height()});
    if(option.orientation == Qt::Horizontal) {
        return {content.right() - extent + 1, content.top() + ((content.height() - extent) / 2), extent, extent};
    }

    return {content.left() + ((content.width() - extent) / 2), content.bottom() - extent + 1, extent, extent};
}
} // namespace

namespace Fooyin::Style::Header {
void drawSection(const QStyleOptionHeader& option, QPainter& painter, const QWidget* widget,
                 const HeaderAppearance& appearance)
{
    if(option.rect.isEmpty()) {
        return;
    }

    updateDragTarget(option, painter, widget);

    const auto group  = colourGroup(option);
    const auto& state = sectionState(option, appearance);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setClipRect(option.rect, Qt::IntersectClip);

    if(state.backgroundOpacity > 0) {
        const double opacity = painter.opacity();
        painter.setOpacity(opacity * state.backgroundOpacity / 100.0);
        painter.fillRect(option.rect,
                         resolveBrush(state.backgroundColour, option.palette, group, option.rect, option.orientation,
                                      option.orientation == Qt::Horizontal && option.direction == Qt::RightToLeft));
        painter.setOpacity(opacity);
    }
    if(state.tintOpacity > 0) {
        const double opacity = painter.opacity();
        painter.setOpacity(opacity * state.tintOpacity / 100.0);
        painter.fillRect(option.rect, resolveBrush(state.tintColour, option.palette, group));
        painter.setOpacity(opacity);
    }

    drawEdge(option, painter, appearance);

    const auto& separator = appearance.separator;
    if(option.position != QStyleOptionHeader::OnlyOneSection && separator.width > 0 && separator.opacity > 0) {
        painter.save();
        painter.setOpacity(painter.opacity() * separator.opacity / 100.0);
        painter.setPen(QPen{resolveBrush(separator.colour, option.palette, group), separator.width});

        const double inset = separator.width / 2.0;
        const QRectF rect  = QRectF{option.rect}.adjusted(inset, inset, -inset, -inset);
        if(!rect.isEmpty()) {
            if(option.orientation == Qt::Horizontal && rect.height() > separator.inset * 2.0) {
                const double x = option.direction == Qt::RightToLeft ? rect.left() : rect.right();
                painter.drawLine(QPointF{x, rect.top() + separator.inset}, QPointF{x, rect.bottom() - separator.inset});
            }
            else if(option.orientation == Qt::Vertical && rect.width() > separator.inset * 2.0) {
                painter.drawLine(QPointF{rect.left() + separator.inset, rect.bottom()},
                                 QPointF{rect.right() - separator.inset, rect.bottom()});
            }
        }
        painter.restore();
    }

    const auto* extended  = qstyleoption_cast<const QStyleOptionHeaderV2*>(&option);
    const bool dragTarget = extended && extended->isSectionDragTarget;
    const auto& focus     = appearance.focus;
    if((option.state & QStyle::State_Enabled) && focus.width > 0
       && (dragTarget
           || ((option.state & QStyle::State_HasFocus)
               && (!focus.keyboardOnly || (option.state & QStyle::State_KeyboardFocusChange))))) {
        const double inset = std::max(focus.inset, focus.width / 2.0);
        const QRectF rect  = QRectF{option.rect}.adjusted(inset, inset, -inset, -inset);
        if(!rect.isEmpty()) {
            painter.setPen(QPen{resolveBrush(focus.colour, option.palette, group), focus.width});
            painter.setBrush(Qt::NoBrush);
            const double radius = std::min({focus.radius, rect.width() / 2.0, rect.height() / 2.0});
            painter.drawRoundedRect(rect, radius, radius);
        }
    }

    painter.restore();
}

void drawEmptyArea(const QStyleOption& option, QPainter& painter, const QWidget* widget,
                   const HeaderAppearance& appearance)
{
    if(option.rect.isEmpty()) {
        return;
    }
    // QHeaderView passes a plain style option for the space beyond its sections
    QStyleOptionHeader background;
    static_cast<QStyleOption&>(background) = option;
    background.orientation                 = option.state & QStyle::State_Horizontal ? Qt::Horizontal : Qt::Vertical;
    if(widget) {
        background.state.setFlag(QStyle::State_Active, widget->window()->isActiveWindow());
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(background.rect,
                     resolveBrush(appearance.emptyColour, background.palette, colourGroup(background), background.rect,
                                  background.orientation,
                                  background.orientation == Qt::Horizontal && background.direction == Qt::RightToLeft));
    drawEdge(background, painter, appearance);
    painter.restore();
}

void drawLabel(const QCommonStyle& style, const QStyleOptionHeader& option, QPainter& painter, const QWidget* widget,
               const HeaderAppearance& appearance)
{
    QStyleOptionHeaderV2 label;
    if(const auto* extended = qstyleoption_cast<const QStyleOptionHeaderV2*>(&option)) {
        label = *extended;
    }
    else {
        static_cast<QStyleOptionHeader&>(label) = option;
    }

    const auto& state = sectionState(option, appearance);
    const auto group  = colourGroup(option);
    label.palette.setCurrentColorGroup(group);
    label.palette.setBrush(group, QPalette::ButtonText, resolveBrush(state.foregroundColour, option.palette, group));
    if(state.tintIcon && !label.icon.isNull()) {
        label.icon = tintedIcon(label.icon, QSize{appearance.iconSize, appearance.iconSize},
                                painter.device()->devicePixelRatioF(),
                                option.state & QStyle::State_Enabled ? QIcon::Normal : QIcon::Disabled, QIcon::Off,
                                resolveBrush(state.iconColour, option.palette, group).color());
    }

    painter.save();
    style.QCommonStyle::drawControl(QStyle::CE_HeaderLabel, &label, &painter, widget);
    painter.restore();
}

void drawSortIndicator(const QStyleOptionHeader& option, QPainter& painter, const HeaderAppearance& appearance)
{
    const auto& arrow = appearance.arrow;
    if(option.sortIndicator == QStyleOptionHeader::None || arrow.size <= 0 || arrow.width <= 0
       || option.rect.isEmpty()) {
        return;
    }

    const double size = std::max(0.0, std::min({arrow.size, option.rect.width() - arrow.width,
                                                (option.rect.height() * 2.0) - (arrow.width * 2.0)}));
    if(size <= 0) {
        return;
    }

    const QPointF centre   = QRectF{option.rect}.center();
    const double direction = option.sortIndicator == QStyleOptionHeader::SortUp ? -1.0 : 1.0;
    const std::array points{centre + QPointF{-size / 2.0, -size / 4.0 * direction},
                            centre + QPointF{0.0, size / 4.0 * direction},
                            centre + QPointF{size / 2.0, -size / 4.0 * direction}};

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen{resolveBrush(arrow.colour, option.palette, colourGroup(option)), arrow.width, Qt::SolidLine,
                        Qt::RoundCap, Qt::RoundJoin});
    painter.drawPolyline(points.data(), static_cast<int>(points.size()));
    painter.restore();
}

std::optional<int> pixelMetric(QStyle::PixelMetric metric, const QStyleOption* option,
                               const HeaderAppearance& appearance)
{
    switch(metric) {
        case QStyle::PM_HeaderMargin:
            return appearance.spacing;
        case QStyle::PM_HeaderMarkSize:
            return appearance.arrow.extent;
        case QStyle::PM_SmallIconSize:
            if(qstyleoption_cast<const QStyleOptionHeader*>(option)) {
                return appearance.iconSize;
            }
            return {};
        default:
            return {};
    }
}

QSize sizeFromContents(const QStyleOptionHeader& option, const QSize& contentsSize, const HeaderAppearance& appearance)
{
    const QSize text     = option.fontMetrics.size(0, option.text);
    const int icon       = option.icon.isNull() ? 0 : appearance.iconSize;
    const int horizontal = contentPadding(appearance.horizontalPadding, appearance);
    const int vertical   = contentPadding(appearance.verticalPadding, appearance);

    QSize size{text.width() + icon + (icon && !option.text.isEmpty() ? appearance.spacing : 0) + (horizontal * 2),
               std::max(text.height(), icon) + (vertical * 2)};
    if(option.sortIndicator != QStyleOptionHeader::None && appearance.arrow.extent > 0) {
        if(option.orientation == Qt::Horizontal) {
            size.rwidth() += appearance.arrow.extent + appearance.spacing;
            size.setHeight(std::max(size.height(), appearance.arrow.extent + (vertical * 2)));
        }
        else {
            size.rheight() += appearance.arrow.extent + appearance.spacing;
            size.setWidth(std::max(size.width(), appearance.arrow.extent + (horizontal * 2)));
        }
    }
    if(option.orientation == Qt::Horizontal) {
        size.setHeight(std::max(size.height(), appearance.minimumThickness));
    }
    else {
        size.setWidth(std::max(size.width(), appearance.minimumThickness));
    }
    return size.expandedTo(contentsSize);
}

QRect subElementRect(const QStyleOptionHeader& option, QStyle::SubElement element, const HeaderAppearance& appearance)
{
    QRect rect = arrowRect(option, appearance);
    if(element == QStyle::SE_HeaderLabel) {
        const QRect arrow{rect};
        rect = contentsRect(option, appearance);
        if(!arrow.isEmpty()) {
            if(option.orientation == Qt::Horizontal) {
                rect.setRight(arrow.left() - appearance.spacing - 1);
            }
            else {
                rect.setBottom(arrow.top() - appearance.spacing - 1);
            }
        }
    }
    return rect.isEmpty() ? QRect{} : QStyle::visualRect(option.direction, option.rect, rect);
}
void unpolish(QWidget* widget)
{
    if(const auto* header = qobject_cast<QHeaderView*>(widget)) {
        header->viewport()->setProperty(DragTargetProperty, {});
    }
}
} // namespace Fooyin::Style::Header
