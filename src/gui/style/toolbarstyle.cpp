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

#include "toolbarstyle.h"

#include "styleutils.h"

#include <QPainter>
#include <QStyleOptionToolBar>
#include <QStyleOptionToolButton>
#include <QToolBar>

#include <array>
#include <cmath>

namespace Fooyin::Style::Toolbar {
namespace {
QColor tintedColour(const QStyleOption& option, const ColourSource& source, const ColourSource& tintSource,
                    int tintOpacity, int opacity)
{
    const auto group  = colourGroup(option);
    const auto base   = resolveBrush(source, option.palette, group).color();
    const auto tint   = resolveBrush(tintSource, option.palette, group).color();
    const auto amount = static_cast<float>(tintOpacity / 100.0) * tint.alphaF();
    return QColor::fromRgbF(
        std::lerp(base.redF(), tint.redF(), amount), std::lerp(base.greenF(), tint.greenF(), amount),
        std::lerp(base.blueF(), tint.blueF(), amount), base.alphaF() * static_cast<float>(opacity / 100.0));
}
} // namespace

void drawPanel(const QStyleOptionToolBar& option, QPainter& painter, const ToolBarAppearance& appearance)
{
    if(option.rect.isEmpty()) {
        return;
    }

    painter.save();
    const double opacity   = painter.opacity();
    const auto orientation = option.state & QStyle::State_Horizontal ? Qt::Horizontal : Qt::Vertical;
    painter.setOpacity(opacity * appearance.backgroundOpacity / 100.0);
    painter.fillRect(option.rect,
                     resolveBrush(appearance.backgroundColour, option.palette, colourGroup(option), option.rect,
                                  orientation, orientation == Qt::Horizontal && option.direction == Qt::RightToLeft));
    painter.setOpacity(opacity);

    if(option.lineWidth > 0 && appearance.border.width > 0) {
        const auto& border = appearance.border;
        const auto colour  = tintedColour(option, border.colour, border.tintColour, border.tintOpacity, border.opacity);

        const double inset = (border.width - 1) / 2.0;
        const QRectF rect  = QRectF{option.rect.adjusted(0, 0, -1, -1)}.adjusted(inset, inset, -inset, -inset);
        if(rect.isEmpty()) {
            painter.restore();
            return;
        }

        painter.setPen(QPen{colour, static_cast<double>(border.width)});
        switch(option.toolBarArea) {
            case Qt::BottomToolBarArea:
                painter.drawLine(rect.topLeft(), rect.topRight());
                break;
            case Qt::LeftToolBarArea:
                painter.drawLine(rect.topRight(), rect.bottomRight());
                break;
            case Qt::RightToolBarArea:
                painter.drawLine(rect.topLeft(), rect.bottomLeft());
                break;
            default:
                if(option.state & QStyle::State_Horizontal) {
                    painter.drawLine(rect.bottomLeft(), rect.bottomRight());
                }
                else {
                    painter.drawLine(rect.topRight(), rect.bottomRight());
                }
                break;
        }
    }

    painter.restore();
}

void drawFrame(const QStyleOption& option, QPainter& painter, const ToolBarAppearance& appearance)
{
    if(option.rect.isEmpty()) {
        return;
    }

    const auto& border = appearance.border;
    const double inset = border.width / 2.0;
    const QRectF rect  = QRectF{option.rect}.adjusted(inset, inset, -inset, -inset);
    if(border.width <= 0 || rect.isEmpty()) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(
        QPen{tintedColour(option, border.colour, border.tintColour, border.tintOpacity, border.floatingOpacity),
             static_cast<double>(border.width)});
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(rect, border.floatingRadius, border.floatingRadius);
    painter.restore();
}

void drawHandle(const QStyleOption& option, QPainter& painter, const ToolBarAppearance& appearance)
{
    if(option.rect.isEmpty()) {
        return;
    }

    if(const auto* toolbar = qstyleoption_cast<const QStyleOptionToolBar*>(&option);
       toolbar && !(toolbar->features & QStyleOptionToolBar::Movable)) {
        return;
    }

    const auto centre     = QRectF{option.rect}.center();
    const bool horizontal = option.state & QStyle::State_Horizontal;

    const auto& handle = appearance.handle;
    if(handle.dotRadius <= 0) {
        return;
    }
    auto colour = resolveBrush(handle.colour, option.palette, colourGroup(option)).color();
    colour.setAlphaF(colour.alphaF() * static_cast<float>(handle.opacity / 100.0));

    painter.save();
    painter.setClipRect(option.rect, Qt::IntersectClip);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(colour);
    for(const double column : {-handle.columnSpacing / 2.0, handle.columnSpacing / 2.0}) {
        for(const double row : {-handle.rowSpacing, 0.0, handle.rowSpacing}) {
            const QPointF offset = horizontal ? QPointF{column, row} : QPointF{row, column};
            painter.drawEllipse(centre + offset, handle.dotRadius, handle.dotRadius);
        }
    }
    painter.restore();
}

void drawSeparator(const QStyleOption& option, QPainter& painter, const ToolBarAppearance& appearance)
{
    if(option.rect.isEmpty()) {
        return;
    }

    const QRectF rect{option.rect};
    const bool horizontal = option.state & QStyle::State_Horizontal;
    const auto& separator = appearance.separator;
    const double length
        = std::min(separator.length, (horizontal ? rect.height() : rect.width()) - 2 * separator.margin);
    if(length <= 0.0 || separator.width <= 0.0) {
        return;
    }

    const auto colour
        = tintedColour(option, separator.colour, separator.tintColour, separator.tintOpacity, separator.opacity);
    const QPointF offset = horizontal ? QPointF{0.0, length / 2.0} : QPointF{length / 2.0, 0.0};

    painter.save();
    painter.setPen(QPen{colour, separator.width});
    painter.drawLine(rect.center() - offset, rect.center() + offset);
    painter.restore();
}

void drawExtension(const QStyleOptionToolButton& option, QPainter& painter, const QWidget* widget,
                   const ToolBarAppearance& appearance)
{
    if(option.rect.isEmpty()) {
        return;
    }

    const auto* toolbar   = widget ? qobject_cast<const QToolBar*>(widget->parentWidget()) : nullptr;
    const bool horizontal = !toolbar || toolbar->orientation() == Qt::Horizontal;
    const auto& extension = appearance.extension;
    const double extent   = std::min(
        {extension.glyphSize, static_cast<double>(option.rect.width()), static_cast<double>(option.rect.height())});
    if(extent <= 0.0 || extension.glyphWidth <= 0.0) {
        return;
    }

    painter.save();
    painter.setClipRect(option.rect, Qt::IntersectClip);
    painter.translate(QRectF{option.rect}.center());
    if(horizontal) {
        painter.rotate(option.direction == Qt::RightToLeft ? 90.0 : -90.0);
    }
    painter.scale(extent / 12.0, extent / 12.0);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen{resolveBrush(extension.colour, option.palette, colourGroup(option)), extension.glyphWidth,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
    for(const double offset : {-2.0, 2.0}) {
        const std::array points{QPointF{-3.0, offset - 1.5}, QPointF{0.0, offset + 1.5}, QPointF{3.0, offset - 1.5}};
        painter.drawPolyline(points.data(), static_cast<int>(points.size()));
    }
    painter.restore();
}

std::optional<int> pixelMetric(QStyle::PixelMetric metric, const ToolBarAppearance& appearance)
{
    switch(metric) {
        case QStyle::PM_ToolBarFrameWidth:
            return appearance.border.width;
        case QStyle::PM_ToolBarItemMargin:
            return appearance.itemMargin;
        case QStyle::PM_ToolBarItemSpacing:
            return appearance.itemSpacing;
        case QStyle::PM_ToolBarHandleExtent:
            return appearance.handle.extent;
        case QStyle::PM_ToolBarSeparatorExtent:
            return appearance.separator.extent;
        case QStyle::PM_ToolBarExtensionExtent:
            return appearance.extension.extent;
        default:
            return {};
    }
}
} // namespace Fooyin::Style::Toolbar
