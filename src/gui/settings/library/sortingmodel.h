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

#pragma once

#include <core/library/librarysort.h>

#include <utils/tablemodel.h>
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

class SortingModel : public TableModel<SortingItem>
{
public:
    explicit SortingModel(SortingRegistry* sortRegistry, QObject* parent = nullptr);

    void populate();
    void addNewSortScript();
    void markForRemoval(const SortScript& sortScript);
    void markForChange(const SortScript& sortScript);
    void processQueue();

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

private:
    void reset();
    void removeSortScript(int index);

    using SortIndexMap = std::map<int, SortingItem>;

    SortingRegistry* m_sortRegistry;

    SortIndexMap m_nodes;
};
} // namespace Fooyin
