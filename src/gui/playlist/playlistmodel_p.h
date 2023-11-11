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

#include "playlistpopulator.h"

#include <QModelIndexList>
#include <QPixmap>
#include <QThread>

namespace Fy {

namespace Utils {
class SettingsManager;
}

namespace Core {
namespace Player {
enum class PlayState;
}
namespace Playlist {
class Playlist;
}
} // namespace Core

namespace Gui {
namespace Library {
class CoverProvider;
}

namespace Widgets::Playlist {
class PlaylistModel;

inline bool cmpTrackIndices(const QModelIndex& index1, const QModelIndex& index2)
{
    QModelIndex item1{index1};
    QModelIndex item2{index2};
    const QModelIndex root;

    while(item1.parent() != item2.parent()) {
        if(item1.parent() != root) {
            item1 = item1.parent();
        }
        if(item2.parent() != root) {
            item2 = item2.parent();
        }
    }
    if(item1.row() == item2.row()) {
        return false;
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

struct IndexRange
{
    int first{0};
    int last{0};

    [[nodiscard]] int count() const
    {
        return last - first + 1;
    }
};

struct TrackIndexResult
{
    QModelIndex index;
    bool endOfPlaylist{false};
};

using ParentChildIndexMap     = std::vector<std::vector<QModelIndex>>;
using ParentChildRowMap       = std::vector<std::pair<QModelIndex, std::vector<IndexRange>>>;
using ParentChildItemGroupMap = std::vector<std::pair<QModelIndex, std::vector<std::vector<PlaylistItem*>>>>;
using ParentChildItemMap      = std::vector<std::pair<QModelIndex, std::vector<PlaylistItem*>>>;

struct MergeResult
{
    QModelIndex fullMergeTarget;
    QModelIndex partMergeTarget;
};

class PlaylistModelPrivate
{
public:
    PlaylistModelPrivate(PlaylistModel* self, Utils::SettingsManager* settings);

    void populateModel(PendingData& data);
    void populateTracks(PendingData& data);
    void populateTrackGroup(PendingData& data);
    void updateModel(ItemKeyMap& data);
    void updateHeaders(const QModelIndexList& headers);

    void beginReset();

    QVariant trackData(PlaylistItem* item, int role) const;
    QVariant headerData(PlaylistItem* item, int role) const;
    QVariant subheaderData(PlaylistItem* item, int role) const;

    PlaylistItem* itemForKey(const QString& key);
    PlaylistItem* cloneParent(PlaylistItem* parent);
    MergeResult canBeMerged(PlaylistItem*& currTarget, int& targetRow, std::vector<PlaylistItem*>& sourceParents,
                            int targetOffset) const;

    bool handleDrop(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent);
    QModelIndex handleDiffParentDrop(PlaylistItem* source, PlaylistItem* target, int& row,
                                     QModelIndexList& headersToUpdate);
    void handleExternalDrop(const PendingData& data);
    void handleTrackGroup(const PendingData& data);
    void storeMimeData(const QModelIndexList& indexes, QMimeData* mimeData);

    bool insertPlaylistRows(const QModelIndex& target, int firstRow, int lastRow,
                            const std::vector<PlaylistItem*>& children);
    bool movePlaylistRows(const QModelIndex& source, int firstRow, int lastRow, const QModelIndex& target, int row,
                          const std::vector<PlaylistItem*>& children);
    bool removePlaylistRows(int row, int count, const QModelIndex& parent);

    void removeEmptyHeaders(QModelIndexList& headers);
    void mergeHeaders(QModelIndexList& headersToUpdate);
    void updateTrackIndexes();
    void deleteNodes(PlaylistItem* parent);

    void removeTracks(const QModelIndexList& indexes);
    void coverUpdated(const Core::Track& track);

    ParentChildIndexMap determineIndexGroups(const QModelIndexList& indexes);
    ParentChildRowMap determineRowGroups(const QModelIndexList& indexes);
    ParentChildItemGroupMap determineItemGroups(PlaylistModel* model, const QModelIndexList& indexes);
    ParentChildItemMap groupChildren(PlaylistModel* model, const std::vector<PlaylistItem*>& children);
    TrackIndexResult indexForTrackIndex(int index);

    PlaylistModel* model;

    Utils::SettingsManager* settings;
    Library::CoverProvider* coverProvider;

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
    Core::Playlist::Playlist* currentPlaylist;
    Core::Player::PlayState currentPlayState;
    Core::Track currentPlayingTrack;
    QPersistentModelIndex currentPlayingIndex;
};
} // namespace Widgets::Playlist
} // namespace Gui
} // namespace Fy
