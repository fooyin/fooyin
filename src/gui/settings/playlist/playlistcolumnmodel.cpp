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

#include "playlistcolumnmodel.h"

#include "playlist/playlistcolumn.h"
#include "playlist/playlistcolumnregistry.h"

#include <utils/treestatusitem.h>

using namespace Qt::Literals::StringLiterals;

namespace Fooyin {
class ColumnItem : public TreeStatusItem<ColumnItem>
{
public:
    ColumnItem()
        : ColumnItem{{}, nullptr}
    { }

    explicit ColumnItem(PlaylistColumn column, ColumnItem* parent)
        : TreeStatusItem{parent}
        , m_column{std::move(column)}
    { }

    [[nodiscard]] PlaylistColumn column() const
    {
        return m_column;
    }

    void changeColumn(const PlaylistColumn& column)
    {
        m_column = column;
    }

private:
    PlaylistColumn m_column;
};

struct PlaylistColumnModel::Private
{
    PlaylistColumnRegistry* columnsRegistry;
    ColumnItem root;
    std::map<int, ColumnItem> nodes;

    explicit Private(PlaylistColumnRegistry* columnsRegistry)
        : columnsRegistry{columnsRegistry}
    { }
};

PlaylistColumnModel::PlaylistColumnModel(PlaylistColumnRegistry* columnsRegistry, QObject* parent)
    : ExtendableTableModel{parent}
    , p{std::make_unique<Private>(columnsRegistry)}
{ }

PlaylistColumnModel::~PlaylistColumnModel() = default;

void PlaylistColumnModel::populate()
{
    beginResetModel();
    p->root = {};
    p->nodes.clear();

    const auto& columns = p->columnsRegistry->items();

    for(const auto& [index, column] : columns) {
        if(column.name.isEmpty()) {
            continue;
        }
        ColumnItem* child = &p->nodes.emplace(index, ColumnItem{column, &p->root}).first->second;
        p->root.insertChild(index, child);
    }

    endResetModel();
}

void PlaylistColumnModel::processQueue()
{
    std::vector<int> columnsToRemove;

    for(auto& [index, node] : p->nodes) {
        const ColumnItem::ItemStatus status = node.status();
        const PlaylistColumn column         = node.column();

        switch(status) {
            case(ColumnItem::Added): {
                if(column.field.isEmpty()) {
                    break;
                }

                const PlaylistColumn addedColumn = p->columnsRegistry->addItem(column);
                if(addedColumn.isValid()) {
                    node.changeColumn(addedColumn);
                    node.setStatus(ColumnItem::None);

                    emit dataChanged({}, {});
                }
                else {
                    qWarning() << "Column " + column.name + " could not be added";
                }
                break;
            }
            case(ColumnItem::Removed): {
                if(p->columnsRegistry->removeByIndex(column.index)) {
                    beginRemoveRows({}, node.row(), node.row());
                    p->root.removeChild(node.row());
                    endRemoveRows();
                    columnsToRemove.push_back(index);
                }
                else {
                    qWarning() << "Column " + column.name + " could not be removed";
                }
                break;
            }
            case(ColumnItem::Changed): {
                if(p->columnsRegistry->changeItem(column)) {
                    node.changeColumn(p->columnsRegistry->itemById(column.id));
                    node.setStatus(ColumnItem::None);

                    emit dataChanged({}, {});
                }
                else {
                    qWarning() << "Column " + column.name + " could not be changed";
                }
                break;
            }
            case(ColumnItem::None):
                break;
        }
    }
    for(const auto& index : columnsToRemove) {
        p->nodes.erase(index);
    }
}

Qt::ItemFlags PlaylistColumnModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }

    auto flags = ExtendableTableModel::flags(index);
    flags |= Qt::ItemIsEditable;

    return flags;
}

QVariant PlaylistColumnModel::headerData(int section, Qt::Orientation orientation, int role) const
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
    }
    return {};
}

QVariant PlaylistColumnModel::data(const QModelIndex& index, int role) const
{
    if(role != Qt::DisplayRole && role != Qt::EditRole && role != Qt::FontRole) {
        return {};
    }

    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item = static_cast<ColumnItem*>(index.internalPointer());

    if(role == Qt::FontRole) {
        return item->font();
    }

    switch(index.column()) {
        case(0):
            return item->column().index;
        case(1): {
            const QString& name = item->column().name;
            return !name.isEmpty() ? name : QStringLiteral("<enter name here>");
        }
        case(2): {
            const QString& field = item->column().field;
            return !field.isEmpty() ? field : QStringLiteral("<enter field here>");
        }
    }
    return {};
}

bool PlaylistColumnModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(role != Qt::EditRole) {
        return false;
    }

    auto* item            = static_cast<ColumnItem*>(index.internalPointer());
    PlaylistColumn column = item->column();

    switch(index.column()) {
        case(1): {
            if(value.toString() == "<enter name here>"_L1 || column.name == value.toString()) {
                if(item->status() == ColumnItem::Added) {
                    emit pendingRowCancelled();
                }
                return false;
            }
            column.name = value.toString();

            emit pendingRowAdded();
            break;
        }
        case(2): {
            if(column.field == value.toString()) {
                return false;
            }
            column.field = value.toString();
            break;
        }
        case(0):
            break;
    }

    if(item->status() == ColumnItem::None) {
        item->setStatus(ColumnItem::Changed);
    }

    item->changeColumn(column);
    emit dataChanged(index, index, {Qt::FontRole, Qt::DisplayRole});

    return true;
}

QModelIndex PlaylistColumnModel::index(int row, int column, const QModelIndex& parent) const
{
    if(!hasIndex(row, column, parent)) {
        return {};
    }

    ColumnItem* item = p->root.child(row);

    return createIndex(row, column, item);
}

int PlaylistColumnModel::rowCount(const QModelIndex& /*parent*/) const
{
    return p->root.childCount();
}

int PlaylistColumnModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 3;
}

bool PlaylistColumnModel::removeRows(int row, int count, const QModelIndex& /*parent*/)
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
                p->root.removeChild(i);
                endRemoveRows();
                p->nodes.erase(item->column().index);
            }
            else {
                item->setStatus(ColumnItem::Removed);
                emit dataChanged({}, {}, {Qt::FontRole});
            }
        }
    }
    return true;
}

void PlaylistColumnModel::addPendingRow()
{
    const int index = static_cast<int>(p->nodes.size());

    PlaylistColumn column;
    column.index = index;

    ColumnItem* item = &p->nodes.emplace(index, ColumnItem{column, &p->root}).first->second;

    item->setStatus(ColumnItem::Added);

    const int row = p->root.childCount();
    beginInsertRows({}, row, row);
    p->root.appendChild(item);
    endInsertRows();

    emit newPendingRow();
}

void PlaylistColumnModel::removePendingRow()
{
    const int row = rowCount({}) - 1;
    beginRemoveRows({}, row, row);
    p->root.removeChild(row);
    endRemoveRows();
}

} // namespace Fooyin
