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
#include "playlistpopulator.h"
#include "playlistpreset.h"

#include <core/player/playerdefs.h>
#include <utils/treemodel.h>

#include <QPixmap>
#include <QThread>

namespace Fooyin {
class SettingsManager;
class MusicLibrary;
class PlayerController;
class Playlist;
struct PlaylistPreset;
struct PlaylistTrack;
class CoverProvider;

struct TrackIndexResult
{
    QModelIndex index;
    bool endOfPlaylist{false};
};

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
    PlaylistModel(MusicLibrary* library, PlayerController* playerController, SettingsManager* settings,
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

    [[nodiscard]] bool playlistIsLoaded() const;
    [[nodiscard]] bool haveTracks() const;

    MoveOperation moveTracks(const MoveOperation& operation);
    Qt::Alignment columnAlignment(int column) const;
    void changeColumnAlignment(int column, Qt::Alignment alignment);
    void resetColumnAlignments();
    void reset(const PlaylistPreset& preset, const PlaylistColumnList& columns, Playlist* playlist);

    TrackIndexResult trackIndexAtPlaylistIndex(int index, bool fetch = false);
    QModelIndex indexAtPlaylistIndex(int index);

    void insertTracks(const TrackGroups& tracks);
    void updateTracks(const std::vector<int>& indexes);
    void removeTracks(const QModelIndexList& indexes);
    void removeTracks(const TrackGroups& groups);
    void updateHeader(Playlist* playlist);

    static TrackGroups saveTrackGroups(const QModelIndexList& indexes);

    void tracksAboutToBeChanged();
    void tracksChanged();

signals:
    void playlistLoaded();
    void filesDropped(const QList<QUrl>& urls, int index);
    void tracksInserted(const TrackGroups& groups);
    void tracksMoved(const MoveOperation& operation);
    void playlistTracksChanged(int index);

public slots:
    void playingTrackChanged(const PlaylistTrack& track);
    void playStateChanged(PlayState state);

private:
    void populateModel(PendingData& data);
    void populateTrackGroup(PendingData& data);
    void updateModel(ItemKeyMap& data);

    QVariant trackData(PlaylistItem* item, int column, int role) const;
    QVariant headerData(PlaylistItem* item, int column, int role) const;
    QVariant subheaderData(PlaylistItem* item, int column, int role) const;

    PlaylistItem* itemForKey(const QString& key);

    struct DropTargetResult
    {
        QModelIndex fullMergeTarget;
        QModelIndex partMergeTarget;
        QModelIndex target;

        [[nodiscard]] QModelIndex dropTarget() const
        {
            if(fullMergeTarget.isValid()) {
                return fullMergeTarget;
            }
            if(partMergeTarget.isValid()) {
                return partMergeTarget;
            }
            return target;
        }
    };

    bool prepareDrop(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent);
    DropTargetResult findDropTarget(PlaylistItem* source, PlaylistItem* target, int& row);
    DropTargetResult canBeMerged(PlaylistItem*& currTarget, int& targetRow, PlaylistItemList& sourceParents,
                                 int targetOffset);
    void handleTrackGroup(const PendingData& data);
    void storeMimeData(const QModelIndexList& indexes, QMimeData* mimeData) const;

    int dropInsertRows(const PlaylistItemList& rows, const QModelIndex& target, int row);
    int dropMoveRows(const QModelIndex& source, const PlaylistItemList& rows, const QModelIndex& target, int row);
    int dropCopyRows(const QModelIndex& source, const PlaylistItemList& rows, const QModelIndex& target, int row);
    int dropCopyRowsRecursive(const QModelIndex& source, const PlaylistItemList& rows, const QModelIndex& target,
                              int row);

    bool insertPlaylistRows(const QModelIndex& target, int firstRow, int lastRow, const PlaylistItemList& children);
    bool movePlaylistRows(const QModelIndex& source, int firstRow, int lastRow, const QModelIndex& target, int row,
                          const PlaylistItemList& children);
    bool removePlaylistRows(int row, int count, const QModelIndex& parent);

    void fetchChildren(PlaylistItem* parentItem, PlaylistItem* child);

    void cleanupHeaders();
    void removeEmptyHeaders();
    void mergeHeaders();
    void updateHeaders();
    void updateTrackIndexes();
    void deleteNodes(PlaylistItem* parent);

    std::vector<int> pixmapColumns() const;
    void coverUpdated(const Track& track);

    using MoveOperationItemGroups = std::vector<PlaylistItemList>;

    struct MoveOperationTargetGroups
    {
        int index;
        MoveOperationItemGroups groups;
    };
    using MoveOperationMap = std::vector<MoveOperationTargetGroups>;

    MoveOperationMap determineMoveOperationGroups(const MoveOperation& operation, bool merge = true);

    struct TrackItemResult
    {
        PlaylistItem* item;
        bool endOfPlaylist{false};
    };

    TrackItemResult itemForTrackIndex(int index);

    MusicLibrary* m_library;
    SettingsManager* m_settings;
    CoverProvider* m_coverProvider;

    bool m_resetting;
    QString m_headerText;

    QPixmap m_playingIcon;
    QPixmap m_pausedIcon;
    QPixmap m_missingIcon;

    QColor m_playingColour;
    QColor m_disabledColour;

    bool m_altColours;
    QSize m_coverSize;
    QThread m_populatorThread;
    PlaylistPopulator m_populator;

    bool m_playlistLoaded;
    NodeKeyMap m_pendingNodes;
    ItemKeyMap m_nodes;
    TrackIdNodeMap m_trackParents;
    std::map<int, QString> m_trackIndexes;

    PlaylistPreset m_currentPreset;
    PlaylistColumnList m_columns;
    std::map<int, Qt::Alignment> m_columnAlignments;
    std::vector<int> m_pixmapColumns;

    Playlist* m_currentPlaylist;
    PlayState m_currentPlayState;
    PlaylistTrack m_currentPlayingTrack;
    QPersistentModelIndex m_currentPlayingIndex;
    int m_tempCurrentPlayingIndex;
    QModelIndexList m_indexesPendingRemoval;
};
} // namespace Fooyin
