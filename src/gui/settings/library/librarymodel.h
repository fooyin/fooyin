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

namespace Fy {
namespace Core::Library {
class LibraryManager;
} // namespace Core::Library

namespace Gui::Settings {
class LibraryItem : public Utils::TreeStatusItem<LibraryItem>
{
public:
    LibraryItem();
    explicit LibraryItem(Core::Library::LibraryInfo info, LibraryItem* parent);

    [[nodiscard]] Core::Library::LibraryInfo info() const;
    void changeInfo(const Core::Library::LibraryInfo& info);

private:
    Core::Library::LibraryInfo m_info;
};

class LibraryModel : public Utils::TableModel<LibraryItem>
{
public:
    explicit LibraryModel(Core::Library::LibraryManager* libraryManager, QObject* parent = nullptr);

    void setupModelData();
    void markForAddition(const Core::Library::LibraryInfo& info);
    void markForRemoval(const Core::Library::LibraryInfo& info);
    void markForChange(const Core::Library::LibraryInfo& info);
    void processQueue();

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

private:
    void updateDisplay(const Core::Library::LibraryInfo& info);

    Core::Library::LibraryManager* m_libraryManager;

    using LibraryPathMap    = std::unordered_map<QString, LibraryItem>;
    using LibraryInfoList = std::vector<Core::Library::LibraryInfo>;

    LibraryPathMap m_nodes;
    LibraryInfoList m_librariesToAdd;
};
} // namespace Gui::Settings
} // namespace Fy
