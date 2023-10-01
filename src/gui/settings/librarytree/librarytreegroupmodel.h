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

#include "gui/librarytree/librarytreegroup.h"

#include <utils/tablemodel.h>
#include <utils/treestatusitem.h>

namespace Fy::Gui {
namespace Widgets {
class LibraryTreeGroupRegistry;
}

namespace Settings {
class LibraryTreeGroupItem : public Utils::TreeStatusItem<LibraryTreeGroupItem>
{
public:
    LibraryTreeGroupItem();
    explicit LibraryTreeGroupItem(Widgets::LibraryTreeGrouping group, LibraryTreeGroupItem* parent);

    [[nodiscard]] Widgets::LibraryTreeGrouping group() const;
    void changeGroup(const Widgets::LibraryTreeGrouping& group);

private:
    Widgets::LibraryTreeGrouping m_group;
};

class LibraryTreeGroupModel : public Utils::TableModel<LibraryTreeGroupItem>
{
public:
    explicit LibraryTreeGroupModel(Widgets::LibraryTreeGroupRegistry* groupsRegistry, QObject* parent = nullptr);

    void populate();
    void addNewGroup();
    void markForRemoval(const Widgets::LibraryTreeGrouping& group);
    void markForChange(const Widgets::LibraryTreeGrouping& group);
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

    Widgets::LibraryTreeGroupRegistry* m_groupsRegistry;

    IndexGroupMap m_nodes;
};
} // namespace Settings
} // namespace Fy::Gui
