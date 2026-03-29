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

#include "outputdevicesdelegate.h"

#include "dsp/dsppresetregistry.h"
#include "outputdevicesmodel.h"

#include <core/engine/audioformat.h>
#include <gui/widgets/expandingcombobox.h>

#include <QComboBox>
#include <QCoreApplication>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace {
int widthForText(const QStyleOptionViewItem& option, const QString& text)
{
    return option.fontMetrics.horizontalAdvance(text) + 36; // Padding to account for editor combobox
}

int findBitdepthIndex(const QComboBox* box, int bitDepth, bool dither)
{
    if(!box) {
        return -1;
    }

    for(int i{0}; i < box->count(); ++i) {
        if(box->itemData(i).toInt() == bitDepth && box->itemData(i, Qt::UserRole + 1).toBool() == dither) {
            return i;
        }
    }

    return -1;
}
} // namespace

namespace Fooyin {
DspPresetDelegate::DspPresetDelegate(DspPresetRegistry* presetRegistry, QObject* parent)
    : QStyledItemDelegate{parent}
    , m_presetRegistry{presetRegistry}
{ }

QWidget* DspPresetDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& /*option*/,
                                         const QModelIndex& index) const
{
    auto* box = new QComboBox(parent);
    box->addItem(tr("<not set>"), -1);

    const auto presets = m_presetRegistry->items();
    for(const auto& preset : presets) {
        box->addItem(preset.name, preset.id);
    }

    const int dspPresetId = index.data(Qt::EditRole).toInt();
    const int itemIndex   = std::max(0, box->findData(dspPresetId));
    box->setCurrentIndex(itemIndex);

    return box;
}

QSize DspPresetDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QSize hint = QStyledItemDelegate::sizeHint(option, index);
    int width  = widthForText(option, tr("<not set>"));

    const auto presets = m_presetRegistry->items();
    for(const auto& preset : presets) {
        width = std::max(width, widthForText(option, preset.name));
    }

    hint.setWidth(std::max(hint.width(), width));
    return hint;
}

void DspPresetDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    auto* box = qobject_cast<QComboBox*>(editor);
    if(!box) {
        return;
    }

    const int dspPresetId = index.data(Qt::EditRole).toInt();
    const int itemIndex   = std::max(0, box->findData(dspPresetId));
    box->setCurrentIndex(itemIndex);
}

void DspPresetDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
    auto* box = qobject_cast<QComboBox*>(editor);
    if(!box) {
        return;
    }

    model->setData(index, box->currentData(), Qt::EditRole);
}

BitdepthDelegate::BitdepthDelegate(QObject* parent)
    : QStyledItemDelegate{parent}
{ }

QWidget* BitdepthDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& /*option*/,
                                        const QModelIndex& index) const
{
    auto* box = new ExpandingComboBox(parent);
    populate(box, index.data(OutputDevicesModel::BitdepthOptionsRole).value<BitdepthOptions>());

    const int bitDepth = index.data(OutputDevicesModel::BitDepthRole).toInt();
    const bool dither  = index.data(OutputDevicesModel::DitherRole).toBool();

    const int itemIndex = findBitdepthIndex(box, bitDepth, dither);
    box->setCurrentIndex(itemIndex >= 0 ? itemIndex : 0);

    box->resizeDropDown();
    return box;
}

QSize BitdepthDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QSize hint = QStyledItemDelegate::sizeHint(option, index);
    int width{0};

    const auto options = index.data(OutputDevicesModel::BitdepthOptionsRole).value<BitdepthOptions>();
    for(const auto& bitdepthOption : options) {
        width = std::max(width, widthForText(option, bitdepthOption.text));
    }

    hint.setWidth(std::max(hint.width(), width));
    return hint;
}

void BitdepthDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    auto* box = qobject_cast<QComboBox*>(editor);
    if(!box) {
        return;
    }

    const int bitDepth = index.data(OutputDevicesModel::BitDepthRole).toInt();
    const bool dither  = index.data(OutputDevicesModel::DitherRole).toBool();

    const int itemIndex = findBitdepthIndex(box, bitDepth, dither);
    box->setCurrentIndex(itemIndex >= 0 ? itemIndex : 0);
}

void BitdepthDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
    auto* box = qobject_cast<QComboBox*>(editor);
    if(!box) {
        return;
    }

    model->setData(index,
                   QVariant::fromValue(BitdepthOption{
                       .text     = box->currentText(),
                       .bitDepth = static_cast<SampleFormat>(box->currentData().toInt()),
                       .dither   = box->currentData(Qt::UserRole + 1).toBool(),
                   }),
                   OutputDevicesModel::BitdepthSelectionRole);
}

void BitdepthDelegate::populate(QComboBox* box, const BitdepthOptions& options)
{
    for(const auto& option : options) {
        box->addItem(option.text, static_cast<int>(option.bitDepth));
        box->setItemData(box->count() - 1, option.dither, Qt::UserRole + 1);
    }
}
} // namespace Fooyin
