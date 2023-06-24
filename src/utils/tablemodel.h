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

namespace Fy::Utils {
class SimpleTableModel : public QAbstractItemModel
{
public:
    explicit SimpleTableModel(QObject* parent = nullptr)
        : QAbstractItemModel{parent}
    { }

    [[nodiscard]] virtual Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        if(!index.isValid()) {
            return Qt::NoItemFlags;
        }
        return QAbstractItemModel::flags(index);
    }

    [[nodiscard]] virtual QModelIndex index(int row, int column, const QModelIndex& /*parent*/) const override
    {
        return createIndex(row, column, nullptr);
    }

    [[nodiscard]] virtual QModelIndex parent(const QModelIndex& /*index*/) const override
    {
        return {};
    }
};

template <class Item>
class TableModel : public SimpleTableModel
{
public:
    explicit TableModel(QObject* parent = nullptr)
        : SimpleTableModel{parent}
        , m_root{}
    { }

    [[nodiscard]] virtual Item* rootItem() const
    {
        return &m_root;
    }

    virtual void resetRoot()
    {
        m_root = Item{};
    }

    [[nodiscard]] virtual Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        if(!index.isValid()) {
            return Qt::NoItemFlags;
        }
        return QAbstractItemModel::flags(index);
    }

    [[nodiscard]] virtual QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override
    {
        if(!hasIndex(row, column, parent)) {
            return {};
        }

        Item* childItem = m_root.child(row);
        if(childItem) {
            return createIndex(row, column, childItem);
        }
        return {};
    }

    [[nodiscard]] virtual QModelIndex parent(const QModelIndex& index) const override
    {
        Q_UNUSED(index)
        return {};
    }

    [[nodiscard]] virtual int rowCount(const QModelIndex& parent = {}) const override
    {
        Q_UNUSED(parent)
        return m_root.childCount();
    }

    [[nodiscard]] virtual QModelIndex indexOfItem(const Item* item)
    {
        if(item) {
            return createIndex(item->row(), 0, item);
        }
        return {};
    }

private:
    mutable Item m_root;
};
} // namespace Fy::Utils
