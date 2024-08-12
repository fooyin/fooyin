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

#include <utils/stardelegate.h>

#include <utils/stareditor.h>
#include <utils/starrating.h>

#include <QApplication>
#include <QPainter>

namespace Fooyin {
QModelIndex StarDelegate::hoveredIndex() const
{
    return m_hoverIndex;
}

void StarDelegate::setHoverIndex(const QModelIndex& index)
{
    m_hoverIndex = index;
}

void StarDelegate::setHoverIndex(const QModelIndex& index, const QPoint& pos, const QModelIndexList& selected)
{
    m_hoverIndex = index;
    m_hoverPos   = pos;
    m_selected   = selected;
}

void StarDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);

    if(!index.data().canConvert<StarRating>()) {
        QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
        return;
    }

    auto starRating = index.data().value<StarRating>();

    const bool hover = m_hoverIndex.isValid()
                    && (m_hoverIndex == index || (m_selected.contains(m_hoverIndex) && m_selected.contains(index)));

    if(hover) {
        starRating.setRating(StarEditor::ratingAtPosition(m_hoverPos, option.rect, starRating, opt.displayAlignment));
    }

    starRating.paint(painter, opt.rect, opt.palette, StarRating::EditMode::ReadOnly, opt.displayAlignment);
}

QSize StarDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if(index.data().canConvert<StarRating>()) {
        const auto starRating = index.data().value<StarRating>();
        return starRating.sizeHint();
    }

    return QStyledItemDelegate::sizeHint(option, index);
}

QWidget* StarDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& /*option*/,
                                    const QModelIndex& index) const
{
    const auto align = static_cast<Qt::Alignment>(index.data(Qt::TextAlignmentRole).toInt());
    auto* editor     = new StarEditor(align, parent);
    QObject::connect(editor, &StarEditor::editingFinished, this, &StarDelegate::finishEditing);
    return editor;
}

void StarDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    if(index.data().canConvert<StarRating>()) {
        const auto starRating = index.data().value<StarRating>();
        auto* starEditor      = qobject_cast<StarEditor*>(editor);
        starEditor->setRating(starRating);
    }
    else {
        QStyledItemDelegate::setEditorData(editor, index);
    }
}

void StarDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
    auto* starEditor = qobject_cast<StarEditor*>(editor);
    model->setData(index, QVariant::fromValue(starEditor->rating()));
}

void StarDelegate::finishEditing()
{
    auto* editor = qobject_cast<StarEditor*>(sender());
    emit commitData(editor);
    emit closeEditor(editor);
}
} // namespace Fooyin
