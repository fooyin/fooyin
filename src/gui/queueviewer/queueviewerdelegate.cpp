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

    QStyle* style        = opt.widget ? opt.widget->style() : QApplication::style();
    const auto colour    = option.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::NoRole;
    const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, opt.widget) * 2;
    QRect textRect       = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);
    textRect.adjust(textMargin, 0, -textMargin, 0);

    const QString right   = index.data(QueueViewerItem::RightText).toString();
    const auto rightAlign = static_cast<int>(Qt::AlignVCenter | Qt::AlignRight);

    const auto rightBounds = painter->boundingRect(textRect, rightAlign | Qt::TextWrapAnywhere, right);

    QRect leftRect{textRect};
    leftRect.setRight(rightBounds.x() - textMargin);
    opt.text = Utils::elideTextWithBreaks(opt.text, painter->fontMetrics(), leftRect.width(), Qt::ElideRight);

    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, option.widget);

    const QString rightText
        = Utils::elideTextWithBreaks(right, painter->fontMetrics(), textRect.width(), Qt::ElideRight);
    style->drawItemText(painter, textRect, rightAlign, opt.palette, true, rightText, colour);
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
