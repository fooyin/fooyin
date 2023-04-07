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

namespace Fy::Filters::Settings {

FieldModel::FieldModel(FieldRegistry* fieldsRegistry, QObject* parent)
    : TableModel{parent}
    , m_fieldsRegistry{fieldsRegistry}
{
    setupModelData();
}

void FieldModel::setupModelData()
{
    const auto& fields = m_fieldsRegistry->fields();

    for(const auto& [index, field] : fields) {
        if(field.name.isEmpty()) {
            continue;
        }
        FieldItem* parent = rootItem();
        FieldItem* child  = m_nodes.emplace_back(std::make_unique<FieldItem>(field, parent)).get();
        parent->appendChild(child);
    }
}

void FieldModel::addNewField()
{
    auto* parent    = rootItem();
    const int index = static_cast<int>(m_nodes.size());

    FilterField field;
    field.index = index;

    auto* item = m_nodes.emplace_back(std::make_unique<FieldItem>(field, parent)).get();

    const int row = parent->childCount();
    beginInsertRows({}, row, row);
    parent->appendChild(item);
    endInsertRows();

    m_queue.emplace_back(QueueEntry{Add, index});
}

void FieldModel::markForRemoval(const FilterField& field)
{
    QueueEntry operation;
    const bool isQueued = findInQueue(field.index, Add, &operation);

    if(isQueued) {
        removeFromQueue(operation);
    }

    FieldItem* item = m_nodes.at(field.index).get();

    beginRemoveRows({}, item->row(), item->row());
    rootItem()->removeChild(item->row());
    endRemoveRows();

    if(!isQueued) {
        operation = {Remove, field.index};
        m_queue.emplace_back(operation);
    }
}

void FieldModel::markForChange(const FilterField& field)
{
    FieldItem* item = m_nodes.at(field.index).get();
    item->changeField(field);
    const QModelIndex index = indexOfItem(item);
    emit dataChanged(index, index, {Qt::DisplayRole});

    m_queue.emplace_back(QueueEntry{Change, field.index});
}

void FieldModel::processQueue()
{
    while(!m_queue.empty()) {
        const QueueEntry& entry  = m_queue.front();
        const OperationType type = entry.type;
        FieldItem* item          = m_nodes.at(entry.index).get();
        const FilterField field  = item->field();

        m_queue.pop_front();

        if(type == Add) {
            const FilterField addedField = m_fieldsRegistry->addField(field);
            if(addedField.isValid()) {
                item->changeField(addedField);
            }
            else {
                qWarning() << QString{"Field %1 could not be added"}.arg(field.name);

                beginRemoveRows({}, item->row(), item->row());
                rootItem()->removeChild(item->row());
                endRemoveRows();
            }
        }
        else if(type == Remove) {
            if(m_fieldsRegistry->removeByIndex(field.index)) {
                m_nodes.erase(std::remove_if(m_nodes.begin(),
                                             m_nodes.end(),
                                             [field](const auto& item) {
                                                 return item->field().index == field.index;
                                             }),
                              m_nodes.end());
            }
            else {
                qWarning() << QString{"Field (%1) could not be removed"}.arg(field.name);

                const int row = rootItem()->childCount();
                beginInsertRows({}, row, row);
                rootItem()->appendChild(item);
                endInsertRows();
            }
        }
        else if(type == Change) {
            if(!m_fieldsRegistry->changeField(field)) {
                qWarning() << QString{"Field (%1) could not be changed"}.arg(field.name);

                item->changeField(m_fieldsRegistry->fieldByIndex(field.index));
                const QModelIndex index = indexOfItem(item);
                emit dataChanged(index, index, {Qt::DisplayRole});
            }
        }
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
    if(role != Qt::DisplayRole && role != Qt::EditRole) {
        return {};
    }

    if(!index.isValid()) {
        return {};
    }

    const int column = index.column();
    auto* item       = static_cast<FieldItem*>(index.internalPointer());

    switch(column) {
        case(0):
            return item->field().index;
        case(1): {
            const QString& name = item->field().name;
            return !name.isEmpty() ? name : "<enter name here>";
        }
        case(2): {
            const QString& field = item->field().field;
            return !field.isEmpty() ? field : "<enter field here>";
        }
        case(3): {
            const QString& sortField = item->field().sortField;
            return !sortField.isEmpty() || item->field().isValid() ? sortField : "<enter sort field here>";
        }
    }
    return {};
}

bool FieldModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(role != Qt::EditRole) {
        return false;
    }

    auto* item        = static_cast<FieldItem*>(index.internalPointer());
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

int FieldModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return 4;
}

bool FieldModel::findInQueue(int index, OperationType type, QueueEntry* field) const
{
    return std::any_of(m_queue.cbegin(), m_queue.cend(), [index, type, field](const QueueEntry& entry) {
        if(entry.index == index && entry.type == type) {
            *field = entry;
            return true;
        }
        return false;
    });
}

void FieldModel::removeFromQueue(const QueueEntry& fieldToDelete)
{
    m_queue.erase(std::remove_if(m_queue.begin(),
                                 m_queue.end(),
                                 [fieldToDelete](const QueueEntry& field) {
                                     return field == fieldToDelete;
                                 }),
                  m_queue.end());
}
} // namespace Fy::Filters::Settings
