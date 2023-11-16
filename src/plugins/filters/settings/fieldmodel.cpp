/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "fieldmodel.h"

#include "fieldregistry.h"

#include <QFont>

namespace Fooyin::Filters {
FieldItem::FieldItem()
    : FieldItem{{}, nullptr}
{ }

FieldItem::FieldItem(FilterField field, FieldItem* parent)
    : TreeStatusItem{parent}
    , m_field{std::move(field)}
{ }

FilterField FieldItem::field() const
{
    return m_field;
}

void FieldItem::changeField(const FilterField& field)
{
    m_field = field;
}

FieldModel::FieldModel(FieldRegistry* fieldsRegistry, QObject* parent)
    : TableModel{parent}
    , m_fieldsRegistry{fieldsRegistry}
{ }

void FieldModel::populate()
{
    beginResetModel();
    reset();

    const auto& fields = m_fieldsRegistry->items();

    for(const auto& [index, field] : fields) {
        if(field.name.isEmpty()) {
            continue;
        }
        FieldItem* parent = rootItem();
        FieldItem* child  = &m_nodes.emplace(index, FieldItem{field, parent}).first->second;
        parent->insertChild(index, child);
    }

    endResetModel();
}

void FieldModel::addNewField()
{
    const int index = static_cast<int>(m_nodes.size());

    FilterField field;
    field.index = index;

    FieldItem* parent = rootItem();
    FieldItem* item   = &m_nodes.emplace(index, FieldItem{field, parent}).first->second;

    item->setStatus(FieldItem::Added);

    const int row = parent->childCount();
    beginInsertRows({}, row, row);
    parent->appendChild(item);
    endInsertRows();
}

void FieldModel::markForRemoval(const FilterField& field)
{
    FieldItem* item = &m_nodes.at(field.index);

    if(item->status() == FieldItem::Added) {
        beginRemoveRows({}, item->row(), item->row());
        rootItem()->removeChild(item->row());
        endRemoveRows();

        removeField(field.index);
    }
    else {
        item->setStatus(FieldItem::Removed);
        emit dataChanged({}, {}, {Qt::FontRole});
    }
}

void FieldModel::markForChange(const FilterField& field)
{
    FieldItem* item = &m_nodes.at(field.index);

    item->changeField(field);
    const QModelIndex index = indexOfItem(item);
    emit dataChanged(index, index, {Qt::DisplayRole});

    if(item->status() == FieldItem::None) {
        item->setStatus(FieldItem::Changed);
    }
}

void FieldModel::processQueue()
{
    std::vector<int> fieldsToRemove;

    for(auto& [index, node] : m_nodes) {
        const FieldItem::ItemStatus status = node.status();
        const FilterField field            = node.field();

        switch(status) {
            case(FieldItem::Added): {
                const FilterField addedField = m_fieldsRegistry->addItem(field);
                if(addedField.isValid()) {
                    node.changeField(addedField);
                    node.setStatus(FieldItem::None);

                    emit dataChanged({}, {});
                }
                else {
                    qWarning() << "Field " + field.name + " could not be added";
                }
                break;
            }
            case(FieldItem::Removed): {
                if(m_fieldsRegistry->removeByIndex(field.index)) {
                    beginRemoveRows({}, node.row(), node.row());
                    rootItem()->removeChild(node.row());
                    endRemoveRows();
                    fieldsToRemove.push_back(index);
                }
                else {
                    qWarning() << "Field " + field.name + " could not be removed";
                }
                break;
            }
            case(FieldItem::Changed): {
                if(m_fieldsRegistry->changeItem(field)) {
                    node.changeField(m_fieldsRegistry->itemById(field.id));
                    node.setStatus(FieldItem::None);

                    emit dataChanged({}, {});
                }
                else {
                    qWarning() << "Field " + field.name + " could not be changed";
                }
                break;
            }
            case(FieldItem::None):
                break;
        }
    }
    for(const auto& index : fieldsToRemove) {
        removeField(index);
    }
}

Qt::ItemFlags FieldModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }

    auto flags = TableModel::flags(index);
    flags |= Qt::ItemIsEditable;

    return flags;
}

QVariant FieldModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    switch(section) {
        case(0):
            return "Index";
        case(1):
            return "Name";
        case(2):
            return "Field";
        case(3):
            return "Sort Field";
    }
    return {};
}

QVariant FieldModel::data(const QModelIndex& index, int role) const
{
    if(role != Qt::DisplayRole && role != Qt::EditRole && role != Qt::FontRole) {
        return {};
    }

    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item = static_cast<FieldItem*>(index.internalPointer());

    if(role == Qt::FontRole) {
        return item->font();
    }

    switch(index.column()) {
        case(0):
            return item->field().index;
        case(1): {
            const QString& name = item->field().name;
            return !name.isEmpty() ? name : QStringLiteral("<enter name here>");
        }
        case(2): {
            const QString& field = item->field().field;
            return !field.isEmpty() ? field : QStringLiteral("<enter field here>");
        }
        case(3): {
            const QString& sortField = item->field().sortField;
            return !sortField.isEmpty() || item->field().isValid() ? sortField
                                                                   : QStringLiteral("<enter sort field here>");
        }
    }
    return {};
}

bool FieldModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(role != Qt::EditRole) {
        return false;
    }

    const auto* item  = static_cast<FieldItem*>(index.internalPointer());
    const int col     = index.column();
    FilterField field = item->field();

    switch(col) {
        case(1): {
            if(field.name == value.toString()) {
                return false;
            }
            field.name = value.toString();
            break;
        }
        case(2): {
            if(field.field == value.toString()) {
                return false;
            }
            field.field = value.toString();
            break;
        }
        case(3): {
            if(field.sortField == value.toString()) {
                return false;
            }
            field.sortField = value.toString();
            break;
        }
        case(0):
            break;
    }

    markForChange(field);

    return true;
}

int FieldModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 4;
}

void FieldModel::reset()
{
    resetRoot();
    m_nodes.clear();
}

void FieldModel::removeField(int index)
{
    if(m_nodes.contains(index)) {
        m_nodes.erase(index);
    }
}
} // namespace Fooyin::Filters
