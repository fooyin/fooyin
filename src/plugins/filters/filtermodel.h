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

#include "filteritem.h"

#include <core/trackfwd.h>
#include <utils/tablemodel.h>

namespace Fooyin::Filters {
struct FilterOptions;
struct FilterField;

class FilterModel : public TableModel<FilterItem>
{
public:
    explicit FilterModel(const FilterField& field, QObject* parent = nullptr);
    ~FilterModel() override;

    [[nodiscard]] Qt::SortOrder sortOrder() const;

    void setSortOrder(Qt::SortOrder order);
    void setAppearance(const FilterOptions& options);

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QStringList mimeTypes() const override;
    [[nodiscard]] Qt::DropActions supportedDragActions() const override;
    [[nodiscard]] QMimeData* mimeData(const QModelIndexList& indexes) const override;

    //    [[nodiscard]] QModelIndexList match(const QModelIndex& start, int role, const QVariant& value, int hits,
    //                                        Qt::MatchFlags flags) const override;

    void addTracks(const TrackList& tracks);
    void updateTracks(const TrackList& tracks);
    void removeTracks(const TrackList& tracks);

    void reset(const FilterField& field, const TrackList& tracks);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin::Filters
