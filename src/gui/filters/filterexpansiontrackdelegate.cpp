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

#include "filterexpansiontrackdelegate.h"

#include "filterexpansionmodel.h"

#include <gui/widgets/widgetbackground.h>

#include <QIcon>
#include <QPainter>

namespace Fooyin {
void ExpansionTrackDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                   const QModelIndex& index) const
{
    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);
    opt.text.clear();
    WidgetBackground::clearDelegateBackground(opt);

    WidgetBackground::drawItemBackground(painter, opt);

    if(index.data(Filters::ExpansionTrackModel::SubheaderRole).toBool()
       || index.data(Filters::ExpansionTrackModel::HeaderRole).toBool()) {
        const auto layout    = index.data(Filters::ExpansionTrackModel::LayoutRole).value<RichTextLayout>();
        const bool subheader = index.data(Filters::ExpansionTrackModel::SubheaderRole).toBool();
        drawRichTextLayout(painter, opt, opt.rect.adjusted(4, subheader ? 6 : 0, -4, subheader ? -4 : -2), layout);
        return;
    }

    const auto layout = index.data(Filters::ExpansionTrackModel::LayoutRole).value<RichTextLayout>();
    const auto icon   = index.data(Filters::ExpansionTrackModel::PlayingIconRole).value<QIcon>();

    drawRichTextLayout(painter, opt, opt.rect.adjusted(4, 0, -4, 0), layout, icon);
}

QSize ExpansionTrackDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const auto size     = index.data(Qt::SizeHintRole).toSize();
    const int charWidth = option.fontMetrics.horizontalAdvance(u'M');

    auto hint = size.isValid() ? size : QSize{0, option.fontMetrics.height() + 5};
    hint.rwidth() += charWidth;
    return hint;
}
} // namespace Fooyin
