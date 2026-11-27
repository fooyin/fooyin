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

#include "dockstyle.h"

#include "styleutils.h"

#include <QPainter>
#include <QStyleOptionDockWidget>

namespace Fooyin::Style::Dock {
void drawTitle(const QCommonStyle& style, const QStyleOptionDockWidget& option, QPainter& painter,
               const QWidget* widget, const DockWidgetAppearance& appearance)
{
    if(option.rect.isEmpty()) {
        return;
    }

    const auto group   = colourGroup(option);
    const auto& title  = appearance.title;
    const auto& border = title.border;

    painter.save();
    painter.setClipRect(option.rect, Qt::IntersectClip);
    const double opacity = painter.opacity();
    painter.setOpacity(opacity * title.backgroundOpacity / 100.0);
    painter.fillRect(option.rect, resolveBrush(title.backgroundColour, option.palette, group, option.rect,
                                               option.verticalTitleBar ? Qt::Vertical : Qt::Horizontal,
                                               !option.verticalTitleBar && option.direction == Qt::RightToLeft));
    painter.setOpacity(opacity);

    if(border.width > 0) {
        const double inset = (border.width - 1) / 2.0;
        const QRectF rect  = QRectF{option.rect.adjusted(0, 0, -1, -1)}.adjusted(inset, inset, -inset, -inset);
        if(!rect.isEmpty()) {
            painter.setPen(
                QPen{tintedColour(option, border.colour, border.tintColour, border.tintOpacity, border.opacity),
                     static_cast<double>(border.width)});
            if(option.verticalTitleBar) {
                painter.drawLine(rect.topRight(), rect.bottomRight());
            }
            else {
                painter.drawLine(rect.bottomLeft(), rect.bottomRight());
            }
        }
    }

    QRect text = style.subElementRect(QStyle::SE_DockWidgetTitleBarText, &option, widget);
    if(option.verticalTitleBar) {
        painter.translate(text.left(), text.top() + text.height());
        painter.rotate(-90);
        text = QRect{0, 0, text.height(), text.width()};
    }

    const int margin = style.pixelMetric(QStyle::PM_DockWidgetTitleMargin, &option, widget);
    text.adjust(margin, 0, -margin, 0);
    if(!text.isEmpty()) {
        auto palette{option.palette};
        palette.setCurrentColorGroup(group);
        palette.setBrush(group, QPalette::WindowText, resolveBrush(title.foregroundColour, option.palette, group));

        const auto label = option.fontMetrics.elidedText(option.title, Qt::ElideRight, text.width());
        style.drawItemText(&painter, text, Qt::AlignLeading | Qt::AlignVCenter | Qt::TextHideMnemonic, palette,
                           option.state & QStyle::State_Enabled, label, QPalette::WindowText);
    }

    painter.restore();
}

void drawFrame(const QStyleOption& option, QPainter& painter, const DockWidgetAppearance& appearance)
{
    if(option.rect.isEmpty()) {
        return;
    }

    const auto& frame  = appearance.frame;
    const double inset = (frame.width - 1) / 2.0;
    const QRectF rect  = QRectF{option.rect.adjusted(0, 0, -1, -1)}.adjusted(inset, inset, -inset, -inset);
    if(frame.width <= 0 || rect.isEmpty()) {
        return;
    }

    painter.save();
    painter.setPen(QPen{tintedColour(option, frame.colour, frame.tintColour, frame.tintOpacity, frame.opacity),
                        static_cast<double>(frame.width)});
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect);
    painter.restore();
}

std::optional<int> pixelMetric(QStyle::PixelMetric metric, const DockWidgetAppearance& appearance)
{
    switch(metric) {
        case QStyle::PM_DockWidgetTitleMargin:
            return appearance.titleMargin;
        case QStyle::PM_DockWidgetTitleBarButtonMargin:
            return appearance.buttonMargin;
        case QStyle::PM_DockWidgetFrameWidth:
            return appearance.frame.width;
        case QStyle::PM_DockWidgetSeparatorExtent:
            return appearance.separatorExtent;
        default:
            return {};
    }
}
} // namespace Fooyin::Style::Dock
