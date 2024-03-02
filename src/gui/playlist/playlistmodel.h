/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include "playlistcolumn.h"
#include "playlistitem.h"

#include <utils/treemodel.h>

namespace Fooyin {
class SettingsManager;
class MusicLibrary;
class PlayerManager;
class Playlist;
enum class PlayState;
struct PlaylistPreset;
struct PlaylistTrack;
class PlaylistModelPrivate;

using TrackGroups = std::map<int, TrackList>;

struct TrackIndexRange
{
    int first{0};
    int last{0};

    [[nodiscard]] int count() const
    {
        return last - first + 1;
    }
};
using TrackIndexRangeList = std::vector<TrackIndexRange>;

struct MoveOperationGroup
{
    int index;
    TrackIndexRangeList tracksToMove;
};

using MoveOperation = std::vector<MoveOperationGroup>;

class PlaylistModel : public TreeModel<PlaylistItem>
{
    Q_OBJECT

public:
    PlaylistModel(MusicLibrary* library, PlayerManager* playerManager, SettingsManager* settings,
                  QObject* parent = nullptr);
    ~PlaylistModel() override;

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    void fetchMore(const QModelIndex& parent) override;
    bool canFetchMore(const QModelIndex& parent) const override;
    [[nodiscard]] bool hasChildren(const QModelIndex& parent) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

    [[nodiscard]] QStringList mimeTypes() const override;
    [[nodiscard]] bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                       const QModelIndex& parent) const override;
    [[nodiscard]] Qt::DropActions supportedDragActions() const override;
    [[nodiscard]] Qt::DropActions supportedDropActions() const override;
    [[nodiscard]] QMimeData* mimeData(const QModelIndexList& indexes) const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                      const QModelIndex& parent) override;

    MoveOperation moveTracks(const MoveOperation& operation);
    Qt::Alignment columnAlignment(int column) const;
    void changeColumnAlignment(int column, Qt::Alignment alignment);
    void reset(const PlaylistPreset& preset, const PlaylistColumnList& columns, Playlist* playlist);

    QModelIndex indexAtTrackIndex(int index);
    void insertTracks(const TrackGroups& tracks);
    void updateTracks(const std::vector<int>& indexes);
    void removeTracks(const QModelIndexList& indexes);
    void removeTracks(const TrackGroups& groups);
    void updateHeader(Playlist* playlist);

    TrackGroups saveTrackGroups(const QModelIndexList& indexes) const;

    void tracksAboutToBeChanged();
    void tracksChanged();

signals:
    void filesDropped(const QList<QUrl>& urls, int index);
    void tracksInserted(const TrackGroups& groups);
    void tracksMoved(const MoveOperation& operation);
    void playlistTracksChanged(int index);

public slots:
    void currentTrackChanged(const PlaylistTrack& track);
    void playStateChanged(PlayState state);

private:
    friend PlaylistModelPrivate;
    std::unique_ptr<PlaylistModelPrivate> p;
};
} // namespace Fooyin
