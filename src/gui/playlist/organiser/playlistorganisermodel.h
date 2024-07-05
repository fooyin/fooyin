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

#include "playlistorganiseritem.h"

#include <core/track.h>
#include <utils/treemodel.h>

#include <QColor>
#include <QIcon>

namespace Fooyin {
class Id;
class Playlist;
class PlayerController;
class PlaylistHandler;

class PlaylistOrganiserModel : public TreeModel<PlaylistOrganiserItem>
{
    Q_OBJECT

public:
    explicit PlaylistOrganiserModel(PlaylistHandler* playlistHandler, PlayerController* playerController);

    void populate();
    void populateMissing();
    QByteArray saveModel();
    bool restoreModel(QByteArray data);

    QModelIndex createGroup(const QModelIndex& parent);
    QModelIndex createPlaylist(Playlist* playlist, const QModelIndex& parent);

    void playlistAdded(Playlist* playlist);
    void playlistInserted(Playlist* playlist, const QString& group, int index);
    void playlistRenamed(Playlist* playlist);
    void playlistRemoved(Playlist* playlist);

    QModelIndex indexForPlaylist(Playlist* playlist);

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] bool hasChildren(const QModelIndex& parent) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    [[nodiscard]] QStringList mimeTypes() const override;
    [[nodiscard]] bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                       const QModelIndex& parent) const override;
    [[nodiscard]] Qt::DropActions supportedDropActions() const override;
    [[nodiscard]] Qt::DropActions supportedDragActions() const override;
    [[nodiscard]] QMimeData* mimeData(const QModelIndexList& indexes) const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                      const QModelIndex& parent) override;

    void removeItems(const QModelIndexList& indexes);

signals:
    void filesDroppedOnPlaylist(const QList<QUrl>& urls, const Fooyin::Id& id);
    void filesDroppedOnGroup(const QList<QUrl>& urls, const QString& group, int index);
    void tracksDroppedOnPlaylist(const std::vector<int>& trackIds, const Fooyin::Id& id);
    void tracksDroppedOnGroup(const std::vector<int>& trackIds, const QString& group, int index);

private:
    QByteArray saveIndexes(const QModelIndexList& indexes) const;
    QModelIndexList restoreIndexes(QByteArray data);
    void recurseSaveModel(QDataStream& stream, PlaylistOrganiserItem* parent);
    QString findUniqueName(const QString& name) const;
    void deleteNodes(PlaylistOrganiserItem* node);
    bool itemsDropped(const QMimeData* data, int row, const QModelIndex& parent);

    PlaylistHandler* m_playlistHandler;
    PlayerController* m_playerController;

    std::unordered_map<QString, PlaylistOrganiserItem> m_nodes;

    QColor m_playingColour;
    QIcon m_playIcon;
    QIcon m_pauseIcon;
};
} // namespace Fooyin
