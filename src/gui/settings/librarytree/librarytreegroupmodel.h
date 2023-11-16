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

#include "librarytree/librarytreegroup.h"

#include <utils/tablemodel.h>
#include <utils/treestatusitem.h>

namespace Fooyin {
class LibraryTreeGroupRegistry;

class LibraryTreeGroupItem : public TreeStatusItem<LibraryTreeGroupItem>
{
public:
    LibraryTreeGroupItem();
    explicit LibraryTreeGroupItem(LibraryTreeGrouping group, LibraryTreeGroupItem* parent);

    [[nodiscard]] LibraryTreeGrouping group() const;
    void changeGroup(const LibraryTreeGrouping& group);

private:
    LibraryTreeGrouping m_group;
};

class LibraryTreeGroupModel : public TableModel<LibraryTreeGroupItem>
{
public:
    explicit LibraryTreeGroupModel(LibraryTreeGroupRegistry* groupsRegistry, QObject* parent = nullptr);

    void populate();
    void addNewGroup();
    void markForRemoval(const LibraryTreeGrouping& group);
    void markForChange(const LibraryTreeGrouping& group);
    void processQueue();

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

private:
    void reset();
    void removeGroup(int index);

    using IndexGroupMap = std::map<int, LibraryTreeGroupItem>;

    LibraryTreeGroupRegistry* m_groupsRegistry;

    IndexGroupMap m_nodes;
};
} // namespace Fooyin
