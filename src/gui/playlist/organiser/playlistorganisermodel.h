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

#include "playlistorganiseritem.h"

#include <utils/treemodel.h>

namespace Fooyin {
class Playlist;
class PlaylistManager;

class PlaylistOrganiserModel : public TreeModel<PlaylistOrganiserItem>
{
    Q_OBJECT

public:
    explicit PlaylistOrganiserModel(PlaylistManager* playlistManager);
    ~PlaylistOrganiserModel() override;

    void reset();
    QByteArray saveModel();
    bool restoreModel(QByteArray data);

    QModelIndex createGroup(const QModelIndex& parent);
    QModelIndex createPlaylist(Playlist* playlist, const QModelIndex& parent);

    void playlistAdded(Playlist* playlist);
    void playlistRenamed(Playlist* playlist);
    void playlistRemoved(Playlist* playlist);

    QModelIndex indexForPlaylist(Playlist* playlist);

    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    bool hasChildren(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    [[nodiscard]] QStringList mimeTypes() const override;
    [[nodiscard]] bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                       const QModelIndex& parent) const override;
    Qt::DropActions supportedDropActions() const override;
    Qt::DropActions supportedDragActions() const override;
    [[nodiscard]] QMimeData* mimeData(const QModelIndexList& indexes) const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                      const QModelIndex& parent) override;

    void removeItems(const QModelIndexList& indexes);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
