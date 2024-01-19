/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include <utils/multilinedelegate.h>

#include <QPainter>
#include <QTextEdit>

namespace Fooyin {
MultiLineEditDelegate::MultiLineEditDelegate(QWidget* parent)
    : QStyledItemDelegate{parent}
{ }

void MultiLineEditDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
    auto* textEdit = qobject_cast<QTextEdit*>(editor);
    if(textEdit) {
        model->setData(index, textEdit->toPlainText(), Qt::EditRole);
    }
    else {
        QStyledItemDelegate::setModelData(editor, model, index);
    }
}

QWidget* MultiLineEditDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                             const QModelIndex& /*index*/) const
{
    auto* editor = new QTextEdit(parent);
    editor->setAcceptRichText(false);
    editor->setTabChangesFocus(true);
    editor->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    const QSize size = {option.rect.width(), 150};
    editor->setFixedSize(size);
    return editor;
}
} // namespace Fooyin

#include "utils/moc_multilinedelegate.cpp"
