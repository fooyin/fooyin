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

namespace Fooyin::Style::Toolbar {
void drawPanel(const QStyleOptionToolBar& option, QPainter& painter)
{
    if(option.rect.isEmpty()) {
        return;
    }

    painter.save();
    painter.fillRect(option.rect, option.palette.brush(colourGroup(option), QPalette::Window));

    if(option.lineWidth > 0) {
        auto border = outlineColour(option);
        border.setAlphaF(border.alphaF() * 0.6F);

        const QRect rect{option.rect};

        painter.setPen(QPen{border, 1.0});
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

void drawFrame(const QStyleOption& option, QPainter& painter)
{
    if(option.rect.isEmpty()) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen{outlineColour(option), 1.0});
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF{option.rect}.adjusted(0.5, 0.5, -0.5, -0.5), 4.0, 4.0);
    painter.restore();
}

void drawHandle(const QStyleOption& option, QPainter& painter)
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

    auto colour = option.palette.color(colourGroup(option), QPalette::WindowText);
    colour.setAlphaF(colour.alphaF() * 0.45F);

    painter.save();
    painter.setClipRect(option.rect, Qt::IntersectClip);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(colour);
    for(const double column : {-1.5, 1.5}) {
        for(const double row : {-4.0, 0.0, 4.0}) {
            const QPointF offset = horizontal ? QPointF{column, row} : QPointF{row, column};
            painter.drawEllipse(centre + offset, 0.75, 0.75);
        }
    }
    painter.restore();
}

void drawSeparator(const QStyleOption& option, QPainter& painter)
{
    if(option.rect.isEmpty()) {
        return;
    }

    const QRectF rect{option.rect};
    const bool horizontal = option.state & QStyle::State_Horizontal;
    const double length   = std::min(16.0, (horizontal ? rect.height() : rect.width()) - 8.0);
    if(length <= 0.0) {
        return;
    }

    auto colour = outlineColour(option);
    colour.setAlphaF(colour.alphaF() * 0.6F);
    const QPointF offset = horizontal ? QPointF{0.0, length / 2.0} : QPointF{length / 2.0, 0.0};

    painter.save();
    painter.setPen(QPen{colour, 1.0});
    painter.drawLine(rect.center() - offset, rect.center() + offset);
    painter.restore();
}

void drawExtension(const QStyleOptionToolButton& option, QPainter& painter, const QWidget* widget)
{
    if(option.rect.isEmpty()) {
        return;
    }

    const auto* toolbar   = widget ? qobject_cast<const QToolBar*>(widget->parentWidget()) : nullptr;
    const bool horizontal = !toolbar || toolbar->orientation() == Qt::Horizontal;
    const double extent   = std::min({12, option.rect.width(), option.rect.height()});

    painter.save();
    painter.setClipRect(option.rect, Qt::IntersectClip);
    painter.translate(QRectF{option.rect}.center());
    if(horizontal) {
        painter.rotate(option.direction == Qt::RightToLeft ? 90.0 : -90.0);
    }
    painter.scale(extent / 12.0, extent / 12.0);
    drawChevron(option, QRect{-6, -8, 12, 12}, painter, false, QPalette::ButtonText);
    drawChevron(option, QRect{-6, -4, 12, 12}, painter, false, QPalette::ButtonText);
    painter.restore();
}

std::optional<int> pixelMetric(QStyle::PixelMetric metric)
{
    switch(metric) {
        case QStyle::PM_ToolBarFrameWidth:
            return 1;
        case QStyle::PM_ToolBarItemMargin:
        case QStyle::PM_ToolBarItemSpacing:
            return 2;
        case QStyle::PM_ToolBarHandleExtent:
            return 10;
        case QStyle::PM_ToolBarSeparatorExtent:
            return 8;
        case QStyle::PM_ToolBarExtensionExtent:
            return 22;
        default:
            return {};
    }
}
} // namespace Fooyin::Style::Toolbar
