/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "treemodel.h"

namespace Utils {
TreeModel::TreeModel(QObject* parent)
    : QAbstractItemModel{parent}
    , m_root{std::make_unique<TreeItem>()}
{ }

TreeItem* TreeModel::rootItem() const
{
    return m_root.get();
}

void TreeModel::resetRoot()
{
    m_root.reset();
    m_root = std::make_unique<TreeItem>();
}

Qt::ItemFlags TreeModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return QAbstractItemModel::flags(index);
}

QModelIndex TreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if(!hasIndex(row, column, parent)) {
        return {};
    }

    TreeItem* parentItem;

    if(!parent.isValid()) {
        parentItem = m_root.get();
    }
    else {
        parentItem = static_cast<TreeItem*>(parent.internalPointer());
    }

    TreeItem* childItem = parentItem->child(row);
    if(childItem) {
        return createIndex(row, column, childItem);
    }
    return {};
}

QModelIndex TreeModel::parent(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return {};
    }

    auto* childItem      = static_cast<TreeItem*>(index.internalPointer());
    TreeItem* parentItem = childItem->parent();

    if(parentItem == m_root.get()) {
        return {};
    }

    return createIndex(parentItem->row(), 0, parentItem);
}

int TreeModel::rowCount(const QModelIndex& parent) const
{
    TreeItem* parentItem;

    if(!parent.isValid()) {
        parentItem = m_root.get();
    }
    else {
        parentItem = static_cast<TreeItem*>(parent.internalPointer());
    }

    return parentItem->childCount();
}

int TreeModel::columnCount(const QModelIndex& parent) const
{
    if(parent.isValid()) {
        return static_cast<TreeItem*>(parent.internalPointer())->columnCount();
    }
    return m_root->columnCount();
}

QModelIndex TreeModel::indexOfItem(const TreeItem* item)
{
    if(item) {
        return createIndex(item->row(), 0, item);
    }
    return {};
}
} // namespace Utils
