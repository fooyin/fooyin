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

#include <QAbstractItemModel>

namespace Utils {
template<class Item>
class TreeModel : public QAbstractItemModel
{
public:
    explicit TreeModel(QObject* parent = nullptr)
        : QAbstractItemModel{parent}
        , m_root{std::make_unique<Item>()}
    { }

    ~TreeModel() override = default;

    [[nodiscard]] Item* rootItem() const
    {
        return m_root.get();
    }

    void resetRoot()
    {
        m_root.reset();
        m_root = std::make_unique<Item>();
    }

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        if(!index.isValid()) {
            return Qt::NoItemFlags;
        }
        return QAbstractItemModel::flags(index);
    }

    [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex& parent) const override
    {
        if(!hasIndex(row, column, parent)) {
            return {};
        }

        Item* parentItem;

        if(!parent.isValid()) {
            parentItem = m_root.get();
        }
        else {
            parentItem = static_cast<Item*>(parent.internalPointer());
        }

        Item* childItem = parentItem->child(row);
        if(childItem) {
            return createIndex(row, column, childItem);
        }
        return {};
    }

    [[nodiscard]] QModelIndex parent(const QModelIndex& index) const override
    {
        if(!index.isValid()) {
            return {};
        }

        auto* childItem      = static_cast<Item*>(index.internalPointer());
        Item* parentItem = childItem->parent();

        if(parentItem == m_root.get()) {
            return {};
        }

        return createIndex(parentItem->row(), 0, parentItem);
    }

    [[nodiscard]] int rowCount(const QModelIndex& parent) const override
    {
        Item* parentItem;

        if(!parent.isValid()) {
            parentItem = m_root.get();
        }
        else {
            parentItem = static_cast<Item*>(parent.internalPointer());
        }

        return parentItem->childCount();
    }

    [[nodiscard]] int columnCount(const QModelIndex& parent) const override
    {
        if(parent.isValid()) {
            return static_cast<Item*>(parent.internalPointer())->columnCount();
        }
        return m_root->columnCount();
    }

    [[nodiscard]] QModelIndex indexOfItem(const Item* item)
    {
        if(item) {
            return createIndex(item->row(), 0, item);
        }
        return {};
    }

private:
    std::unique_ptr<Item> m_root;
};
} // namespace Utils
