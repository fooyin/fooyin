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

#include "advancedsettingsmodel.h"

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
template <typename T>
[[nodiscard]] bool isEditor(const AdvancedSettingDescriptor& descriptor)
{
    return std::holds_alternative<T>(descriptor.editor);
}

[[nodiscard]] const AdvancedSettingSpinBox* spinBoxEditor(const AdvancedSettingDescriptor& descriptor)
{
    return std::get_if<AdvancedSettingSpinBox>(&descriptor.editor);
}

[[nodiscard]] const AdvancedSettingStringListLineEdit*
stringListLineEditEditor(const AdvancedSettingDescriptor& descriptor)
{
    return std::get_if<AdvancedSettingStringListLineEdit>(&descriptor.editor);
}

[[nodiscard]] const AdvancedSettingRadioButtons* radioButtonsEditor(const AdvancedSettingDescriptor& descriptor)
{
    return std::get_if<AdvancedSettingRadioButtons>(&descriptor.editor);
}

[[nodiscard]] bool isInlineTextEditor(const AdvancedSettingDescriptor& descriptor)
{
    return isEditor<AdvancedSettingLineEdit>(descriptor) || stringListLineEditEditor(descriptor)
        || spinBoxEditor(descriptor);
}

[[nodiscard]] QString stringListText(const QStringList& values, QChar separator)
{
    return values.join(QString{separator});
}

[[nodiscard]] QStringList stringListFromText(const QString& text, QChar separator)
{
    QStringList values = text.split(separator, Qt::SkipEmptyParts);
    for(QString& value : values) {
        value = value.trimmed();
    }
    values.removeAll(QString{});
    return values;
}

[[nodiscard]] QString displayText(const AdvancedSettingDescriptor& descriptor, const QVariant& value)
{
    if(const auto* spinBox = spinBoxEditor(descriptor)) {
        if(!spinBox->specialValueText.isEmpty() && value.toInt() == spinBox->minimum) {
            return spinBox->specialValueText;
        }
        return QString::number(value.toInt()) + spinBox->suffix;
    }
    if(const auto* stringList = stringListLineEditEditor(descriptor)) {
        return stringListText(value.toStringList(), stringList->separator);
    }
    return value.toString();
}
} // namespace

AdvancedSettingsModel::AdvancedSettingsModel(AdvancedSettingsRegistry* registry, QObject* parent)
    : TreeModel{parent}
    , m_registry{registry}
{ }

void AdvancedSettingsModel::load()
{
    beginResetModel();
    m_nodes.clear();
    resetRoot();

    std::vector<AdvancedSettingDescriptor> descriptors = m_registry->descriptors();
    std::ranges::sort(descriptors, [](const AdvancedSettingDescriptor& lhs, const AdvancedSettingDescriptor& rhs) {
        return std::tie(lhs.category, lhs.label, lhs.id) < std::tie(rhs.category, rhs.label, rhs.id);
    });

    for(const AdvancedSettingDescriptor& descriptor : descriptors) {
        addDescriptor(descriptor);
    }

    endResetModel();
}

void AdvancedSettingsModel::apply()
{
    applyNode(rootItem());
}

void AdvancedSettingsModel::reset()
{
    beginResetModel();
    resetNode(rootItem());
    endResetModel();
}

QString AdvancedSettingsModel::validationError() const
{
    return validationErrorForNode(rootItem());
}

QVariant AdvancedSettingsModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid()) {
        return {};
    }

    const auto* node = itemForIndex(index);

    const bool isSetting = node->type == AdvancedItemType::Setting;

    switch(role) {
        case Qt::DisplayRole:
            if(isSetting && isInlineTextEditor(node->descriptor)) {
                return u"%1: %2"_s.arg(node->label, displayText(node->descriptor, node->value));
            }
            return node->label;
        case Qt::EditRole:
            if(isSetting && isEditor<AdvancedSettingStringListLineEdit>(node->descriptor)) {
                return displayText(node->descriptor, node->value);
            }
            if(isSetting && (isEditor<AdvancedSettingLineEdit>(node->descriptor) || spinBoxEditor(node->descriptor))) {
                return node->value;
            }
            return {};
        case Qt::ToolTipRole:
            return node->description;
        case Qt::CheckStateRole:
            if(isSetting && isEditor<AdvancedSettingCheckBox>(node->descriptor)) {
                return node->value.toBool() ? Qt::Checked : Qt::Unchecked;
            }
            if(node->type == AdvancedItemType::RadioOption && node->parent()) {
                return node->parent()->value == node->value ? Qt::Checked : Qt::Unchecked;
            }
            return {};
        case ItemType:
            return static_cast<int>(node->type);
        case IsSpinBoxEditor:
            return isSetting && spinBoxEditor(node->descriptor);
        case MinimumValue:
            if(const auto* spinBox = spinBoxEditor(node->descriptor)) {
                return spinBox->minimum;
            }
            return {};
        case MaximumValue:
            if(const auto* spinBox = spinBoxEditor(node->descriptor)) {
                return spinBox->maximum;
            }
            return {};
        case SingleStep:
            if(const auto* spinBox = spinBoxEditor(node->descriptor)) {
                return spinBox->singleStep;
            }
            return {};
        case Suffix:
            if(const auto* spinBox = spinBoxEditor(node->descriptor)) {
                return spinBox->suffix;
            }
            return {};
        case SpecialValueText:
            if(const auto* spinBox = spinBoxEditor(node->descriptor)) {
                return spinBox->specialValueText;
            }
            return {};
        case StableKey:
            if(node->type == AdvancedItemType::Category) {
                return u"c:%1"_s.arg(node->label);
            }
            if(node->type == AdvancedItemType::Setting) {
                return u"s:%1"_s.arg(node->descriptor.id);
            }
            if(node->type == AdvancedItemType::RadioOption) {
                return u"o:%1"_s.arg(node->value.toString());
            }
            return {};
        default:
            return {};
    }
}

