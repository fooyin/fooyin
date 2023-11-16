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

#include <core/library/libraryinfo.h>
#include <utils/tablemodel.h>
#include <utils/treestatusitem.h>

namespace Fooyin {
class LibraryManager;

class LibraryItem : public TreeStatusItem<LibraryItem>
{
public:
    LibraryItem();
    explicit LibraryItem(LibraryInfo info, LibraryItem* parent);

    [[nodiscard]] LibraryInfo info() const;
    void changeInfo(const LibraryInfo& info);

private:
    LibraryInfo m_info;
};

class LibraryModel : public TableModel<LibraryItem>
{
public:
    explicit LibraryModel(LibraryManager* libraryManager, QObject* parent = nullptr);

    void populate();
    void markForAddition(const LibraryInfo& info);
    void markForRemoval(const LibraryInfo& info);
    void markForChange(const LibraryInfo& info);
    void processQueue();

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

private:
    void reset();
    void updateDisplay(const LibraryInfo& info);

    LibraryManager* m_libraryManager;

    using LibraryPathMap  = std::unordered_map<QString, LibraryItem>;
    using LibraryInfoList = std::vector<LibraryInfo>;

    LibraryPathMap m_nodes;
    LibraryInfoList m_librariesToAdd;
};
} // namespace Fooyin
