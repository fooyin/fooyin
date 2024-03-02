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

#include "playlistmodel.h"
#include "playlistpopulator.h"

#include <core/player/playerdefs.h>

#include <QModelIndexList>
#include <QPixmap>
#include <QThread>

#include <set>

namespace Fooyin {
class SettingsManager;
class MusicLibrary;
class Playlist;
class CoverProvider;
enum class PlayState;
class PlaylistModel;

inline bool cmpItemsPlaylistItems(PlaylistItem* pItem1, PlaylistItem* pItem2, bool reverse = false)
{
    PlaylistItem* item1{pItem1};
    PlaylistItem* item2{pItem2};

    while(item1->parent() && item2->parent() && item1->parent() != item2->parent()) {
        if(item1->parent() == item2) {
            return true;
        }
        if(item2->parent() == item1) {
            return false;
        }
        if(item1->parent()->type() != PlaylistItem::Root) {
            item1 = item1->parent();
        }
        if(item2->parent()->type() != PlaylistItem::Root) {
            item2 = item2->parent();
        }
    }
    if(item1->row() == item2->row()) {
        return false;
    }
    return reverse ? item1->row() > item2->row() : item1->row() < item2->row();
};

struct cmpItems
{
    bool operator()(PlaylistItem* pItem1, PlaylistItem* pItem2) const
    {
        return cmpItemsPlaylistItems(pItem1, pItem2);
    }
};

struct cmpItemsReverse
{
    bool operator()(PlaylistItem* pItem1, PlaylistItem* pItem2) const
    {
        return cmpItemsPlaylistItems(pItem1, pItem2, true);
    }
};

using ItemPtrSet = std::set<PlaylistItem*, cmpItemsReverse>;

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

using IndexGroupsList = std::vector<QModelIndexList>;

using ColumnAlignments = std::map<int, Qt::Alignment>;

class PlaylistModelPrivate
{
public:
    PlaylistModelPrivate(PlaylistModel* self, MusicLibrary* library, PlayerManager* playerManager, SettingsManager* settings);

    void populateModel(PendingData& data);
    void populateTrackGroup(PendingData& data);
    void updateModel(ItemKeyMap& data);

    QVariant trackData(PlaylistItem* item, int column, int role) const;
    QVariant headerData(PlaylistItem* item, int column, int role) const;
    QVariant subheaderData(PlaylistItem* item, int column, int role) const;

    PlaylistItem* itemForKey(const QString& key);

    bool prepareDrop(const QMimeData* data, Qt::DropAction action, int row, int column,
                     const QModelIndex& parent) const;
    MoveOperation handleMove(const MoveOperation& operation);
    void handleTrackGroup(const PendingData& data);
    void storeMimeData(const QModelIndexList& indexes, QMimeData* mimeData) const;

    bool insertPlaylistRows(const QModelIndex& target, int firstRow, int lastRow,
                            const PlaylistItemList& children) const;
    bool movePlaylistRows(const QModelIndex& source, int firstRow, int lastRow, const QModelIndex& target, int row,
                          const PlaylistItemList& children) const;
    bool removePlaylistRows(int row, int count, const QModelIndex& parent);

    void fetchChildren(PlaylistItem* parentItem, PlaylistItem* child);

    void cleanupHeaders();
    void removeEmptyHeaders();
    void mergeHeaders();
    void updateHeaders();
    void updateTrackIndexes();
    void deleteNodes(PlaylistItem* parent);

    void removeTracks(const QModelIndexList& indexes);
    void coverUpdated(const Track& track);

    static IndexGroupsList determineIndexGroups(const QModelIndexList& indexes);
    TrackIndexResult indexForTrackIndex(int index) const;
    TrackItemResult itemForTrackIndex(int index);

    PlaylistModel* model;

    MusicLibrary* library;
    PlayerManager* playerManager;
    SettingsManager* settings;
    CoverProvider* coverProvider;

    bool resetting;
    QString headerText;
    QPixmap playingIcon;
    QPixmap pausedIcon;
    QPixmap missingIcon;
    bool altColours;
    QSize coverSize;
    QThread populatorThread;
    PlaylistPopulator populator;

    NodeKeyMap pendingNodes;
    ItemKeyMap nodes;
    TrackIdNodeMap trackParents;
    std::unordered_map<int, QString> trackIndexes;

    PlaylistPreset currentPreset;
    PlaylistColumnList columns;
    ColumnAlignments columnAlignments;

    Playlist* currentPlaylist;
    PlayState currentPlayState;
    PlaylistTrack currentTrack;
    QPersistentModelIndex currentPlayingIndex;
    int currentIndex;
    QModelIndexList indexesToRemove;
};
} // namespace Fooyin
