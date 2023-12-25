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

#include "playlistmodel.h"

#include "internalguisettings.h"
#include "playlistitem.h"
#include "playlistmodel_p.h"
#include "playlistpopulator.h"
#include "playlistpreset.h"

#include <core/player/playermanager.h>
#include <core/playlist/playlist.h>
#include <core/track.h>
#include <gui/coverprovider.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <utils/crypto.h>
#include <utils/settings/settingsmanager.h>

#include <QIcon>
#include <QMimeData>

#include <queue>

namespace Fooyin {
PlaylistModel::PlaylistModel(MusicLibrary* library, SettingsManager* settings, QObject* parent)
    : TreeModel{parent}
    , p{std::make_unique<PlaylistModelPrivate>(this, library, settings)}
{
    p->settings->subscribe<Settings::Gui::Internal::PlaylistAltColours>(this, [this](bool enabled) {
        p->altColours = enabled;
        emit dataChanged({}, {}, {Qt::BackgroundRole});
    });
    p->settings->subscribe<Settings::Gui::Internal::PlaylistThumbnailSize>(this, [this](int size) {
        p->coverSize = {size, size};
        p->coverProvider->clearCache();
        emit dataChanged({}, {}, {PlaylistItem::Role::Cover});
    });

    p->settings->subscribe<Settings::Gui::IconTheme>(this, [this]() {
        p->playingIcon = QIcon::fromTheme(Constants::Icons::Play).pixmap(20);
        p->pausedIcon  = QIcon::fromTheme(Constants::Icons::Pause).pixmap(20);
        emit dataChanged({}, {}, {Qt::DecorationRole});
    });

    QObject::connect(&p->populator, &PlaylistPopulator::populated, this,
                     [this](PendingData data) { p->populateModel(data); });

    QObject::connect(&p->populator, &PlaylistPopulator::populatedTracks, this,
                     [this](PendingData data) { p->populateTracks(data); });

    QObject::connect(&p->populator, &PlaylistPopulator::populatedTrackGroup, this,
                     [this](PendingData data) { p->populateTrackGroup(data); });

    QObject::connect(&p->populator, &PlaylistPopulator::headersUpdated, this,
                     [this](ItemKeyMap data) { p->updateModel(data); });

    QObject::connect(p->coverProvider, &CoverProvider::coverAdded, this,
                     [this](const Track& track) { p->coverUpdated(track); });
}

PlaylistModel::~PlaylistModel()
{
    p->populator.stopThread();
    p->populatorThread.quit();
    p->populatorThread.wait();
}

Qt::ItemFlags PlaylistModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(index);
    const auto type            = index.data(PlaylistItem::Type).toInt();

    if(type == PlaylistItem::Track) {
        defaultFlags |= Qt::ItemIsDragEnabled;
        defaultFlags |= Qt::ItemNeverHasChildren;
    }
    //    else {
    //        defaultFlags &= ~Qt::ItemIsSelectable;
    //    }
    defaultFlags |= Qt::ItemIsDropEnabled;
    return defaultFlags;
}

QVariant PlaylistModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    if(!p->columns.empty()) {
        if(section >= 0 && section < static_cast<int>(p->columns.size())) {
            return p->columns.at(section).name;
        }
    }

    return p->headerText;
}

QVariant PlaylistModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item = static_cast<PlaylistItem*>(index.internalPointer());

    const PlaylistItem::ItemType type = item->type();

    if(role == PlaylistItem::Type) {
        return type;
    }

    if(role == PlaylistItem::Index) {
        return item->index();
    }

    if(role == PlaylistItem::BaseKey) {
        return item->baseKey();
    }

    if(role == PlaylistItem::MultiColumnMode) {
        return !p->columns.empty();
    }

    if(role == PlaylistItem::FirstColumn) {
        return p->firstColumn;
    }

    switch(type) {
        case(PlaylistItem::Header):
            return p->headerData(item, index.column(), role);
        case(PlaylistItem::Track):
            return p->trackData(item, index.column(), role);
        case(PlaylistItem::Subheader):
            return p->subheaderData(item, index.column(), role);
        case(PlaylistItem::Root):
            return {};
    }
    return {};
}

void PlaylistModel::fetchMore(const QModelIndex& parent)
{
    auto* parentItem = itemForIndex(parent);
    auto& rows       = p->pendingNodes[parentItem->key()];

    const int row           = parentItem->childCount();
    const int totalRows     = static_cast<int>(rows.size());
    const int rowCount      = parent.isValid() ? totalRows : std::min(50, totalRows);
    const auto rowsToInsert = std::ranges::views::take(rows, rowCount);

    beginInsertRows(parent, row, row + rowCount - 1);
    for(const QString& pendingRow : rowsToInsert) {
        PlaylistItem* child = &p->nodes.at(pendingRow);
        parentItem->appendChild(child);
        child->setPending(false);
    }
    endInsertRows();

    rows.erase(rows.begin(), rows.begin() + rowCount);
    p->updateTrackIndexes();
}

bool PlaylistModel::canFetchMore(const QModelIndex& parent) const
{
    auto* item = itemForIndex(parent);
    return p->pendingNodes.contains(item->key()) && !p->pendingNodes[item->key()].empty();
}

bool PlaylistModel::hasChildren(const QModelIndex& parent) const
{
    if(!parent.isValid()) {
        return true;
    }
    return parent.data(PlaylistItem::Type).toInt() != PlaylistItem::ItemType::Track;
}

