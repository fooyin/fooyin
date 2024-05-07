/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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
#include <QPlainTextEdit>

namespace Fooyin {
QWidget* MultiLineEditDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                             const QModelIndex& index) const
{
    auto* editor = new QPlainTextEdit(parent);

    editor->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    editor->setTabChangesFocus(true);

    connect(editor, &QPlainTextEdit::textChanged, this,
            [this, editor, option, index]() { adjustEditorHeight(editor, option, index); });

    return editor;
}

void MultiLineEditDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    if(auto* textEdit = qobject_cast<QPlainTextEdit*>(editor)) {
        textEdit->setPlainText(index.data(Qt::EditRole).toString());
        textEdit->selectAll();
    }
    else {
        QStyledItemDelegate::setEditorData(editor, index);
    }
}

void MultiLineEditDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
    if(const auto* textEdit = qobject_cast<QPlainTextEdit*>(editor)) {
        model->setData(index, textEdit->toPlainText(), Qt::EditRole);
    }
    else {
        QStyledItemDelegate::setModelData(editor, model, index);
    }
}

void MultiLineEditDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                                                 const QModelIndex& index) const
{
    if(qobject_cast<QPlainTextEdit*>(editor)) {
        adjustEditorHeight(editor, option, index);
    }
    else {
        QStyledItemDelegate::updateEditorGeometry(editor, option, index);
    }
}

QSize MultiLineEditDelegate::editorSizeHint(QWidget* editor, const QStyleOptionViewItem& option,
                                            const QModelIndex& index) const
{
    const QVariant value = index.data(Qt::SizeHintRole);
    if(value.isValid()) {
        return value.toSize();
    }

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    if(const auto* textEdit = qobject_cast<QPlainTextEdit*>(editor)) {
        opt.text = textEdit->toPlainText();
    }

    const QStyle* style = editor->style();

    QSize size = style->sizeFromContents(QStyle::CT_ItemViewItem, &opt, {}, editor);

    return size;
}

void MultiLineEditDelegate::adjustEditorHeight(QWidget* editor, const QStyleOptionViewItem& option,
                                               const QModelIndex& index) const
{
    const QSize size = editorSizeHint(editor, option, index);

    QRect rect = option.rect;
    rect.setHeight(std::max(rect.height() * 2, size.height()));

    const int yPosToWindow = editor->parentWidget()->y() + editor->parentWidget()->height();
    const bool displayAbove
        = editor->mapTo(editor->parentWidget(), QPoint{0, option.rect.bottom()}).y() + rect.height() > yPosToWindow;

    if(displayAbove) {
        rect.moveBottom(option.rect.bottom());
        rect.setY(std::max(rect.y(), editor->parentWidget()->y()));
    }
    else {
        rect.setBottom(std::min(rect.bottom(), editor->parentWidget()->rect().bottom()));
    }

    editor->setGeometry(rect);
}
} // namespace Fooyin

#include "utils/moc_multilinedelegate.cpp"
