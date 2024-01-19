/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

using namespace Qt::Literals::StringLiterals;

namespace Fooyin {
class LibraryTreeGroupItem : public TreeStatusItem<LibraryTreeGroupItem>
{
public:
    LibraryTreeGroupItem()
        : LibraryTreeGroupItem{{}, nullptr}
    { }

    explicit LibraryTreeGroupItem(LibraryTreeGrouping group, LibraryTreeGroupItem* parent)
        : TreeStatusItem{parent}
        , m_group{std::move(group)}
    { }

    [[nodiscard]] LibraryTreeGrouping group() const
    {
        return m_group;
    }

    void changeGroup(const LibraryTreeGrouping& group)
    {
        m_group = group;
    }

private:
    LibraryTreeGrouping m_group;
};

struct LibraryTreeGroupModel::Private
{
    LibraryTreeGroupRegistry* groupsRegistry;
    std::map<int, LibraryTreeGroupItem> nodes;
    LibraryTreeGroupItem root;

    explicit Private(LibraryTreeGroupRegistry* groupsRegistry)
        : groupsRegistry{groupsRegistry}
    { }
};

LibraryTreeGroupModel::LibraryTreeGroupModel(LibraryTreeGroupRegistry* groupsRegistry, QObject* parent)
    : ExtendableTableModel{parent}
    , p{std::make_unique<Private>(groupsRegistry)}
{ }

LibraryTreeGroupModel::~LibraryTreeGroupModel() = default;

void LibraryTreeGroupModel::populate()
{
    beginResetModel();
    p->root = {};
    p->nodes.clear();

    const auto& groups = p->groupsRegistry->items();

    for(const auto& group : groups) {
        if(!group.isValid()) {
            continue;
        }
        LibraryTreeGroupItem* child
            = &p->nodes.emplace(group.index, LibraryTreeGroupItem{group, &p->root}).first->second;
        p->root.appendChild(child);
    }

    endResetModel();
}

void LibraryTreeGroupModel::processQueue()
{
    std::vector<LibraryTreeGroupItem> groupsToRemove;

    for(auto& [index, node] : p->nodes) {
        const LibraryTreeGroupItem::ItemStatus status = node.status();
        const LibraryTreeGrouping group               = node.group();

        switch(status) {
            case(LibraryTreeGroupItem::Added): {
                if(group.script.isEmpty()) {
                    break;
                }

                const auto addedField = p->groupsRegistry->addItem(group);
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
                if(p->groupsRegistry->removeByIndex(group.index)) {
                    beginRemoveRows({}, node.row(), node.row());
                    p->root.removeChild(node.row());
                    endRemoveRows();
                    groupsToRemove.push_back(node);
                }
                else {
                    qWarning() << "Group " + group.name + " could not be removed";
                }
                break;
            }
            case(LibraryTreeGroupItem::Changed): {
                if(p->groupsRegistry->changeItem(group)) {
                    node.changeGroup(p->groupsRegistry->itemById(group.id));
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
        p->nodes.erase(item.group().index);
    }
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
                return !name.isEmpty() ? name : QStringLiteral("<enter name here>");
            }
            case(2): {
                const QString& field = item->group().script;
                return !field.isEmpty() ? field : QStringLiteral("<enter grouping here>");
            }
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
            if(value.toString() == "<enter name here>"_L1 || group.name == value.toString()) {
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
        case(0):
            break;
    }

    if(item->status() == LibraryTreeGroupItem::None) {
        item->setStatus(LibraryTreeGroupItem::Changed);
    }

    item->changeGroup(group);
    emit dataChanged({}, {}, {Qt::FontRole, Qt::DisplayRole});

    return true;
}

QModelIndex LibraryTreeGroupModel::index(int row, int column, const QModelIndex& parent) const
{
    if(!hasIndex(row, column, parent)) {
        return {};
    }

    LibraryTreeGroupItem* item = p->root.child(row);

    return createIndex(row, column, item);
}

int LibraryTreeGroupModel::rowCount(const QModelIndex& /*parent*/) const
{
    return p->root.childCount();
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
                p->root.removeChild(i);
                endRemoveRows();
                p->nodes.erase(item->group().index);
            }
            else if(!item->group().isDefault) {
                item->setStatus(LibraryTreeGroupItem::Removed);
                emit dataChanged({}, {}, {Qt::FontRole});
            }
        }
    }
    return true;
}

void LibraryTreeGroupModel::addPendingRow()
{
    const int index = static_cast<int>(p->nodes.size());

    LibraryTreeGrouping group;
    group.index = index;

    LibraryTreeGroupItem* item = &p->nodes.emplace(index, LibraryTreeGroupItem{group, &p->root}).first->second;

    item->setStatus(LibraryTreeGroupItem::Added);

    const int row = p->root.childCount();
    beginInsertRows({}, row, row);
    p->root.appendChild(item);
    endInsertRows();
}

void LibraryTreeGroupModel::removePendingRow()
{
    const int row = rowCount({}) - 1;
    beginRemoveRows({}, row, row);
    p->root.removeChild(row);
    endRemoveRows();
}
} // namespace Fooyin
