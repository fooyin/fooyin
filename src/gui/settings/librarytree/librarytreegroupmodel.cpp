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

#include "librarytreegroupmodel.h"

#include "librarytree/librarytreegroupregistry.h"

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
    : TableModel{parent}
    , m_groupsRegistry{groupsRegistry}
{ }

void LibraryTreeGroupModel::populate()
{
    beginResetModel();
    reset();

    const auto& groups = m_groupsRegistry->items();

    for(const auto& [index, group] : groups) {
        if(!group.isValid()) {
            continue;
        }
        LibraryTreeGroupItem* parent = rootItem();
        LibraryTreeGroupItem* child  = &m_nodes.emplace(index, LibraryTreeGroupItem{group, parent}).first->second;
        parent->appendChild(child);
    }

    endResetModel();
}

void LibraryTreeGroupModel::addNewGroup()
{
    const int index = static_cast<int>(m_nodes.size());

    LibraryTreeGrouping group;
    group.index = index;

    auto* parent = rootItem();

    LibraryTreeGroupItem* item = &m_nodes.emplace(index, LibraryTreeGroupItem{group, parent}).first->second;

    item->setStatus(LibraryTreeGroupItem::Added);

    const int row = parent->childCount();
    beginInsertRows({}, row, row);
    parent->appendChild(item);
    endInsertRows();
}

void LibraryTreeGroupModel::markForRemoval(const LibraryTreeGrouping& group)
{
    LibraryTreeGroupItem* item = &m_nodes.at(group.index);

    if(item->status() == LibraryTreeGroupItem::Added) {
        beginRemoveRows({}, item->row(), item->row());
        rootItem()->removeChild(item->row());
        endRemoveRows();

        removeGroup(group.index);
    }
    else {
        item->setStatus(LibraryTreeGroupItem::Removed);
        emit dataChanged({}, {}, {Qt::FontRole});
    }
}

void LibraryTreeGroupModel::markForChange(const LibraryTreeGrouping& group)
{
    LibraryTreeGroupItem* item = &m_nodes.at(group.index);
    item->changeGroup(group);
    emit dataChanged({}, {}, {Qt::FontRole});

    if(item->status() == LibraryTreeGroupItem::None) {
        item->setStatus(LibraryTreeGroupItem::Changed);
    }
}

void LibraryTreeGroupModel::processQueue()
{
    std::vector<LibraryTreeGroupItem> groupsToRemove;

    for(auto& [index, node] : m_nodes) {
        const LibraryTreeGroupItem::ItemStatus status = node.status();
        const LibraryTreeGrouping group               = node.group();

        switch(status) {
            case(LibraryTreeGroupItem::Added): {
                const auto addedField = m_groupsRegistry->addItem(group);
                if(addedField.isValid()) {
                    node.changeGroup(addedField);
                    node.setStatus(LibraryTreeGroupItem::None);

                    emit dataChanged({}, {});
                }
                else {
                    qWarning() << "Group " + group.name + " could not be added";
                }
                break;
            }
            case(LibraryTreeGroupItem::Removed): {
                if(m_groupsRegistry->removeByIndex(group.index)) {
                    beginRemoveRows({}, node.row(), node.row());
                    rootItem()->removeChild(node.row());
                    endRemoveRows();
                    groupsToRemove.push_back(node);
                }
                else {
                    qWarning() << "Group " + group.name + " could not be removed";
                }
                break;
            }
            case(LibraryTreeGroupItem::Changed): {
                if(m_groupsRegistry->changeItem(group)) {
                    node.changeGroup(m_groupsRegistry->itemById(group.id));
                    node.setStatus(LibraryTreeGroupItem::None);

                    emit dataChanged({}, {});
                }
                else {
                    qWarning() << "Group " + group.name + " could not be changed";
                }
                break;
            }
            case(LibraryTreeGroupItem::None):
                break;
        }
    }
    for(const auto& item : groupsToRemove) {
        removeGroup(item.group().index);
    }
}

Qt::ItemFlags LibraryTreeGroupModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }

    auto flags = TableModel::flags(index);
    flags |= Qt::ItemIsEditable;

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
            return QStringLiteral("Index");
        case(1):
            return QStringLiteral("Name");
        case(2):
            return QStringLiteral("Grouping");
    }
    return {};
}

QVariant LibraryTreeGroupModel::data(const QModelIndex& index, int role) const
{
    if(role != Qt::DisplayRole && role != Qt::EditRole && role != Qt::FontRole) {
        return {};
    }

    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item = static_cast<LibraryTreeGroupItem*>(index.internalPointer());

    if(role == Qt::FontRole) {
        return item->font();
    }

    switch(index.column()) {
        case(0):
            return item->group().index;
        case(1): {
            const QString& name = item->group().name;
            return !name.isEmpty() ? name : QStringLiteral("<enter name here>");
        }
        case(2): {
            const QString& field = item->group().script;
            return !field.isEmpty() ? field : QStringLiteral("<enter grouping here>");
        }
    }
    return {};
}

bool LibraryTreeGroupModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(role != Qt::EditRole) {
        return false;
    }

    const auto* item = static_cast<LibraryTreeGroupItem*>(index.internalPointer());
    auto group       = item->group();

    switch(index.column()) {
        case(1): {
            if(group.name == value.toString()) {
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
        case(0):
            break;
    }

    markForChange(group);

    return true;
}

int LibraryTreeGroupModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 3;
}

void LibraryTreeGroupModel::reset()
{
    resetRoot();
    m_nodes.clear();
}

void LibraryTreeGroupModel::removeGroup(int index)
{
    if(!m_nodes.contains(index)) {
        return;
    }
    m_nodes.erase(index);
}
} // namespace Fooyin
