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

#include "queueviewerdelegate.h"

#include "queuevieweritem.h"
#include "scripting/richtextdelegateutils.h"

#include <gui/guiutils.h>
#include <gui/iconloader.h>
#include <gui/scripting/richtext.h>

#include <QApplication>
#include <QPainter>

constexpr auto TextElideMargin = 1;

namespace Fooyin {
void QueueViewerDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);
    opt.decorationSize         = option.decorationSize;
    opt.showDecorationSelected = true;
    opt.text.clear();

    const QStyle* style  = opt.widget ? opt.widget->style() : QApplication::style();
    const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &opt, opt.widget) + 1;
    const QRect textRect = Gui::itemViewTextRect(opt);

    const auto leftRichText  = index.data(QueueViewerItem::RichTitle).value<RichText>();
    const auto rightRichText = index.data(QueueViewerItem::RightRichTitle).value<RichText>();
    const QSize rightSize    = Utils::richTextNaturalSize(opt, rightRichText);

    const int rightWidth = std::min(rightSize.width() + TextElideMargin, textRect.width());
    int leftWidth        = textRect.width();
    if(rightWidth > 0) {
        leftWidth = std::max(0, textRect.width() - rightWidth - textMargin);
    }

    QRect leftRect{textRect};
    leftRect.setWidth(leftWidth);

    QRect rightRect{textRect};
    rightRect.setLeft(textRect.right() - rightWidth + 1);

    const auto leftLines  = Utils::prepareTextLines(opt, leftRect.width(), leftRichText);
    const auto rightLines = Utils::prepareTextLines(opt, rightRect.width(), rightRichText);

    const bool isPlaybackIcon = index.data(QueueViewerItem::IsPlaybackIcon).toBool();
    const bool hasArtwork     = !isPlaybackIcon && !opt.icon.isNull();
    const QIcon itemIcon      = (isPlaybackIcon || hasArtwork) ? opt.icon : QIcon{};
    const QRect itemIconRect
        = !itemIcon.isNull() ? style->subElementRect(QStyle::SE_ItemViewItemDecoration, &opt, opt.widget) : QRect{};
    if(isPlaybackIcon || hasArtwork) {
        opt.icon = {};
    }

    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, option.widget);

    if(isPlaybackIcon) {
        Gui::drawItemViewIcon(painter, opt, itemIcon, itemIconRect, opt.decorationAlignment);
    }
    else if(hasArtwork) {
        const double dpr = opt.widget ? opt.widget->devicePixelRatioF() : 1.0;
        Gui::drawRoundedPixmap(*painter, itemIconRect, opt.decorationAlignment,
                               itemIcon.pixmap(itemIconRect.size(), dpr, Gui::itemViewIconMode(opt)),
                               m_artworkCornerRadius);
    }

    Utils::drawPreparedTextLines(painter, leftRect, leftLines, Qt::AlignLeft);
    Utils::drawPreparedTextLines(painter, rightRect, rightLines, Qt::AlignRight);
}

QSize QueueViewerDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);

    opt.decorationSize = option.decorationSize;

    const QStyle* style  = opt.widget ? opt.widget->style() : QApplication::style();
    const int textGap    = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, opt.widget) * 2;
    const QSize leftSize = Utils::richTextNaturalSize(opt, index.data(QueueViewerItem::RichTitle).value<RichText>());
    const QSize rightSize
        = Utils::richTextNaturalSize(opt, index.data(QueueViewerItem::RightRichTitle).value<RichText>());
    const QSize textSize{leftSize.width() + rightSize.width() + (rightSize.width() > 0 ? textGap + TextElideMargin : 0),
                         std::max(leftSize.height(), rightSize.height())};

    return Gui::itemViewTextSizeHint(opt, textSize);
}

void QueueViewerDelegate::setArtworkCornerRadius(int radius)
{
    m_artworkCornerRadius = std::clamp(radius, 0, 100);
}
} // namespace Fooyin
