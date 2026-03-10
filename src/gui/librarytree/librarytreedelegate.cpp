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

#include "librarytreedelegate.h"

#include "librarytreeitem.h"

#include <gui/scripting/richtext.h>

#include <QApplication>
#include <QPainter>

namespace Fooyin {
namespace {
struct DrawTextResult
{
    QRect bound;
    int totalWidth{0};
    int maxHeight{0};
};

RichText fallbackRichText(const QStyleOptionViewItem& option, const QModelIndex& index)
{
    const auto richText = index.data(LibraryTreeItem::RichTitle).value<RichText>();
    if(!richText.empty()) {
        return richText;
    }

    RichText fallback;
    const QString text = index.data(Qt::DisplayRole).toString();
    if(text.isEmpty()) {
        return fallback;
    }

    RichTextBlock block;
    block.text          = text;
    block.format.font   = option.font;
    block.format.colour = option.palette.color(QPalette::Text);
    fallback.blocks.push_back(std::move(block));
    return fallback;
}

DrawTextResult drawTextBlocks(QPainter* painter, const QStyleOptionViewItem& option, QRect rect,
                              const std::vector<RichTextBlock>& blocks)
{
    DrawTextResult result;

    QStyle* style = option.widget ? option.widget->style() : QApplication::style();

    const auto selectedColour = option.palette.color(QPalette::HighlightedText);
    const auto defaultColour  = option.palette.color(QPalette::Text);
    const auto colourRole     = option.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::NoRole;

    for(const auto& block : blocks) {
        if(block.text.isEmpty() || rect.width() <= 0) {
            continue;
        }

        QFont font = block.format.font;
        if(font == QFont{}) {
            font = option.font;
        }

        painter->setFont(font);

        QColor colour = block.format.colour;
        if(!colour.isValid()) {
            colour = defaultColour;
        }
        if(option.state & QStyle::State_Selected) {
            colour = selectedColour;
        }
        painter->setPen(colour);

        const QFontMetrics metrics{font};
        const QString text = metrics.elidedText(block.text, Qt::ElideRight, rect.width());
        if(text.isEmpty()) {
            continue;
        }

        result.bound = painter->boundingRect(rect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, text);
        style->drawItemText(painter, rect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, option.palette, true,
                            text, colourRole);

        rect.setWidth(std::max(0, rect.width() - result.bound.width()));
        rect.moveLeft(rect.x() + result.bound.width());

        result.totalWidth += result.bound.width();
        result.maxHeight = std::max(result.maxHeight, result.bound.height());
    }

    return result;
}

QSize richTextSize(const QStyleOptionViewItem& option, const QModelIndex& index)
{
    const auto richText = fallbackRichText(option, index);
    if(richText.empty()) {
        const QFontMetrics metrics{option.font};
        return metrics.size(Qt::TextSingleLine, index.data(Qt::DisplayRole).toString());
    }

    QSize size;
    for(const auto& block : richText.blocks) {
        QFont font = block.format.font;
        if(font == QFont{}) {
            font = option.font;
        }

        const QFontMetrics metrics{font};
        const QRect bound = metrics.boundingRect(block.text);
        size.setWidth(size.width() + bound.width());
        size.setHeight(std::max(size.height(), bound.height()));
    }

    return size;
}
} // namespace

void LibraryTreeDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    opt.decorationSize = option.decorationSize;

    const auto decPos = index.data(LibraryTreeItem::Role::DecorationPosition).value<QStyleOptionViewItem::Position>();
    if(!opt.icon.isNull()) {
        opt.decorationPosition = decPos;
    }

    QStyle* style       = option.widget ? option.widget->style() : QApplication::style();
    const auto richText = fallbackRichText(opt, index);

    painter->save();

    if(opt.backgroundBrush.style() != Qt::NoBrush) {
        painter->fillRect(option.rect, opt.backgroundBrush);
        opt.backgroundBrush = Qt::NoBrush;
    }

    opt.text.clear();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, option.widget);

    if(!richText.empty()) {
        QRect textRect       = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, option.widget);
        const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &opt, option.widget) * 2;
        textRect.adjust(textMargin, 0, -textMargin, 0);
        drawTextBlocks(painter, opt, textRect, richText.blocks);
    }

    painter->restore();
}

QSize LibraryTreeDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);
    opt.decorationSize = option.decorationSize;

    const auto decPos = index.data(LibraryTreeItem::Role::DecorationPosition).value<QStyleOptionViewItem::Position>();
    if(!opt.icon.isNull()) {
        opt.decorationPosition = decPos;
    }

    const QStyle* style  = opt.widget ? opt.widget->style() : QApplication::style();
    const QSize textSize = richTextSize(opt, index);
    QSize size           = style->sizeFromContents(QStyle::CT_ItemViewItem, &opt, textSize, opt.widget);

    const QSize sizeHint = index.data(Qt::SizeHintRole).toSize();
    if(sizeHint.height() > 0) {
        size.setHeight(sizeHint.height());
    }

    return size;
}
} // namespace Fooyin
