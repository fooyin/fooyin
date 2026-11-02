/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include "infodelegate.h"

#include <gui/guiutils.h>

#include <QApplication>
#include <QPainter>

namespace {
void paintHeader(QPainter* painter, const QStyleOptionViewItem& opt, const QModelIndex& /*index*/)
{
    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();

    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    const QRect& rect    = opt.rect;
    constexpr int offset = 10;

    QPen linePen = painter->pen();
    linePen.setWidth(1);
    QColor lineColour = opt.palette.color(QPalette::Text);
    lineColour.setAlpha(40);
    linePen.setColor(lineColour);

    if(!opt.text.isEmpty()) {
        const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt);
        painter->setPen(linePen);
        const QRect titleBound = painter->boundingRect(textRect, static_cast<int>(opt.displayAlignment), opt.text);
        const QLineF headerLine(titleBound.right() + (2 * offset), titleBound.center().y() + 1, rect.right() - offset,
                                titleBound.center().y() + 1);
        painter->drawLine(headerLine);
    }
}

void paintEntry(QPainter* painter, QStyleOptionViewItem opt, const QModelIndex& /*index*/)
{
    const QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    const QString text  = std::exchange(opt.text, {});

    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    const QRect textRect = Fooyin::Gui::itemViewTextRect(opt);
    const auto textRole
        = opt.state.testFlag(QStyle::State_Selected) ? Fooyin::Gui::itemViewSelectionTextRole(opt) : QPalette::Text;
    const bool enabled = opt.state.testFlag(QStyle::State_Enabled);

    opt.palette.setCurrentColorGroup(!enabled                                   ? QPalette::Disabled
                                     : opt.state.testFlag(QStyle::State_Active) ? QPalette::Active
                                                                                : QPalette::Inactive);

    painter->save();
    painter->setClipRect(opt.rect, Qt::IntersectClip);
    painter->setFont(opt.font);
    style->drawItemText(painter, textRect,
                        QStyle::visualAlignment(opt.direction, opt.displayAlignment) | Qt::TextSingleLine, opt.palette,
                        enabled, opt.fontMetrics.elidedText(text, opt.textElideMode, textRect.width()), textRole);
    painter->restore();
}
} // namespace

namespace Fooyin {
void ItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    if(index.flags() & Qt::ItemNeverHasChildren) {
        paintEntry(painter, opt, index);
    }
    else {
        paintHeader(painter, opt, index);
    }
}

QSize ItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if(index.column() == 1) {
        return sizeHint(option, index.siblingAtColumn(0));
    }

    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);

    const QWidget* widget = opt.widget;
    const QStyle* style   = widget ? widget->style() : QApplication::style();

    const QSize size = index.data(Qt::SizeHintRole).toSize();

    return style->sizeFromContents(QStyle::CT_ItemViewItem, &opt, size, widget);
}
} // namespace Fooyin

#include "moc_infodelegate.cpp"
