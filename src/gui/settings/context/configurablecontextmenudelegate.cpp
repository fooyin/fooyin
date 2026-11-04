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

#include "configurablecontextmenudelegate.h"

#include "configurablecontextmenumodel.h"

#include <gui/guiutils.h>

#include <QApplication>
#include <QPainter>
#include <QSize>
#include <QStyle>

namespace Fooyin {
void ConfigurableContextMenuDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                            const QModelIndex& index) const
{
    const auto* model = qobject_cast<const ConfigurableContextMenuModel*>(index.model());
    if(!model || !model->isSeparator(index)) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);

    const QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

    static constexpr int horizontalMargin = 10;

    const QRect lineRect = opt.rect.adjusted(horizontalMargin, 0, -horizontalMargin, 0);
    const int y          = lineRect.center().y();

    QColor lineColour = (opt.state & QStyle::State_Selected) ? opt.palette.color(Gui::itemViewSelectionTextRole(opt))
                                                             : opt.palette.color(QPalette::Text);
    lineColour.setAlphaF(0.4);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);

    QPen pen{lineColour, 1};
    pen.setDashPattern({2.0, 4.0});
    pen.setCosmetic(true);
    painter->setPen(pen);

    painter->drawLine(lineRect.left(), y, lineRect.right(), y);
    painter->restore();
}

QSize ConfigurableContextMenuDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const auto* model = qobject_cast<const ConfigurableContextMenuModel*>(index.model());
    if(!model || !model->isSeparator(index)) {
        return QStyledItemDelegate::sizeHint(option, index);
    }

    QSize hint = QStyledItemDelegate::sizeHint(option, index);
    hint.setHeight(std::max(8, hint.height() / 2));
    return hint;
}
} // namespace Fooyin
