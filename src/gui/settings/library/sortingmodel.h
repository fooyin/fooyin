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

#pragma once

#include <core/library/librarysort.h>
#include <utils/extendabletableview.h>
#include <utils/treestatusitem.h>

#include <QObject>

namespace Fooyin {
class SortingRegistry;

class SortingItem : public TreeStatusItem<SortingItem>
{
public:
    SortingItem();
    explicit SortingItem(SortScript sortScript, SortingItem* parent);

    [[nodiscard]] SortScript sortScript() const;
    void changeSort(SortScript sortScript);

private:
    SortScript m_sortScript;
};

class SortingModel : public ExtendableTableModel
{
    Q_OBJECT

public:
    explicit SortingModel(SortingRegistry* sortRegistry, QObject* parent = nullptr);

    void populate();
    void processQueue();

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

    [[nodiscard]] bool removeRows(int row, int count, const QModelIndex& parent) override;

    void addPendingRow() override;
    void removePendingRow() override;

private:
    SortingRegistry* m_sortRegistry;
    std::map<int, SortingItem> m_nodes;
    SortingItem m_root;
};
} // namespace Fooyin
