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

#include <utils/tooltipfilter.h>

#include <QAbstractItemView>
#include <QHelpEvent>
#include <QToolTip>

namespace Fooyin {
ToolTipFilter::ToolTipFilter(QObject* parent)
    : QObject{parent}
{ }

bool ToolTipFilter::eventFilter(QObject* obj, QEvent* event)
{
    if(event->type() != QEvent::ToolTip) {
        return QObject::eventFilter(obj, event);
    }

    auto* view = qobject_cast<QAbstractItemView*>(obj->parent());
    if(!view) {
        return false;
    }

    auto* helpEvent         = static_cast<QHelpEvent*>(event);
    const QPoint pos        = helpEvent->pos();
    const QModelIndex index = view->indexAt(pos);

    if(!index.isValid()) {
        return false;
    }

    const QString itemTooltip = view->model()->data(index, Qt::ToolTipRole).toString();
    const int textMargin      = view->style()->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, view) + 1;

    const QFontMetrics fm{view->font()};
    const int itemTextWidth = fm.horizontalAdvance(itemTooltip) + (2 * textMargin);
    const QRect rect        = view->visualRect(index);
    const int rectWidth     = rect.width();

    if(!itemTooltip.isEmpty() && itemTextWidth > rectWidth) {
        QToolTip::showText(helpEvent->globalPos(), itemTooltip, view, rect);
    }
    else {
        QToolTip::hideText();
    }

    return true;
}
} // namespace Fooyin
