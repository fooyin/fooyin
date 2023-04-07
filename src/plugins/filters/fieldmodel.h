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

#include "fielditem.h"

#include <utils/tablemodel.h>

namespace Fy::Filters {
class FieldRegistry;

namespace Settings {
class FieldModel : public Utils::TableModel<FieldItem>
{
public:
    explicit FieldModel(FieldRegistry* fieldsRegistry, QObject* parent = nullptr);

    void setupModelData();
    void addNewField();
    void markForRemoval(const FilterField& field);
    void markForChange(const FilterField& field);
    void processQueue();

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

private:
    enum OperationType
    {
        Add    = 1,
        Remove = 2,
        Change = 3
    };

    struct QueueEntry
    {
        OperationType type;
        int index;

        bool operator==(const QueueEntry& other) const
        {
            return std::tie(type, index) == std::tie(other.type, other.index);
        }
    };

    [[nodiscard]] bool findInQueue(int index, OperationType op, QueueEntry* field = nullptr) const;
    void removeFromQueue(const QueueEntry& fieldToDelete);

    using FieldQueueMap = std::deque<QueueEntry>;
    using FieldItemList = std::vector<std::unique_ptr<FieldItem>>;

    FieldRegistry* m_fieldsRegistry;

    FieldItemList m_nodes;
    FieldQueueMap m_queue;
};
} // namespace Settings
} // namespace Fy::Filters
