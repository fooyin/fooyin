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

#include "appearancepropertymodel.h"

#include <gui/brushsource.h>

#include <QApplication>
#include <QFont>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
bool isCheckProperty(AppearanceProperty::Type type)
{
    return type == AppearanceProperty::Type::Boolean || type == AppearanceProperty::Type::OptionalBoolean
        || type == AppearanceProperty::Type::FontBold;
}

const QJsonValue& effectiveValue(const AppearanceProperty& property)
{
    if(!property.value.isUndefined()) {
        return property.value;
    }
    return property.inheritedValue.isUndefined() ? property.defaultValue : property.inheritedValue;
}

QJsonValue propertyValue(QJsonObject object, QStringView group, const QString& key)
{
    while(!group.isEmpty()) {
        const auto separator = group.indexOf(u'.');
        object               = object.value((separator < 0 ? group : group.first(separator)).toString()).toObject();
        group                = separator < 0 ? QStringView{} : group.sliced(separator + 1);
    }
    return object.value(key);
}
} // namespace

AppearancePropertyModel::AppearancePropertyModel(QObject* parent)
    : TreeModel{parent}
    , m_colourPalette{QApplication::palette()}
{ }

int AppearancePropertyModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 2;
}

QVariant AppearancePropertyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        if(section == 0) {
            return tr("Property");
        }
        if(section == 1) {
            return tr("Value");
        }
    }
    return {};
}

QVariant AppearancePropertyModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }
    if(itemForIndex(index)->parent() == rootItem()) {
        return {};
    }

    const auto& entry = property(index);
    const auto& value = effectiveValue(entry);

    switch(role) {
        case Qt::DisplayRole:
            if(index.column() == 0) {
                return entry.label;
            }
            if(index.column() == 1) {
                if(entry.type == AppearanceProperty::Type::Group) {
                    return summary(itemForIndex(index));
                }
                if((entry.type == AppearanceProperty::Type::OptionalBoolean && value.isNull())
                   || (entry.type == AppearanceProperty::Type::FontBold && value.toInt() == 0)) {
                    return tr("Inherited");
                }
                if(entry.type == AppearanceProperty::Type::Number) {
                    if(!entry.specialValueText.isEmpty() && value.toDouble() == entry.minimum) {
                        return entry.specialValueText;
                    }
                    return QString{QLocale{}.toString(value.toDouble(), 'f', entry.decimals) + entry.suffix};
                }
                if(entry.type == AppearanceProperty::Type::Colour || entry.type == AppearanceProperty::Type::Brush) {
                    const auto colour = readBrushSource(value, entry.key);
                    return colour ? brushSourceName(*colour) : QString{};
                }
                if(entry.type == AppearanceProperty::Type::Choice) {
                    const auto choice
                        = std::ranges::find(entry.choices, value.toString(), &std::pair<QString, QString>::second);
                    return choice != entry.choices.end() ? choice->first : value.toString();
                }
            }
            return {};
        case Qt::EditRole:
            return index.column() == 1 ? value.toVariant() : QVariant{};
        case OverrideValueRole:
            return QVariant::fromValue(entry.value);
        case PropertyRole:
            return QVariant::fromValue(entry);
        case Qt::CheckStateRole:
            if(index.column() == 1 && isCheckProperty(entry.type)) {
                if(entry.type == AppearanceProperty::Type::FontBold) {
                    return value.toInt() == 0            ? Qt::PartiallyChecked
                         : value.toInt() > QFont::Medium ? Qt::Checked
                                                         : Qt::Unchecked;
                }
                if(entry.type == AppearanceProperty::Type::OptionalBoolean && value.isNull()) {
                    return Qt::PartiallyChecked;
                }
                return value.toBool() ? Qt::Checked : Qt::Unchecked;
            }
            return {};
        case Qt::FontRole:
            if(index.column() == 0 && !entry.value.isUndefined()) {
                QFont font;
                font.setBold(true);
                return font;
            }
            return {};
        case Qt::ToolTipRole: {
            QString text = entry.toolTip.isEmpty() ? entry.label : entry.label + u'\n' + entry.toolTip;
            if(entry.type == AppearanceProperty::Type::Group) {
                text += u'\n' + summary(itemForIndex(index));
            }
            if(!entry.inheritanceGroups.empty()) {
                text += u'\n';
                text += !entry.value.isUndefined()         ? tr("Overridden for this state")
                      : entry.inheritedValue.isUndefined() ? tr("Uses this state’s default; follows inherited changes")
                                                           : tr("Uses an inherited value");
            }
            else if(entry.stateBase) {
                text += u'\n' + tr("Changes are inherited by other states unless overridden");
            }
            return text;
        }
        case InheritedRole:
            return !entry.inheritanceGroups.empty() && entry.value.isUndefined();
        case CanInheritRole:
            return !entry.inheritanceGroups.empty();
        case ResetRole:
            return !entry.value.isUndefined();
        case ColourPaletteRole:
            return QVariant::fromValue(m_colourPalette);
        default:
            return {};
    }
}

