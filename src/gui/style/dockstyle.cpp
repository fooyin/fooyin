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
               const QWidget* widget)
{
    if(option.rect.isEmpty()) {
        return;
    }

    auto border = outlineColour(option);
    border.setAlphaF(border.alphaF() * 0.6F);

    painter.save();
    painter.setClipRect(option.rect, Qt::IntersectClip);
    painter.fillRect(option.rect, option.palette.brush(colourGroup(option), QPalette::Button));
    painter.setPen(border);

    if(option.verticalTitleBar) {
        painter.drawLine(option.rect.topRight(), option.rect.bottomRight());
    }
    else {
        painter.drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
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
        const auto title = option.fontMetrics.elidedText(option.title, Qt::ElideRight, text.width());
        style.drawItemText(&painter, text, Qt::AlignLeading | Qt::AlignVCenter | Qt::TextHideMnemonic, option.palette,
                           option.state & QStyle::State_Enabled, title, QPalette::WindowText);
    }

    painter.restore();
}

void drawFrame(const QStyleOption& option, QPainter& painter)
{
    if(option.rect.isEmpty()) {
        return;
    }

    auto border = outlineColour(option);
    border.setAlphaF(border.alphaF() * 0.6F);

    painter.save();
    painter.setPen(border);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(option.rect.adjusted(0, 0, -1, -1));
    painter.restore();
}

std::optional<int> pixelMetric(QStyle::PixelMetric metric)
{
    switch(metric) {
        case QStyle::PM_DockWidgetTitleMargin:
            return 4;
        case QStyle::PM_DockWidgetTitleBarButtonMargin:
            return 2;
        case QStyle::PM_DockWidgetFrameWidth:
            return 1;
        case QStyle::PM_DockWidgetSeparatorExtent:
            return 6;
        default:
            return {};
    }
}
} // namespace Fooyin::Style::Dock