QHash<int, QByteArray> PlaylistModel::roleNames() const
{
    auto roles = QAbstractItemModel::roleNames();

    roles.insert(+PlaylistItem::Role::Id, "ID");
    roles.insert(+PlaylistItem::Role::Subtitle, "Subtitle");
    roles.insert(+PlaylistItem::Role::Cover, "Cover");
    roles.insert(+PlaylistItem::Role::Playing, "IsPlaying");
    roles.insert(+PlaylistItem::Role::Path, "Path");
    roles.insert(+PlaylistItem::Role::ItemData, "ItemData");

    return roles;
}

int PlaylistModel::columnCount(const QModelIndex& /*parent*/) const
{
    if(!p->columns.empty()) {
        return static_cast<int>(p->columns.size());
    }
    return 1;
}

QStringList PlaylistModel::mimeTypes() const
{
    return {Constants::Mime::PlaylistItems, Constants::Mime::TrackIds};
}

bool PlaylistModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                    const QModelIndex& parent) const
{
    if((action == Qt::MoveAction || action == Qt::CopyAction)
       && (data->hasUrls() || data->hasFormat(Constants::Mime::PlaylistItems)
           || data->hasFormat(Constants::Mime::TrackIds))) {
        return true;
    }
    return QAbstractItemModel::canDropMimeData(data, action, row, column, parent);
}

Qt::DropActions PlaylistModel::supportedDragActions() const
{
    return Qt::MoveAction | Qt::CopyAction;
}

Qt::DropActions PlaylistModel::supportedDropActions() const
{
    return Qt::MoveAction | Qt::CopyAction;
}

QMimeData* PlaylistModel::mimeData(const QModelIndexList& indexes) const
{
    auto* mimeData = new QMimeData();
    p->storeMimeData(indexes, mimeData);
    return mimeData;
}

bool PlaylistModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                 const QModelIndex& parent)
{
    if(!canDropMimeData(data, action, row, column, parent)) {
        return false;
    }

    return p->prepareDrop(data, action, row, column, parent);
}

MoveOperation PlaylistModel::moveTracks(const MoveOperation& operation)
{
    return p->handleMove(operation);
}

void PlaylistModel::changeFirstColumn(int column)
{
    p->firstColumn = column;
}

void PlaylistModel::reset(const PlaylistPreset& preset, const PlaylistColumnList& columns, Playlist* playlist)
{
    if(preset.isValid()) {
        p->currentPreset = preset;
    }

    p->columns = columns;

    if(!playlist) {
        return;
    }

    p->populator.stopThread();

    p->resetting       = true;
    p->currentPlaylist = playlist;

    updateHeader(playlist);

    QMetaObject::invokeMethod(&p->populator,
                              [this, playlist] { p->populator.run(p->currentPreset, p->columns, playlist->tracks()); });
}

QModelIndex PlaylistModel::indexAtTrackIndex(int index)
{
    return p->indexForTrackIndex(index).index;
}

void PlaylistModel::insertTracks(const TrackGroups& tracks)
{
    QMetaObject::invokeMethod(&p->populator,
                              [this, tracks] { p->populator.runTracks(p->currentPreset, p->columns, tracks); });
}

void PlaylistModel::removeTracks(const QModelIndexList& indexes)
{
    p->removeTracks(indexes);
}

void PlaylistModel::removeTracks(const TrackGroups& groups)
{
    QModelIndexList rows;

    for(const auto& [index, tracks] : groups) {
        int currIndex   = index;
        const int total = index + static_cast<int>(tracks.size()) - 1;

        while(currIndex <= total) {
            auto [trackIndex, _] = p->indexForTrackIndex(currIndex);
            rows.push_back(trackIndex);
            currIndex++;
        }
    }

    p->removeTracks(rows);
}

void PlaylistModel::updateHeader(Playlist* playlist)
{
    if(playlist) {
        p->headerText = playlist->name() + ": " + QString::number(playlist->trackCount()) + " Tracks";
    }
}

void PlaylistModel::setCurrentPlaylistIsActive(bool active)
{
    p->isActivePlaylist = active;
    emit dataChanged({}, {}, {PlaylistItem::Role::Playing});
}

TrackGroups PlaylistModel::saveTrackGroups(const QModelIndexList& indexes) const
{
    TrackGroups result;

    const IndexGroupsList indexGroups = p->determineIndexGroups(indexes);

    for(const auto& group : indexGroups) {
        const int index = group.front().data(Fooyin::PlaylistItem::Role::Index).toInt();
        std::ranges::transform(std::as_const(group), std::back_inserter(result[index]), [](const QModelIndex& index) {
            return index.data(Fooyin::PlaylistItem::Role::ItemData).value<Fooyin::Track>();
        });
    }
    return result;
}

void PlaylistModel::tracksAboutToBeChanged()
{
    p->currentPlayingIndex = indexAtTrackIndex(p->currentPlaylist->currentTrackIndex());
}

void PlaylistModel::tracksChanged()
{
    const int playingIndex
        = p->currentPlayingIndex.isValid() ? p->currentPlayingIndex.data(PlaylistItem::Index).toInt() : -1;
    emit playlistTracksChanged(playingIndex);
    p->currentPlayingIndex = QPersistentModelIndex{};
}

void PlaylistModel::currentTrackChanged(const Track& track)
{
    p->currentPlayingTrack = track;
    emit dataChanged({}, {}, {Qt::DecorationRole, PlaylistItem::Role::Playing});
}

void PlaylistModel::playStateChanged(PlayState state)
{
    p->currentPlayState = state;
    if(state == PlayState::Stopped) {
        currentTrackChanged({});
    }
    emit dataChanged({}, {}, {Qt::DecorationRole, PlaylistItem::Role::Playing});
}
} // namespace Fooyin

#include "moc_playlistmodel.cpp"
