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

#include "advanceditemdelegate.h"

#include "advancedsettingsmodel.h"

#include <QApplication>
#include <QMouseEvent>
#include <QSpinBox>

namespace Fooyin {
QWidget* AdvancedItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                            const QModelIndex& index) const
{
    using Role = AdvancedSettingsModel::AdvancedItemRole;

    if(!index.data(Role::IsSpinBoxEditor).toBool()) {
        return QStyledItemDelegate::createEditor(parent, option, index);
    }

    auto* spinBox = new QSpinBox(parent);
    spinBox->setRange(index.data(Role::MinimumValue).toInt(), index.data(Role::MaximumValue).toInt());
    spinBox->setSingleStep(std::max(1, index.data(Role::SingleStep).toInt()));
    spinBox->setSuffix(index.data(Role::Suffix).toString());
    spinBox->setSpecialValueText(index.data(Role::SpecialValueText).toString());
    return spinBox;
}

void AdvancedItemDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    if(auto* spinBox = qobject_cast<QSpinBox*>(editor)) {
        spinBox->setValue(index.data(Qt::EditRole).toInt());
        return;
    }

    QStyledItemDelegate::setEditorData(editor, index);
}

void AdvancedItemDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
    if(auto* spinBox = qobject_cast<QSpinBox*>(editor)) {
        model->setData(index, spinBox->value(), Qt::EditRole);
        return;
    }

    QStyledItemDelegate::setModelData(editor, model, index);
}

void AdvancedItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if(index.data(AdvancedSettingsModel::AdvancedItemRole::ItemType).toInt()
       != static_cast<int>(AdvancedItemType::RadioOption)) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);
    opt.features &= ~QStyleOptionViewItem::HasCheckIndicator;
    opt.text.clear();

    const QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    QStyleOptionButton radioOption;
    radioOption.state = QStyle::State_Enabled;
    if(opt.state & QStyle::State_MouseOver) {
        radioOption.state |= QStyle::State_MouseOver;
    }
    if(index.data(Qt::CheckStateRole).toInt() == Qt::Checked) {
        radioOption.state |= QStyle::State_On;
    }
    else {
        radioOption.state |= QStyle::State_Off;
    }

    const QRect indicatorRect = style->subElementRect(QStyle::SE_RadioButtonIndicator, &radioOption, opt.widget);

    radioOption.rect = QRect{opt.rect.left(), opt.rect.top(), indicatorRect.width(), opt.rect.height()};
    radioOption.rect.moveTop(opt.rect.top() + ((opt.rect.height() - indicatorRect.height()) / 2));
    radioOption.rect.setHeight(indicatorRect.height());

    style->drawPrimitive(QStyle::PE_IndicatorRadioButton, &radioOption, painter, opt.widget);

    QRect textRect{opt.rect};
    textRect.setLeft(radioOption.rect.right()
                     + style->pixelMetric(QStyle::PM_CheckBoxLabelSpacing, nullptr, opt.widget));

    const auto textRole = opt.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::Text;
    style->drawItemText(painter, textRect, static_cast<int>(opt.displayAlignment), opt.palette, true,
                        index.data().toString(), textRole);
}

bool AdvancedItemDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                                       const QModelIndex& index)
{
    if(index.data(AdvancedSettingsModel::AdvancedItemRole::ItemType).toInt()
       != static_cast<int>(AdvancedItemType::RadioOption)) {
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }

    if(event->type() == QEvent::MouseButtonRelease) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if(mouseEvent->button() == Qt::LeftButton) {
            return model->setData(index, Qt::Checked, Qt::CheckStateRole);
        }
    }

    if(event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if(keyEvent->key() == Qt::Key_Space || keyEvent->key() == Qt::Key_Select) {
            return model->setData(index, Qt::Checked, Qt::CheckStateRole);
        }
    }

    return false;
}
} // namespace Fooyin
