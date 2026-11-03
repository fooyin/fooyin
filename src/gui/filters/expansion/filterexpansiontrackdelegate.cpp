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

#include <QAbstractItemView>
#include <QIcon>
#include <QItemSelectionModel>
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

    if(!hasInlineTrackEditors(layout)) {
        drawRichTextLayout(painter, opt, opt.rect.adjusted(4, 0, -4, 0), layout, icon);
        return;
    }
    const auto editors = inlineEditors(opt, index);
    drawRichTextLayout(painter, opt, opt.rect.adjusted(4, 0, -4, 0), layout, icon, &editors);
}

void ExpansionTrackDelegate::setHover(const QModelIndex& index, const InlineTrackEditorHit& hit, const QPoint& pos)
{
    m_hover.update(index, hit, pos,
                   index.data(Filters::ExpansionTrackModel::InlineEditorsRole).value<InlineTrackEditors>());
}

void ExpansionTrackDelegate::clearHover()
{
    m_hover.clear();
    m_hoveredRow = QPersistentModelIndex{};
}

void ExpansionTrackDelegate::setHoveredRow(const QModelIndex& index)
{
    m_hoveredRow = index;
}

InlineTrackEditorHit ExpansionTrackDelegate::inlineEditorAt(const QStyleOptionViewItem& option,
                                                            const QModelIndex& index, const QPoint& pos) const
{
    if(!index.isValid() || index.data(Filters::ExpansionTrackModel::TrackIndexRole).toInt() < 0) {
        return {};
    }

    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);

    const auto layout = index.data(Filters::ExpansionTrackModel::LayoutRole).value<RichTextLayout>();
    if(!hasInlineTrackEditors(layout)) {
        return {};
    }

    const auto icon    = index.data(Filters::ExpansionTrackModel::PlayingIconRole).value<QIcon>();
    const auto editors = index.data(Filters::ExpansionTrackModel::InlineEditorsRole).value<InlineTrackEditors>();
    return inlineTrackEditorAt(drawRichTextLayout(nullptr, opt, opt.rect.adjusted(4, 0, -4, 0), layout, icon, &editors),
                               pos);
}

QSize ExpansionTrackDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const auto size     = index.data(Qt::SizeHintRole).toSize();
    const int charWidth = option.fontMetrics.horizontalAdvance(u'M');

    auto hint = size.isValid() ? size : QSize{0, option.fontMetrics.height() + 5};
    hint.rwidth() += charWidth;
    return hint;
}

InlineTrackEditors ExpansionTrackDelegate::inlineEditors(const QStyleOptionViewItem& option,
                                                         const QModelIndex& index) const
{
    const auto* view      = qobject_cast<const QAbstractItemView*>(option.widget);
    const auto* selection = view ? view->selectionModel() : nullptr;
    auto editors          = index.data(Filters::ExpansionTrackModel::InlineEditorsRole).value<InlineTrackEditors>();
    editors.active        = m_hoveredRow == index || (selection && selection->isSelected(index));

    if(m_hover.index.isValid()
       && (m_hover.index == index
           || (selection && selection->isSelected(m_hover.index) && selection->isSelected(index)))) {
        m_hover.apply(editors);
    }

    return editors;
}
} // namespace Fooyin
