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

#include <core/models/trackfwd.h>

#include <utils/treemodel.h>

#include <QAbstractListModel>

namespace Fy::Filters {
class FilterItem;

class FilterModel : public QAbstractListModel
{
public:
    FilterModel(Filters::FilterType type, QObject* parent = nullptr);

    void setType(Filters::FilterType type);
    [[nodiscard]] int index() const;
    void setIndex(int index);

    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    [[nodiscard]] QModelIndex parent(const QModelIndex& index) const override;
    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

    [[nodiscard]] QModelIndexList match(const QModelIndex& start, int role, const QVariant& value, int hits,
                                        Qt::MatchFlags flags) const override;
    void reload(const Core::TrackPtrList& tracks);

private:
    void beginReset();
    void setupModelData(const Core::TrackPtrList& tracks);
    FilterItem* createNode(const QString& title);
    std::vector<FilterItem*> createNodes(const QStringList& titles);

    std::unique_ptr<FilterItem> m_root;
    std::unique_ptr<FilterItem> m_allNode;
    std::unordered_map<QString, std::unique_ptr<FilterItem>> m_nodes;
    FilterType m_type;
};
} // namespace Fy::Filters
