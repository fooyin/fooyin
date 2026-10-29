/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include "playlistorganiserdelegate.h"

#include "scripting/richtextdelegateutils.h"

#include <gui/guiutils.h>
#include <gui/scripting/richtext.h>

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QApplication>
#include <QPainter>

constexpr auto RightContentPadding = 5;
constexpr auto MinLeftContentWidth = 80;

namespace Fooyin {
void PlaylistOrganiserDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                      const QModelIndex& index) const
{
    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);
    opt.decorationSize = option.decorationSize;

    QStyle* style = option.widget ? option.widget->style() : QApplication::style();
    opt.text.clear();
    opt.showDecorationSelected = true;

    const auto leftRichText  = index.data(PlaylistOrganiserItem::RichTitle).value<RichText>();
    const auto rightRichText = index.data(PlaylistOrganiserItem::RightRichTitle).value<RichText>();

    painter->save();

    if(opt.backgroundBrush.style() != Qt::NoBrush) {
        painter->fillRect(option.rect, opt.backgroundBrush);
        opt.backgroundBrush = Qt::NoBrush;
    }
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, option.widget);

    const QRect textRect = Gui::itemViewTextRect(opt);

    const QSize rightSize = Utils::richTextNaturalSize(opt, rightRichText);

    int rightWidth{0};
    if(rightSize.width() > 0) {
        rightWidth = std::min(rightSize.width() + RightContentPadding, textRect.width());
        if(textRect.width() - rightWidth - RightContentPadding < MinLeftContentWidth) {
            rightWidth = 0;
        }
    }

    QRect leftRect{textRect};
    QRect rightRect{textRect};

    if(rightWidth > 0) {
        rightRect.setLeft(textRect.right() - rightWidth + 1);
        leftRect.setRight(rightRect.left() - RightContentPadding);
    }

    const auto leftLines  = Utils::prepareTextLines(opt, leftRect.width(), leftRichText);
    const auto rightLines = Utils::prepareTextLines(opt, rightRect.width(), rightRichText);

    Utils::drawPreparedTextLines(painter, leftRect, leftLines, Qt::AlignLeft);
    if(rightWidth > 0) {
        Utils::drawPreparedTextLines(painter, rightRect, rightLines, Qt::AlignRight);
    }

    painter->restore();
}

QSize PlaylistOrganiserDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);
    opt.decorationSize = option.decorationSize;
    opt.text.clear();

    const QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    const int textGap   = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, opt.widget) * 2;

    const QSize leftSize
        = Utils::richTextNaturalSize(opt, index.data(PlaylistOrganiserItem::RichTitle).value<RichText>());
    const QSize rightSize
        = Utils::richTextNaturalSize(opt, index.data(PlaylistOrganiserItem::RightRichTitle).value<RichText>());
    const QSize contentSize{leftSize.width() + rightSize.width()
                                + (rightSize.width() > 0 ? textGap + RightContentPadding : 0),
                            std::max(leftSize.height(), rightSize.height())};

    return Gui::itemViewTextSizeHint(opt, contentSize);
}
} // namespace Fooyin
