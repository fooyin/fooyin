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

#include "librarytreegroup.h"
#include "librarytreeitem.h"

#include <core/trackfwd.h>
#include <utils/treemodel.h>

namespace Fooyin {
struct LibraryTreeAppearance;

class LibraryTreeModel : public TreeModel<LibraryTreeItem>
{
    Q_OBJECT

public:
    explicit LibraryTreeModel(QObject* parent = nullptr);
    ~LibraryTreeModel() override;

    void setAppearance(const LibraryTreeAppearance& options);

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] bool hasChildren(const QModelIndex& parent) const override;
    void fetchMore(const QModelIndex& parent) override;
    bool canFetchMore(const QModelIndex& parent) const override;

    [[nodiscard]] QStringList mimeTypes() const override;
    [[nodiscard]] Qt::DropActions supportedDragActions() const override;
    [[nodiscard]] QMimeData* mimeData(const QModelIndexList& indexes) const override;

    void addTracks(const TrackList& tracks);
    void updateTracks(const TrackList& tracks);
    void removeTracks(const TrackList& tracks);

    void changeGrouping(const LibraryTreeGrouping& grouping);
    void reset(const TrackList& tracks);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
