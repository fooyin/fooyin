/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include "playlistdelegate.h"

#include "playlistitem.h"

#include <utils/utils.h>

#include <QApplication>
#include <QPainter>

namespace Fooyin {
struct DrawTextResult
{
    QRect bound;
    int totalWidth{0};
};

template <typename Range>
DrawTextResult drawTextBlocks(QPainter* painter, const QStyleOptionViewItem& option, QRect rect, const Range& blocks,
                              Qt::Alignment alignment)
{
    DrawTextResult result;

    QStyle* style     = option.widget ? option.widget->style() : QApplication::style();
    const auto colour = option.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::NoRole;

    for(const auto& block : blocks) {
        painter->setFont(block.format.font);
        painter->setPen(block.format.colour);

        result.bound = painter->boundingRect(rect, alignment | Qt::TextWrapAnywhere, block.text);
        style->drawItemText(painter, rect, alignment, option.palette, true,
                            painter->fontMetrics().elidedText(block.text, Qt::ElideRight, rect.width()), colour);

        if(alignment & Qt::AlignRight) {
            rect.moveRight((rect.x() + rect.width()) - result.bound.width());
        }
        else {
            rect.setWidth(rect.width() - result.bound.width());
            rect.moveLeft(rect.x() + result.bound.width());
        }

        result.totalWidth += result.bound.width();
    }

    return result;
}

void paintHeader(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    QStyleOptionViewItem opt{option};
    opt.text.clear();
    opt.icon = {};

    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();

    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    QPen linePen = painter->pen();
    linePen.setWidth(1);
    QColor lineColour = opt.palette.color(QPalette::Text);
    lineColour.setAlpha(40);
    linePen.setColor(lineColour);

    const auto title    = index.data(PlaylistItem::Role::Title).value<RichText>();
    const auto subtitle = index.data(PlaylistItem::Role::Subtitle).value<RichText>();
    const auto side     = index.data(PlaylistItem::Role::Right).value<RichText>();
    const auto info     = index.data(PlaylistItem::Role::Info).value<RichText>();
    const auto cover    = index.data(Qt::DecorationRole).value<QPixmap>();

    const QRect& rect           = opt.rect;
    const int halfWidth         = rect.width() / 2;
    static constexpr int offset = 5;

    static constexpr int coverMargin     = 10;
    const int coverSize                  = rect.height() - (2 * coverMargin);
    static constexpr int coverFrameWidth = 2;
    const int coverFrameOffset           = coverFrameWidth / 2;

    const QRect coverRect{rect.left() + coverMargin, rect.top() + coverMargin, coverSize, coverSize};
    QRect coverFrameRect = coverRect.adjusted(-coverFrameOffset, -coverFrameOffset, coverFrameWidth, coverFrameWidth);

    if(cover.isNull()) {
        coverFrameRect.setWidth(0);
    }

    const auto titleOffset = static_cast<int>(rect.height() * 0.08);
    const auto infoOffset  = static_cast<int>(rect.height() * 0.12);

    const QRect rightRect{rect.left() + halfWidth, rect.top(), halfWidth - offset, rect.height()};
    const auto [rightBound, totalRightWidth]
        = drawTextBlocks(painter, opt, rightRect, side | std::views::reverse, Qt::AlignVCenter | Qt::AlignRight);

    const int leftWidth = rect.width() - coverFrameRect.width() - totalRightWidth;

    QRect subtitleRect{coverFrameRect.right() + (2 * offset), rect.top(), leftWidth, rect.height()};
    if(totalRightWidth > 0) {
        subtitleRect.setWidth(subtitleRect.width() - (5 * offset));
    }
    const auto [subtitleBound, _]
        = drawTextBlocks(painter, opt, subtitleRect, subtitle, Qt::AlignVCenter | Qt::AlignLeft);

    const QRect titleRect{coverFrameRect.right() + 2 * offset, rect.top() + titleOffset, leftWidth, rect.height()};
    drawTextBlocks(painter, opt, titleRect, title, Qt::AlignTop);

    const QRect infoRect{coverFrameRect.right() + 2 * offset, rect.top() - infoOffset, leftWidth, rect.height()};
    drawTextBlocks(painter, opt, infoRect, info, Qt::AlignBottom);

    const QLineF headerLine(coverFrameRect.right() + 2 * offset, coverFrameRect.bottom() + coverFrameWidth,
                            rect.right() - offset, coverFrameRect.bottom() + coverFrameWidth);

    painter->setPen(linePen);
    if(!subtitle.empty() && !side.empty() && rect.width() > 160) {
        static constexpr int lineOffset = 10;

        const QLineF rightLine(subtitleBound.right() + lineOffset, subtitleBound.center().y() + 1,
                               rightBound.left() - lineOffset, rightBound.center().y() + 1);
        painter->drawLine(rightLine);
    }

    painter->drawLine(headerLine);

    if(!cover.isNull()) {
        QPen coverPen     = painter->pen();
        QColor coverColor = opt.palette.color(QPalette::Shadow);
        coverColor.setAlpha(65);
        coverPen.setColor(coverColor);
        coverPen.setWidth(coverFrameWidth);

        painter->setRenderHint(QPainter::Antialiasing);
        painter->drawRect(coverFrameRect);

        const int width = coverRect.width();
        painter->drawPixmap(coverRect, Utils::scalePixmap(cover, width, opt.widget->devicePixelRatioF(), true));
    }
}

void paintSimpleHeader(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    QStyleOptionViewItem opt{option};
    opt.text.clear();
    opt.icon = {};

    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();

    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    QPen linePen = painter->pen();
    linePen.setWidth(1);
    QColor lineColour = opt.palette.color(QPalette::Text);
    lineColour.setAlpha(40);
    linePen.setColor(lineColour);

    const auto title    = index.data(PlaylistItem::Role::Title).value<RichText>();
    const auto subtitle = index.data(PlaylistItem::Role::Right).value<RichText>();

    const QRect& rect           = opt.rect;
    const int height            = rect.height();
    const int halfWidth         = rect.width() / 2;
    static constexpr int offset = 5;

    const QRect rightRect{rect.left() + halfWidth, rect.top(), halfWidth - offset, height};
    auto [rightBound, totalRightWidth]
        = drawTextBlocks(painter, opt, rightRect, subtitle | std::views::reverse, Qt::AlignVCenter | Qt::AlignRight);

    QRect leftRect{rect.left() + offset, rect.top(), rect.width() - totalRightWidth, height};
    if(totalRightWidth > 0) {
        leftRect.setWidth(leftRect.width() - (4 * offset));
    }
    auto [leftBound, _] = drawTextBlocks(painter, opt, leftRect, title, Qt::AlignVCenter | Qt::AlignLeft);

    if(!title.empty()) {
        if(subtitle.empty()) {
            rightBound = {rect.right() - offset, rect.top(), offset, height};
        }

        const int lineOffset = subtitle.empty() ? 5 : 10;

        const QLineF rightLine(leftBound.right() + lineOffset, leftBound.center().y() + 1,
                               rightBound.left() - lineOffset, rightBound.center().y() + 1);

        painter->setPen(lineColour);
        painter->drawLine(rightLine);
    }
}

void paintSubheader(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    QStyleOptionViewItem opt{option};

    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();

    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    QPen linePen = painter->pen();
    linePen.setWidth(1);
    QColor lineColour = opt.palette.color(QPalette::Text);
    lineColour.setAlpha(40);
    linePen.setColor(lineColour);

    const auto title    = index.data(PlaylistItem::Role::Title).value<RichText>();
    const auto subtitle = index.data(PlaylistItem::Role::Subtitle).value<RichText>();

    const QRect& rect           = opt.rect;
    const int height            = rect.height();
    const int halfWidth         = rect.width() / 2;
    static constexpr int offset = 5;

    const QRect rightRect{rect.left() + halfWidth, rect.top(), halfWidth - offset, height};
    auto [rightBound, totalRightWidth]
        = drawTextBlocks(painter, opt, rightRect, subtitle | std::views::reverse, Qt::AlignVCenter | Qt::AlignRight);

    QRect leftRect{rect.left() + offset, rect.top(), rect.width() - totalRightWidth, height};
    if(totalRightWidth > 0) {
        leftRect.setWidth(leftRect.width() - (4 * offset));
    }
    auto [leftBound, _] = drawTextBlocks(painter, opt, leftRect, title, Qt::AlignVCenter | Qt::AlignLeft);

    if(title.empty()) {
        leftBound = {rect.left(), rect.top(), 0, height};
    }

    if(subtitle.empty()) {
        rightBound = {rect.right(), rect.top(), 0, height};
    }

    const int leftOffset  = !title.empty() ? 10 : offset;
    const int rightOffset = !subtitle.empty() ? 10 : offset;

    painter->setPen(linePen);
    const QLineF titleLine(leftBound.right() + leftOffset, leftBound.center().y() + 1, rightBound.left() - rightOffset,
                           rightBound.center().y() + 1);
    painter->drawLine(titleLine);
}

void paintTrack(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    QStyleOptionViewItem opt{option};

    QStyle* style = option.widget ? option.widget->style() : QApplication::style();

    const bool singleColumn = index.data(PlaylistItem::Role::SingleColumnMode).toBool();

    const QRect textRect = opt.rect;
    const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &opt, opt.widget) * 2;

