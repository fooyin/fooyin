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

#pragma once

#include "treemodel.h"

namespace Fooyin {
template <class Item>
class TableModel : public TreeModel<Item>
{
public:
    using TreeModel<Item>::rootItem;

    explicit TableModel(QObject* parent = nullptr)
        : TreeModel<Item>{parent}
    { }

    [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override
    {
        if(!QAbstractItemModel::hasIndex(row, column, parent)) {
            return {};
        }

        Item* childItem = rootItem()->child(row);
        if(childItem) {
            return QAbstractItemModel::createIndex(row, column, childItem);
        }
        return {};
    }

    [[nodiscard]] QModelIndex parent(const QModelIndex& /*index*/) const override
    {
        return {};
    }

    [[nodiscard]] int rowCount(const QModelIndex& /*parent*/ = {}) const override
    {
        return rootItem()->childCount();
    }
};
} // namespace Fooyin