Qt::ItemFlags AppearancePropertyModel::flags(const QModelIndex& index) const
{
    auto flags = TreeModel::flags(index);
    if(!index.isValid() || itemForIndex(index)->parent() == rootItem()) {
        return flags;
    }

    if(index.column() == 1) {
        flags |= Qt::ItemIsEditable;

        if(isCheckProperty(property(index).type)) {
            flags |= Qt::ItemIsUserCheckable;

            if(property(index).type != AppearanceProperty::Type::Boolean) {
                flags |= Qt::ItemIsUserTristate;
            }
        }
    }

    return flags;
}

bool AppearancePropertyModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(!index.isValid() || itemForIndex(index)->parent() == rootItem()) {
        return false;
    }

    auto* item  = itemForIndex(index);
    auto& entry = item->property;

    if(entry.type == AppearanceProperty::Type::Group) {
        if(index.column() != 1 || role != ResetRole || entry.value.isUndefined()) {
            return false;
        }

        clearOverrides(item);
        updateAncestors(item);
        notifyChanges(item->page);

        Q_EMIT propertiesEdited(indexOfItem(item->page), {{.group = entry.group, .key = entry.key}});
        return true;
    }

    if(index.column() != 1) {
        return false;
    }

    QJsonValue updated{QJsonValue::Undefined};

    if(role == OverrideValueRole) {
        updated = value.value<QJsonValue>();
    }
    else if(role == PinRole) {
        if(entry.inheritanceGroups.empty()) {
            return false;
        }
        if(entry.value.isUndefined()) {
            updated = effectiveValue(entry);
        }
    }
    else if(role == ResetRole) {
        if(entry.value.isUndefined()) {
            return false;
        }
    }
    else {
        if(isCheckProperty(entry.type) && role == Qt::CheckStateRole) {
            if(entry.type != AppearanceProperty::Type::Boolean && value.toInt() == Qt::PartiallyChecked) {
                updated = QJsonValue::Undefined;
            }
            else if(entry.type == AppearanceProperty::Type::FontBold) {
                updated = value.toInt() == Qt::Checked ? QFont::Bold : QFont::Normal;
            }
            else {
                updated = value.toInt() == Qt::Checked;
            }
        }
        else if(entry.type == AppearanceProperty::Type::Number && role == Qt::EditRole) {
            const auto number = value.toDouble();
            if(!std::isfinite(number) || number < entry.minimum || number > entry.maximum) {
                return false;
            }
            updated = number;
        }
        else if((entry.type == AppearanceProperty::Type::Colour || entry.type == AppearanceProperty::Type::Brush)
                && role == Qt::EditRole) {
            if(entry.type == AppearanceProperty::Type::Colour
               && !readColourSource(QJsonValue::fromVariant(value), entry.key)) {
                return false;
            }

            const auto colour = readBrushSource(QJsonValue::fromVariant(value), entry.key);
            if(!colour) {
                return false;
            }

            const auto current = readBrushSource(effectiveValue(entry), entry.key);
            if(current && *current == *colour) {
                return false;
            }
            updated = brushSourceToJson(*colour);
        }
        else if(entry.type == AppearanceProperty::Type::Choice && role == Qt::EditRole) {
            if(std::ranges::find(entry.choices, value.toString(), &std::pair<QString, QString>::second)
               == entry.choices.end()) {
                return false;
            }
            updated = value.toString();
        }
        else {
            return false;
        }
    }

    if(entry.value == updated || (role != PinRole && entry.value.isUndefined() && updated == effectiveValue(entry))) {
        return false;
    }

    // Aliases like Bold and Weight edit the same stored property
    const auto children = item->parent()->children();
    for(auto* candidate : children) {
        if(candidate->property.group == entry.group && candidate->property.key == entry.key) {
            candidate->property.value = updated;
        }
    }

    updateAncestors(item);
    notifyChanges(item->page);
    Q_EMIT propertiesEdited(indexOfItem(item->page), {{.group = entry.group, .key = entry.key, .value = updated}});
    return true;
}

const AppearanceProperty& AppearancePropertyModel::property(const QModelIndex& index) const
{
    return itemForIndex(index)->property;
}

QModelIndex AppearancePropertyModel::addPage()
{
    const int row = rootItem()->childCount();
    beginInsertRows({}, row, row);
    auto& page = m_nodes.emplace_back();
    page.page  = &page;
    rootItem()->appendChild(&page);
    endInsertRows();
    return indexOfItem(&page);
}

