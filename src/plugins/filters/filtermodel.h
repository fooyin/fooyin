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

#include <core/models/trackfwd.h>

#include <utils/treemodel.h>

#include <QAbstractListModel>

namespace Fy {
namespace Core::Scripting {
class Parser;
}

namespace Filters {
class FilterItem;
class FilterField;

class FilterModel : public QAbstractListModel
{
public:
    explicit FilterModel(FilterField* field, QObject* parent = nullptr);

    void setField(FilterField* field);

    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    [[nodiscard]] QModelIndex parent(const QModelIndex& index) const override;
    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

    void sort(int column, Qt::SortOrder order) override;
    //    [[nodiscard]] QModelIndexList match(const QModelIndex& start, int role, const QVariant& value, int hits,
    //                                        Qt::MatchFlags flags) const override;
    void reload(const Core::TrackPtrList& tracks);

private:
    void beginReset();
    void setupModelData(const Core::TrackPtrList& tracks);
    FilterItem* createNode(const QString& title, const QString& sortTitle = {});
    std::vector<FilterItem*> createNodes(const QStringList& titles, const QString& sortTitle = {});

    std::unique_ptr<FilterItem> m_root;
    std::unique_ptr<FilterItem> m_allNode;
    std::unordered_map<QString, std::unique_ptr<FilterItem>> m_nodes;
    FilterField* m_field;

    std::unique_ptr<Core::Scripting::Parser> m_parser;
};
} // namespace Filters
} // namespace Fy
