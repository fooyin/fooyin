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
void StarDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);

    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    if(index.data().canConvert<StarRating>()) {
        const auto starRating = index.data().value<StarRating>();
        starRating.paint(painter, opt.rect, opt.palette, StarRating::EditMode::ReadOnly);
    }
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
                                    const QModelIndex& /*index*/) const
{
    auto* editor = new StarEditor(parent);
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