Qt::ItemFlags AdvancedSettingsModel::flags(const QModelIndex& index) const
{
    auto defaultFlags = TreeModel::flags(index);

    if(!index.isValid()) {
        return defaultFlags;
    }

    const auto* node = itemForIndex(index);

    if(node->type == AdvancedItemType::Setting) {
        if(isEditor<AdvancedSettingCheckBox>(node->descriptor)) {
            defaultFlags |= Qt::ItemIsUserCheckable;
        }
        else if(isInlineTextEditor(node->descriptor)) {
            defaultFlags |= Qt::ItemIsEditable;
        }
    }
    else if(node->type == AdvancedItemType::RadioOption) {
        defaultFlags |= Qt::ItemIsUserCheckable;
    }

    return defaultFlags;
}

bool AdvancedSettingsModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(!index.isValid()) {
        return false;
    }

    auto* node = itemForIndex(index);

    const bool isSetting = node->type == AdvancedItemType::Setting;

    if(role == Qt::EditRole && isSetting && isInlineTextEditor(node->descriptor)) {
        if(const auto* stringList = stringListLineEditEditor(node->descriptor)) {
            node->value = normaliseValue(node->descriptor, stringListFromText(value.toString(), stringList->separator));
        }
        else {
            node->value = normaliseValue(node->descriptor, value);
        }
        Q_EMIT dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
        return true;
    }

    if(role == Qt::CheckStateRole && isSetting && isEditor<AdvancedSettingCheckBox>(node->descriptor)) {
        node->value = value.toInt() == Qt::Checked;
        Q_EMIT dataChanged(index, index, {Qt::CheckStateRole});
        return true;
    }

    if(role == Qt::CheckStateRole && node->type == AdvancedItemType::RadioOption && node->parent()) {
        node->parent()->value = node->value;
        emitRadioSiblingsChanged(node);
        return true;
    }

    return false;
}

AdvancedNode* AdvancedSettingsModel::appendNode(AdvancedNode* parent, AdvancedItemType type, QString label)
{
    auto node   = std::make_unique<AdvancedNode>(parent);
    node->type  = type;
    node->label = std::move(label);

    auto* nodePtr = m_nodes.emplace_back(std::move(node)).get();
    parent->appendChild(nodePtr);
    return nodePtr;
}

AdvancedNode* AdvancedSettingsModel::categoryNode(const QStringList& path)
{
    AdvancedNode* parent{rootItem()};

    for(const QString& part : path) {
        const auto children = parent->children();
        const auto it       = std::ranges::find(children, part, &AdvancedNode::label);
        parent              = it != children.end() ? *it : appendNode(parent, AdvancedItemType::Category, part);
    }

    return parent;
}

void AdvancedSettingsModel::addDescriptor(const AdvancedSettingDescriptor& descriptor)
{
    if(descriptor.id.isEmpty() || !descriptor.read || !descriptor.write) {
        return;
    }

    auto* node        = appendNode(categoryNode(descriptor.category), AdvancedItemType::Setting, descriptor.label);
    node->description = descriptor.description;
    node->descriptor  = descriptor;
    node->value       = normaliseValue(descriptor, descriptor.read());

    if(const auto* radioButtons = radioButtonsEditor(descriptor)) {
        for(const auto& option : radioButtons->options) {
            auto* child        = appendNode(node, AdvancedItemType::RadioOption, option.label);
            child->description = descriptor.description;
            child->value       = option.value;
        }
    }
}

void AdvancedSettingsModel::applyNode(const AdvancedNode* node)
{
    if(node->type == AdvancedItemType::Setting && node->descriptor.write) {
        node->descriptor.write(normaliseValue(node->descriptor, node->value));
    }

    for(const auto* child : node->children()) {
        applyNode(child);
    }
}

void AdvancedSettingsModel::resetNode(AdvancedNode* node)
{
    if(node->type == AdvancedItemType::Setting) {
        node->value = normaliseValue(node->descriptor, node->descriptor.defaultValue);
        if(node->descriptor.write) {
            node->descriptor.write(node->value);
        }
    }

    for(auto* child : node->children()) {
        resetNode(child);
    }
}

QString AdvancedSettingsModel::validationErrorForNode(const AdvancedNode* node) const
{
    if(node->type == AdvancedItemType::Setting && node->descriptor.validate) {
        const QString error = node->descriptor.validate(normaliseValue(node->descriptor, node->value));
        if(!error.isEmpty()) {
            return error;
        }
    }

    for(const auto* child : node->children()) {
        if(const QString error = validationErrorForNode(child); !error.isEmpty()) {
            return error;
        }
    }

    return {};
}

QVariant AdvancedSettingsModel::normaliseValue(const AdvancedSettingDescriptor& descriptor, const QVariant& value)
{
    return descriptor.normalise ? descriptor.normalise(value) : value;
}

void AdvancedSettingsModel::emitRadioSiblingsChanged(const AdvancedNode* node)
{
    if(!node->parent()) {
        return;
    }

    for(auto* child : node->parent()->children()) {
        const QModelIndex childIndex = indexOfItem(child);
        Q_EMIT dataChanged(childIndex, childIndex, {Qt::CheckStateRole});
    }

    const QModelIndex parentIndex = indexOfItem(node->parent());
    Q_EMIT dataChanged(parentIndex, parentIndex, {Qt::DisplayRole});
}
} // namespace Fooyin
