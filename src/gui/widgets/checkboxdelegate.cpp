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

#include <gui/widgets/checkboxdelegate.h>

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>

namespace Fooyin {
QWidget* CheckBoxDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                        const QModelIndex& index) const
{
    if(index.data(Qt::CheckStateRole).isValid()) {
        // Prevent editing for checkbox cells
        return nullptr;
    }
    return QStyledItemDelegate::createEditor(parent, option, index);
}

void CheckBoxDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();

    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

    if(opt.features & QStyleOptionViewItem::HasCheckIndicator) {
        switch(opt.checkState) {
            case Qt::Unchecked:
                opt.state |= QStyle::State_Off;
                break;
            case Qt::PartiallyChecked:
                opt.state |= QStyle::State_NoChange;
                break;
            case Qt::Checked:
                opt.state |= QStyle::State_On;
                break;
        }
        const QRect rect = style->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &opt, opt.widget);
        opt.rect         = QStyle::alignedRect(opt.direction, index.data(Qt::TextAlignmentRole).value<Qt::Alignment>(),
                                               rect.size(), opt.rect);
        opt.state        = opt.state & ~QStyle::State_HasFocus;
        style->drawPrimitive(QStyle::PE_IndicatorItemViewItemCheck, &opt, painter, opt.widget);
    }
    else if(!opt.icon.isNull()) {
        QRect iconRect = style->subElementRect(QStyle::SE_ItemViewItemDecoration, &opt, opt.widget);
        iconRect       = QStyle::alignedRect(opt.direction, index.data(Qt::TextAlignmentRole).value<Qt::Alignment>(),
                                             iconRect.size(), opt.rect);

        QIcon::Mode mode = QIcon::Normal;
        if(!(opt.state & QStyle::State_Enabled)) {
            mode = QIcon::Disabled;
        }
        else if(opt.state & QStyle::State_Selected) {
            mode = QIcon::Selected;
        }

        const QIcon::State state = opt.state & QStyle::State_Open ? QIcon::On : QIcon::Off;
        opt.icon.paint(painter, iconRect, opt.decorationAlignment, mode, state);
    }
    else {
        QStyledItemDelegate::paint(painter, option, index);
    }
}

bool CheckBoxDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                                   const QModelIndex& index)
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    const Qt::ItemFlags flags = model->flags(index);
    if(!(flags & Qt::ItemIsUserCheckable) || !(opt.state & QStyle::State_Enabled) || !(flags & Qt::ItemIsEnabled)) {
        return false;
    }

    auto state = index.data(Qt::CheckStateRole).value<Qt::CheckState>();
    state      = state == Qt::Checked ? Qt::Unchecked : Qt::Checked;

    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();

    const auto type = event->type();
    if(type == QEvent::MouseButtonRelease || type == QEvent::MouseButtonDblClick || type == QEvent::MouseButtonPress) {
        QRect checkRect = style->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &opt, opt.widget);
        checkRect       = QStyle::alignedRect(opt.direction, index.data(Qt::TextAlignmentRole).value<Qt::Alignment>(),
                                              checkRect.size(), opt.rect);

        const auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if(mouseEvent->button() != Qt::LeftButton || !checkRect.contains(mouseEvent->position().toPoint())) {
            return false;
        }

        if(type == QEvent::MouseButtonPress || type == QEvent::MouseButtonDblClick) {
            return true;
        }
    }
    else if(event->type() == QEvent::KeyPress) {
        const auto key = static_cast<QKeyEvent*>(event)->key();
        if(key != Qt::Key_Space && key != Qt::Key_Select) {
            return false;
        }
    }
    else {
        return false;
    }

    return model->setData(index, state, Qt::CheckStateRole);
}
} // namespace Fooyin
