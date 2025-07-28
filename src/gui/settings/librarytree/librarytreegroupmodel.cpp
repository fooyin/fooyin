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

#include "librarytreegroupmodel.h"

#include "librarytree/librarytreegroupregistry.h"

#include <utils/treestatusitem.h>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(LIBTREE_MOD, "fy.libtreegroupmodel")

using namespace Qt::StringLiterals;

namespace Fooyin {
LibraryTreeGroupItem::LibraryTreeGroupItem()
    : LibraryTreeGroupItem{{}, nullptr}
{ }

LibraryTreeGroupItem::LibraryTreeGroupItem(LibraryTreeGrouping group, LibraryTreeGroupItem* parent)
    : TreeStatusItem{parent}
    , m_group{std::move(group)}
{ }

LibraryTreeGrouping LibraryTreeGroupItem::group() const
{
    return m_group;
}

void LibraryTreeGroupItem::changeGroup(const LibraryTreeGrouping& group)
{
    m_group = group;
}

LibraryTreeGroupModel::LibraryTreeGroupModel(LibraryTreeGroupRegistry* groupsRegistry, QObject* parent)
    : ExtendableTableModel{parent}
    , m_groupsRegistry{groupsRegistry}
{ }

void LibraryTreeGroupModel::populate()
{
    beginResetModel();
    m_root = {};
    m_nodes.clear();

    const auto& groups = m_groupsRegistry->items();

    for(const auto& group : groups) {
        if(!group.isValid()) {
            continue;
        }
        LibraryTreeGroupItem* child = &m_nodes.emplace(group.index, LibraryTreeGroupItem{group, &m_root}).first->second;
        m_root.appendChild(child);
    }

    endResetModel();
}

void LibraryTreeGroupModel::processQueue()
{
    std::vector<LibraryTreeGroupItem> groupsToRemove;

    for(auto& [index, node] : m_nodes) {
        const LibraryTreeGroupItem::ItemStatus status = node.status();
        const LibraryTreeGrouping group               = node.group();

        switch(status) {
            case(LibraryTreeGroupItem::Added): {
                if(group.script.isEmpty()) {
                    break;
                }

                const auto addedField = m_groupsRegistry->addItem(group);
                if(addedField.isValid()) {
                    node.changeGroup(addedField);
                    node.setStatus(LibraryTreeGroupItem::None);
                }
                else {
                    qCWarning(LIBTREE_MOD) << "Group could not be added:" << group.name;
                }
                break;
            }
            case(LibraryTreeGroupItem::Removed): {
                if(m_groupsRegistry->removeById(group.id)) {
                    beginRemoveRows({}, node.row(), node.row());
                    m_root.removeChild(node.row());
                    endRemoveRows();
                    groupsToRemove.push_back(node);
                }
                else {
                    qCWarning(LIBTREE_MOD) << "Group could not be removed:" << group.name;
                }
                break;
            }
            case(LibraryTreeGroupItem::Changed): {
                if(m_groupsRegistry->changeItem(group)) {
                    if(const auto updatedGroup = m_groupsRegistry->itemById(group.id)) {
                        node.changeGroup(updatedGroup.value());
                        node.setStatus(LibraryTreeGroupItem::None);
                    }
                }
                else {
                    qCWarning(LIBTREE_MOD) << "Group could not be changed:" << group.name;
                }
                break;
            }
            case(LibraryTreeGroupItem::None):
                break;
        }
    }

    for(const auto& item : groupsToRemove) {
        m_nodes.erase(item.group().index);
    }

    invalidateData();
}

Qt::ItemFlags LibraryTreeGroupModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }

    auto flags = ExtendableTableModel::flags(index);

    auto* item = static_cast<LibraryTreeGroupItem*>(index.internalPointer());
    if(item && !item->group().isDefault) {
        flags |= Qt::ItemIsEditable;
    }

    return flags;
}

QVariant LibraryTreeGroupModel::headerData(int section, Qt::Orientation orientation, int role) const
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
            return tr("Grouping");
        default:
            break;
    }

    return {};
}

QVariant LibraryTreeGroupModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item = static_cast<LibraryTreeGroupItem*>(index.internalPointer());

    if(role == Qt::FontRole) {
        return item->font();
    }

    if(role == Qt::UserRole) {
        return QVariant::fromValue(item->group());
    }

    if(role == Qt::DisplayRole || role == Qt::EditRole) {
        switch(index.column()) {
            case(0):
                return item->group().index;
            case(1): {
                const QString& name = item->group().name;
                return !name.isEmpty() ? name : u"<enter name here>"_s;
            }
            case(2): {
                const QString& field = item->group().script;
                return !field.isEmpty() ? field : u"<enter grouping here>"_s;
            }
            default:
                break;
        }
    }

    return {};
}

bool LibraryTreeGroupModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(role != Qt::EditRole) {
        return false;
    }

    auto* item = static_cast<LibraryTreeGroupItem*>(index.internalPointer());
    auto group = item->group();

    switch(index.column()) {
        case(1): {
            if(value.toString() == u"<enter name here>"_s || group.name == value.toString()) {
                if(item->status() == LibraryTreeGroupItem::Added) {
                    emit pendingRowCancelled();
                }
                return false;
            }
            group.name = value.toString();
            break;
        }
        case(2): {
            if(group.script == value.toString()) {
                return false;
            }
            group.script = value.toString();
            break;
        }
        default:
            break;
    }

    if(item->status() == LibraryTreeGroupItem::None) {
        item->setStatus(LibraryTreeGroupItem::Changed);
    }

    item->changeGroup(group);
    emit dataChanged(index, index.siblingAtColumn(columnCount({}) - 1), {Qt::FontRole, Qt::DisplayRole});

    return true;
}

QModelIndex LibraryTreeGroupModel::index(int row, int column, const QModelIndex& parent) const
{
    if(!hasIndex(row, column, parent)) {
        return {};
    }

    LibraryTreeGroupItem* item = m_root.child(row);

    return createIndex(row, column, item);
}

int LibraryTreeGroupModel::rowCount(const QModelIndex& /*parent*/) const
{
    return m_root.childCount();
}

int LibraryTreeGroupModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 3;
}

bool LibraryTreeGroupModel::removeRows(int row, int count, const QModelIndex& /*parent*/)
{
    for(int i{row}; i < row + count; ++i) {
        const QModelIndex& index = this->index(i, 0, {});

        if(!index.isValid()) {
            return false;
        }

        auto* item = static_cast<LibraryTreeGroupItem*>(index.internalPointer());
        if(item) {
            if(item->status() == LibraryTreeGroupItem::Added) {
                beginRemoveRows({}, i, i);
                m_root.removeChild(i);
                endRemoveRows();
                m_nodes.erase(item->group().index);
            }
            else if(!item->group().isDefault) {
                item->setStatus(LibraryTreeGroupItem::Removed);
            }
        }
    }

    invalidateData();

    return true;
}

void LibraryTreeGroupModel::addPendingRow()
{
    const int index = static_cast<int>(m_nodes.size());

    LibraryTreeGrouping group;
    group.index = index;

    LibraryTreeGroupItem* item = &m_nodes.emplace(index, LibraryTreeGroupItem{group, &m_root}).first->second;

    item->setStatus(LibraryTreeGroupItem::Added);

    const int row = m_root.childCount();
    beginInsertRows({}, row, row);
    m_root.appendChild(item);
    endInsertRows();
}

void LibraryTreeGroupModel::removePendingRow()
{
    const int row = rowCount({}) - 1;
    beginRemoveRows({}, row, row);
    m_root.removeChild(row);
    endRemoveRows();
}
} // namespace Fooyin
