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

#include <core/library/libraryinfo.h>
#include <gui/widgets/extendabletableview.h>
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

class LibraryModel : public ExtendableTableModel
{
    Q_OBJECT

public:
    explicit LibraryModel(LibraryManager* libraryManager, QObject* parent = nullptr);

    void populate();
    void markForAddition(const LibraryInfo& info);
    void processQueue();

    void invalidateData();

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

    [[nodiscard]] bool removeRows(int row, int count, const QModelIndex& parent) override;

    void addPendingRow() override;
    void removePendingRow() override;

signals:
    void requestAddLibrary();

private:
    LibraryManager* m_libraryManager;

    LibraryItem m_root;
    std::unordered_map<QString, LibraryItem> m_nodes;
    std::vector<LibraryInfo> m_librariesToAdd;
};
} // namespace Fooyin
