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

#include "filterscolumnmodel.h"

#include "filtercolumnregistry.h"
#include "filtersettings.h"

#include <QFont>

using namespace Qt::StringLiterals;

namespace Fooyin::Filters {
ColumnItem::ColumnItem()
    : ColumnItem{{}, nullptr}
{ }

ColumnItem::ColumnItem(FilterColumn column, ColumnItem* parent)
    : TreeStatusItem{parent}
    , m_column{std::move(column)}
{ }

FilterColumn ColumnItem::column() const
{
    return m_column;
}

void ColumnItem::changeColumn(const FilterColumn& column)
{
    m_column = column;
}

FiltersColumnModel::FiltersColumnModel(FilterColumnRegistry* columnsRegistry, QObject* parent)
    : ExtendableTableModel{parent}
    , m_columnsRegistry{columnsRegistry}
{ }

void FiltersColumnModel::populate()
{
    beginResetModel();
    m_root = {};
    m_nodes.clear();

    const auto& columns = m_columnsRegistry->items();

    for(const auto& column : columns) {
        if(column.name.isEmpty()) {
            continue;
        }
        ColumnItem* child = &m_nodes.emplace(column.index, ColumnItem{column, &m_root}).first->second;
        m_root.insertChild(column.index, child);
    }

    endResetModel();
}

void FiltersColumnModel::processQueue()
{
    std::vector<int> columnsToRemove;

    for(auto& [index, node] : m_nodes) {
        const ColumnItem::ItemStatus status = node.status();
        const FilterColumn column           = node.column();

        switch(status) {
            case(ColumnItem::Added): {
                if(column.field.isEmpty()) {
                    break;
                }

                const FilterColumn addedField = m_columnsRegistry->addItem(column);
                if(addedField.isValid()) {
                    node.changeColumn(addedField);
                    node.setStatus(ColumnItem::None);
                }
                else {
                    qCWarning(FILTERS) << "Column could not be added:" << column.name;
                }
                break;
            }
            case(ColumnItem::Removed): {
                if(m_columnsRegistry->removeById(column.id)) {
                    beginRemoveRows({}, node.row(), node.row());
                    m_root.removeChild(node.row());
                    endRemoveRows();
                    columnsToRemove.push_back(index);
                }
                else {
                    qCWarning(FILTERS) << "Column could not be removed:" << column.name;
                }
                break;
            }
            case(ColumnItem::Changed): {
                if(m_columnsRegistry->changeItem(column)) {
                    if(const auto item = m_columnsRegistry->itemById(column.id)) {
                        node.changeColumn(item.value());
                        node.setStatus(ColumnItem::None);
                    }
                }
                else {
                    qCWarning(FILTERS) << "Column ould not be changed:" << column.name;
                }
                break;
            }
            case(ColumnItem::None):
                break;
        }
    }

    for(const auto& index : columnsToRemove) {
        m_nodes.erase(index);
    }

    invalidateData();
}

Qt::ItemFlags FiltersColumnModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }

    auto flags = ExtendableTableModel::flags(index);

    auto* item = static_cast<ColumnItem*>(index.internalPointer());
    if(item && !item->column().isDefault) {
        flags |= Qt::ItemIsEditable;
    }

    return flags;
}

QVariant FiltersColumnModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    switch(section) {
        case(0):
            return tr("Index");
        case(1):
            return tr("Name");
        case(2):
            return tr("Field");
        default:
            break;
    }

    return {};
}

QVariant FiltersColumnModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item = static_cast<ColumnItem*>(index.internalPointer());

    if(role == Qt::FontRole) {
        return item->font();
    }

    if(role == Qt::UserRole) {
        return QVariant::fromValue(item->column());
    }

    if(role == Qt::DisplayRole || role == Qt::EditRole) {
        switch(index.column()) {
            case(0):
                return item->column().index;
            case(1): {
                const QString& name = item->column().name;
                return !name.isEmpty() ? name : u"<enter name here>"_s;
            }
            case(2): {
                const QString& field = item->column().field;
                return !field.isEmpty() ? field : u"<enter field here>"_s;
            }
            default:
                break;
        }
    }

    return {};
}

bool FiltersColumnModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(role != Qt::EditRole) {
        return false;
    }

    auto* item          = static_cast<ColumnItem*>(index.internalPointer());
    FilterColumn column = item->column();

    switch(index.column()) {
        case(1): {
            if(value.toString() == u"<enter name here>"_s || column.name == value.toString()) {
                if(item->status() == ColumnItem::Added) {
                    emit pendingRowCancelled();
                }
                return false;
            }
            column.name = value.toString();
            break;
        }
        case(2): {
            if(column.field == value.toString()) {
                return false;
            }
            column.field = value.toString();
            break;
        }
        default:
            break;
    }

    if(item->status() == ColumnItem::None) {
        item->setStatus(ColumnItem::Changed);
    }

    item->changeColumn(column);
    emit dataChanged(index, index, {Qt::FontRole, Qt::DisplayRole});

    return true;
}

QModelIndex FiltersColumnModel::index(int row, int column, const QModelIndex& parent) const
{
    if(!hasIndex(row, column, parent)) {
        return {};
    }

    ColumnItem* item = m_root.child(row);

    return createIndex(row, column, item);
}

int FiltersColumnModel::rowCount(const QModelIndex& /*parent*/) const
{
    return m_root.childCount();
}

int FiltersColumnModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 3;
}

bool FiltersColumnModel::removeRows(int row, int count, const QModelIndex& /*parent*/)
{
    for(int i{row}; i < row + count; ++i) {
        const QModelIndex& index = this->index(i, 0, {});

        if(!index.isValid()) {
            return false;
        }

        auto* item = static_cast<ColumnItem*>(index.internalPointer());
        if(item) {
            if(item->status() == ColumnItem::Added) {
                beginRemoveRows({}, i, i);
                m_root.removeChild(i);
                endRemoveRows();
                m_nodes.erase(item->column().index);
            }
            else if(!item->column().isDefault) {
                item->setStatus(ColumnItem::Removed);
            }
        }
    }

    invalidateData();

    return true;
}

void FiltersColumnModel::addPendingRow()
{
    const int index = static_cast<int>(m_nodes.size());

    FilterColumn column;
    column.index = index;

    ColumnItem* item = &m_nodes.emplace(index, ColumnItem{column, &m_root}).first->second;

    item->setStatus(ColumnItem::Added);

    const int row = m_root.childCount();
    beginInsertRows({}, row, row);
    m_root.appendChild(item);
    endInsertRows();
}

void FiltersColumnModel::removePendingRow()
{
    const int row = rowCount({}) - 1;
    beginRemoveRows({}, row, row);
    m_root.removeChild(row);
    endRemoveRows();
}
} // namespace Fooyin::Filters
