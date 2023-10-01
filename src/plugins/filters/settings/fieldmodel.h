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

#include "filterfwd.h"

#include <utils/tablemodel.h>
#include <utils/treestatusitem.h>

namespace Fy::Filters {
class FieldRegistry;

namespace Settings {
class FieldItem : public Utils::TreeStatusItem<FieldItem>
{
public:
    FieldItem();
    explicit FieldItem(FilterField field, FieldItem* parent);

    [[nodiscard]] FilterField field() const;
    void changeField(const FilterField& field);

private:
    FilterField m_field;
};

class FieldModel : public Utils::TableModel<FieldItem>
{
public:
    explicit FieldModel(FieldRegistry* fieldsRegistry, QObject* parent = nullptr);

    void populate();
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
    void reset();
    void removeField(int index);

    using FieldItemMap = std::map<int, FieldItem>;

    FieldRegistry* m_fieldsRegistry;
    FieldItemMap m_nodes;
};
} // namespace Settings
} // namespace Fy::Filters