    QIcon::Mode mode{QIcon::Normal};
    if(!(opt.state & QStyle::State_Enabled)) {
        mode = QIcon::Disabled;
    }
    else if(opt.state & QStyle::State_Selected) {
        mode = QIcon::Selected;
    }
    opt.decorationAlignment = opt.displayAlignment;

    if(singleColumn) {
        const auto icon      = QIcon{index.data(Qt::DecorationRole).value<QPixmap>()};
        const int indent     = icon.isNull() ? 0 : opt.decorationSize.width() + textMargin;
        const auto leftSide  = index.data(PlaylistItem::Role::Left).value<RichText>();
        const auto rightSide = index.data(PlaylistItem::Role::Right).value<RichText>();

        const QRect rightRect     = textRect.adjusted(textRect.center().x() - textRect.left(), 0, -textMargin, 0);
        auto [_, totalRightWidth] = drawTextBlocks(painter, opt, rightRect, rightSide | std::views::reverse,
                                                   Qt::AlignVCenter | Qt::AlignRight);

        const QRect leftRect = textRect.adjusted(indent + textMargin, 0, -totalRightWidth, 0);
        drawTextBlocks(painter, opt, leftRect, leftSide, Qt::AlignVCenter | Qt::AlignLeft);

        if(!icon.isNull()) {
            opt.rect.setX(opt.rect.x() + textMargin);
            opt.icon.paint(painter, opt.rect, Qt::AlignVCenter | Qt::AlignLeft, mode, QIcon::On);
        }
    }
    else {
        if(index.data(PlaylistItem::Role::Column).canConvert<QPixmap>()) {
            const auto image = index.data(PlaylistItem::Role::Column).value<QPixmap>();

            if(!image.isNull()) {
                const auto imagePadding    = index.data(PlaylistItem::Role::ImagePadding).toInt();
                const auto imagePaddingTop = index.data(PlaylistItem::Role::ImagePaddingTop).toInt();

                opt.rect.adjust(imagePadding, imagePaddingTop, -imagePadding, imagePaddingTop);

                style->drawItemPixmap(
                    painter, opt.rect, Qt::AlignHCenter | Qt::AlignTop,
                    Utils::scalePixmap(image, opt.rect.width(), opt.widget->devicePixelRatioF(), true));
            }
        }
        else {
            const auto columnText = index.data(PlaylistItem::Role::Column).value<RichText>();

            const QRect columnRect = textRect.adjusted(textMargin, 0, -textMargin, 0);
            drawTextBlocks(painter, opt, columnRect, columnText, Qt::AlignVCenter | opt.displayAlignment);

            const auto icon = QIcon{index.data(Qt::DecorationRole).value<QPixmap>()};
            if(!icon.isNull()) {
                opt.icon.paint(painter, opt.rect, opt.decorationAlignment, mode, QIcon::On);
            }
        }
    }
}

void PlaylistDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    painter->save();

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    const auto type = index.data(PlaylistItem::Type).toInt();
    switch(type) {
        case(PlaylistItem::Track):
            paintTrack(painter, opt, index);
            break;
        case(PlaylistItem::Header): {
            const auto simple = index.data(PlaylistItem::Simple).toBool();
            simple ? paintSimpleHeader(painter, opt, index) : paintHeader(painter, opt, index);
            break;
        }
        case(PlaylistItem::Subheader):
            paintSubheader(painter, opt, index);
            break;
        default:
            break;
    }

    painter->restore();
}

QSize PlaylistDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    // Prevents the current playing row being larger than the rest
    opt.icon           = {};
    opt.decorationSize = {};

    const QWidget* widget = opt.widget;
    const QStyle* style   = widget ? widget->style() : QApplication::style();

    QSize size = index.data(Qt::SizeHintRole).toSize();

    const int margin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &opt, opt.widget) * 5;
    const QSize hint = style->sizeFromContents(QStyle::CT_ItemViewItem, &opt, size, widget);

    if(size.width() <= 0) {
        size.setWidth(hint.width());
    }
    if(size.height() < 1) {
        size.setHeight(hint.height());
    }

    size.setWidth(size.width() + margin);

    return size;
}
} // namespace Fooyin

#include "moc_playlistdelegate.cpp"