AppearanceProperty* AppearancePropertyModel::addProperty(const QModelIndex& page, AppearanceProperty property)
{
    auto* pageItem = itemForIndex(page);
    auto* parent   = pageItem;
    for(auto& candidate : m_nodes) {
        const auto& entry = candidate.property;
        const auto path   = entry.group.isEmpty() ? entry.key : entry.group + u'.' + entry.key;
        if(candidate.page == pageItem && entry.type == AppearanceProperty::Type::Group && property.group == path) {
            parent = &candidate;
            break;
        }
    }

    const int row          = parent->childCount();
    const auto parentIndex = indexOfItem(parent);

    beginInsertRows(parentIndex, row, row);
    auto& item    = m_nodes.emplace_back();
    item.property = std::move(property);
    item.page     = pageItem;
    parent->appendChild(&item);
    endInsertRows();

    return &m_nodes.back().property;
}

void AppearancePropertyModel::setOverrides(const QModelIndex& page, const QJsonObject& overrides)
{
    auto* pageItem = itemForIndex(page);

    const auto update = [&](auto&& self, AppearancePropertyNode* parent) -> void {
        const auto children = parent->children();
        for(auto* item : children) {
            auto& entry{item->property};
            entry.value          = propertyValue(overrides, entry.group, entry.key);
            entry.inheritedValue = QJsonValue::Undefined;

            for(const auto& group : entry.inheritanceGroups) {
                const auto value = propertyValue(overrides, group, entry.key);
                if(!value.isUndefined()) {
                    entry.inheritedValue = value;
                    break;
                }
            }

            self(self, item);
        }
    };

    update(update, pageItem);
    notifyChanges(pageItem);
}

void AppearancePropertyModel::resetOverrides(const QModelIndex& page)
{
    std::vector<AppearancePropertyEdit> edits;

    const auto children = itemForIndex(page)->children();
    for(const auto* item : children) {
        const auto& entry = item->property;
        if(!entry.value.isUndefined()) {
            edits.push_back({.group = entry.group, .key = entry.key});
        }
    }

    if(!edits.empty()) {
        Q_EMIT propertiesEdited(page, edits);
    }
}

void AppearancePropertyModel::setColourPalette(const QPalette& palette)
{
    m_colourPalette = palette;
    notifyChanges();
}

QString AppearancePropertyModel::summary(const AppearancePropertyNode* item) const
{
    QStringList values;

    const auto children = item->children();
    for(const auto* child : children) {
        const auto& entry = child->property;
        if(entry.value.isUndefined() || entry.type == AppearanceProperty::Type::FontBold) {
            continue;
        }

        if(entry.type == AppearanceProperty::Type::Boolean || entry.type == AppearanceProperty::Type::OptionalBoolean) {
            if(!entry.value.isNull()) {
                values.push_back(entry.value.toBool() ? entry.label : tr("%1: off").arg(entry.label));
            }
        }
        else {
            if(entry.type == AppearanceProperty::Type::Number && entry.value.toInt() == QFont::Bold) {
                const auto bold = std::ranges::find_if(children, [&entry](const AppearancePropertyNode* sibling) {
                    return sibling->property.type == AppearanceProperty::Type::FontBold
                        && sibling->property.group == entry.group && sibling->property.key == entry.key;
                });
                if(bold != children.end()) {
                    values.push_back((*bold)->property.label);
                    continue;
                }
            }

            const auto text = data(indexOfItem(child).siblingAtColumn(1), Qt::DisplayRole).toString();
            if(!text.isEmpty() && text != tr("Inherited")) {
                values.push_back(entry.type == AppearanceProperty::Type::Number && entry.suffix.isEmpty()
                                     ? u"%1: %2"_s.arg(entry.label, text)
                                     : text);
            }
        }
    }

    return values.isEmpty() ? tr("Inherited") : values.join(u", "_s);
}

void AppearancePropertyModel::clearOverrides(AppearancePropertyNode* item)
{
    item->property.value = QJsonValue::Undefined;
    const auto children  = item->children();
    for(auto* child : children) {
        clearOverrides(child);
    }
}

void AppearancePropertyModel::updateAncestors(AppearancePropertyNode* item)
{
    while(item->parent() != item->page) {
        auto* parent = item->parent();

        auto value = parent->property.value.toObject();
        value.insert(item->property.key, item->property.value);
        parent->property.value = value.isEmpty() ? QJsonValue{QJsonValue::Undefined} : QJsonValue{value};

        item = parent;
    }
}

void AppearancePropertyModel::notifyChanges(AppearancePropertyNode* parent)
{
    if(!parent) {
        parent = rootItem();
    }

    const auto children    = parent->children();
    const auto parentIndex = indexOfItem(parent);

    if(!children.empty()) {
        Q_EMIT dataChanged(index(0, 0, parentIndex), index(static_cast<int>(children.size()) - 1, 1, parentIndex));
        for(auto* child : children) {
            notifyChanges(child);
        }
    }
}
} // namespace Fooyin
