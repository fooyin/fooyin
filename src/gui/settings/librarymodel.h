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

#include "libraryitem.h"

#include <utils/tablemodel.h>

#include <deque>

namespace Fy {

namespace Core::Library {
class LibraryManager;
} // namespace Core::Library

namespace Gui::Settings {
class LibraryModel : public Utils::TableModel<LibraryItem>
{
public:
    explicit LibraryModel(Core::Library::LibraryManager* libraryManager, QObject* parent = nullptr);

    void setupModelData();
    void markForAddition(const Core::Library::LibraryInfo& info);
    void markForRemoval(Core::Library::LibraryInfo* info);
    void markForRename(Core::Library::LibraryInfo* info);
    void processQueue();

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

private:
    enum OperationType
    {
        Add    = 1,
        Remove = 2,
        Rename = 3
    };

    struct QueueEntry
    {
        QueueEntry() = default;
        QueueEntry(OperationType type, Core::Library::LibraryInfo* info)
            : type{type}
            , info{info}
        { }
        OperationType type;
        Core::Library::LibraryInfo* info;

        bool operator==(const QueueEntry& other) const
        {
            return std::tie(type, info) == std::tie(other.type, other.info);
        }
    };

    [[nodiscard]] bool findInQueue(const QString& id, OperationType op, QueueEntry* library = nullptr) const;
    void removeFromQueue(const QueueEntry& libraryToDelete);

    Core::Library::LibraryManager* m_libraryManager;

    using IdLibraryMap    = std::unordered_map<QString, std::unique_ptr<LibraryItem>>;
    using LibaryQueueMap  = std::deque<QueueEntry>;
    using LibraryInfoList = std::vector<std::unique_ptr<Core::Library::LibraryInfo>>;

    IdLibraryMap m_nodes;
    LibaryQueueMap m_queue;
    LibraryInfoList m_librariesToAdd;
};
} // namespace Gui::Settings
} // namespace Fy
