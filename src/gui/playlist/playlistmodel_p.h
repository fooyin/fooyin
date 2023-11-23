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

#include "playlistmodel.h"
#include "playlistpopulator.h"

#include <QModelIndexList>
#include <QPixmap>
#include <QThread>

#include <set>

namespace Fooyin {
class SettingsManager;
class Playlist;
class CoverProvider;
enum class PlayState;
class PlaylistModel;

inline bool cmpTrackIndices(const QModelIndex& index1, const QModelIndex& index2)
{
    QModelIndex item1{index1};
    QModelIndex item2{index2};

    QModelIndexList item1Parents;
    QModelIndexList item2Parents;
    const QModelIndex root;

    while(item1.parent() != item2.parent()) {
        if(item1.parent() != root) {
            item1Parents.push_back(item1);
            item1 = item1.parent();
        }
        if(item2.parent() != root) {
            item2Parents.push_back(item2);
            item2 = item2.parent();
        }
    }
    if(item1.row() == item2.row()) {
        return item1Parents.size() < item2Parents.size();
    }
    return item1.row() < item2.row();
}

struct cmpIndexes
{
    bool operator()(const QModelIndex& index1, const QModelIndex& index2) const
    {
        return cmpTrackIndices(index1, index2);
    }
};

struct TrackIndexResult
{
    QModelIndex index;
    bool endOfPlaylist{false};
};

struct TrackItemResult
{
    PlaylistItem* item;
    bool endOfPlaylist{false};
};

using ModelIndexSet   = std::set<QModelIndex, cmpIndexes>;
using IndexGroupsList = std::vector<QModelIndexList>;

class PlaylistModelPrivate
{
public:
    PlaylistModelPrivate(PlaylistModel* self, SettingsManager* settings);

    void populateModel(PendingData& data);
    void populateTracks(PendingData& data);
    void populateTrackGroup(PendingData& data);
    void updateModel(ItemKeyMap& data);
    void updateHeaders(const ModelIndexSet& headers);

    QVariant trackData(PlaylistItem* item, int role) const;
    QVariant headerData(PlaylistItem* item, int role) const;
    QVariant subheaderData(PlaylistItem* item, int role) const;

    PlaylistItem* itemForKey(const QString& key);

    bool prepareDrop(const QMimeData* data, Qt::DropAction action, int row, int column,
                     const QModelIndex& parent) const;
    MoveOperation handleDrop(const MoveOperation& operation);
    void handleExternalDrop(const PendingData& data);
    void handleTrackGroup(const PendingData& data);
    void storeMimeData(const QModelIndexList& indexes, QMimeData* mimeData) const;

    bool insertPlaylistRows(const QModelIndex& target, int firstRow, int lastRow,
                            const PlaylistItemList& children) const;
    bool movePlaylistRows(const QModelIndex& source, int firstRow, int lastRow, const QModelIndex& target, int row,
                          const PlaylistItemList& children) const;
    bool removePlaylistRows(int row, int count, const QModelIndex& parent);

    void cleanupHeaders(const ModelIndexSet& headers);
    void removeEmptyHeaders(ModelIndexSet& headers);
    void mergeHeaders(ModelIndexSet& headersToUpdate);
    void updateTrackIndexes();
    void deleteNodes(PlaylistItem* parent);

    void removeTracks(const QModelIndexList& indexes);
    void coverUpdated(const Track& track);

    static IndexGroupsList determineIndexGroups(const QModelIndexList& indexes);
    TrackIndexResult indexForTrackIndex(int index);
    TrackItemResult itemForTrackIndex(int index);

    PlaylistModel* model;

    SettingsManager* settings;
    CoverProvider* coverProvider;

    bool resetting;
    QString headerText;
    QPixmap playingIcon;
    QPixmap pausedIcon;
    bool altColours;
    QSize coverSize;
    QThread populatorThread;
    PlaylistPopulator populator;

    NodeKeyMap pendingNodes;
    ItemKeyMap nodes;
    ItemKeyMap oldNodes;
    TrackIdNodeMap trackParents;
    std::unordered_map<int, QString> trackIndexes;

    PlaylistPreset currentPreset;
    bool isActivePlaylist;
    Playlist* currentPlaylist;
    PlayState currentPlayState;
    Track currentPlayingTrack;
    QPersistentModelIndex currentPlayingIndex;
};
} // namespace Fooyin
