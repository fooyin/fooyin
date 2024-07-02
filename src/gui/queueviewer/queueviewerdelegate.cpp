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

#include "queueviewerdelegate.h"

#include "queuevieweritem.h"

#include <utils/utils.h>

#include <QApplication>
#include <QPainter>

namespace Fooyin {
void QueueViewerDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);

    opt.decorationSize = option.decorationSize;

    const QString rightText = index.data(QueueViewerItem::RightText).toString();

    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    QRect textRect       = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);
    const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, opt.widget) * 2;
    textRect.adjust(textMargin, 0, -textMargin, 0);

    const auto rightBounds = painter->boundingRect(textRect, Qt::AlignRight | Qt::TextWrapAnywhere, rightText);

    const auto colour = option.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::NoRole;
    style->drawItemText(painter, textRect, Qt::AlignRight, opt.palette, true,
                        painter->fontMetrics().elidedText(rightText, Qt::ElideRight, textRect.width()), colour);

    opt.rect.setRight(rightBounds.x() - textMargin);

    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, option.widget);
}

QSize QueueViewerDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);

    opt.decorationSize = option.decorationSize;

    const QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    QSize size          = style->sizeFromContents(QStyle::CT_ItemViewItem, &opt, {}, opt.widget);

    return size;
}
} // namespace Fooyin
