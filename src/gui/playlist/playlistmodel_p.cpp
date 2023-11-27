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

#include "playlistmodel_p.h"

#include <core/library/musiclibrary.h>
#include <core/player/playermanager.h>
#include <core/playlist/playlist.h>
#include <gui/coverprovider.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <utils/crypto.h>
#include <utils/settings/settingsmanager.h>

#include <QIODevice>
#include <QIcon>
#include <QMimeData>
#include <QPalette>

#include <queue>
#include <set>
#include <stack>

using namespace Qt::Literals::StringLiterals;

namespace {
bool cmpItemsPlaylistItems(Fooyin::PlaylistItem* pItem1, Fooyin::PlaylistItem* pItem2, bool reverse = false)
{
    Fooyin::PlaylistItem* item1{pItem1};
    Fooyin::PlaylistItem* item2{pItem2};

    while(item1->parent() != item2->parent()) {
        if(item1->parent() == item2) {
            return true;
        }
        if(item2->parent() == item1) {
            return false;
        }
        if(item1->parent()->type() != Fooyin::PlaylistItem::Root) {
            item1 = item1->parent();
        }
        if(item2->parent()->type() != Fooyin::PlaylistItem::Root) {
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
    bool operator()(Fooyin::PlaylistItem* pItem1, Fooyin::PlaylistItem* pItem2) const
    {
        return cmpItemsPlaylistItems(pItem1, pItem2);
    }
};

struct cmpItemsReverse
{
    bool operator()(Fooyin::PlaylistItem* pItem1, Fooyin::PlaylistItem* pItem2) const
    {
        return cmpItemsPlaylistItems(pItem1, pItem2, true);
    }
};
using ItemPtrSet = std::set<Fooyin::PlaylistItem*, cmpItems>;

QModelIndex determineDropIndex(Fooyin::PlaylistModel* model, const QModelIndex& parent, int row)
{
    if(!parent.isValid() && row >= model->rowCount(parent)) {
        return {};
    }

    QModelIndex current = model->index(row, 0, parent);
    while(model->rowCount(current) > 0) {
        current = model->index(0, 0, current);
    }
    return current;
}

Fooyin::PlaylistItem* cloneParent(Fooyin::ItemKeyMap& nodes, Fooyin::PlaylistItem* parent)
{
    const QString parentKey = Fooyin::Utils::generateRandomHash();
    auto* newParent         = &nodes.emplace(parentKey, *parent).first->second;
    newParent->setKey(parentKey);
    newParent->resetRow();
    newParent->clearChildren();

    return newParent;
}

struct ParentChildIndexRanges
{
    QModelIndex parent;
    std::vector<Fooyin::TrackIndexRange> ranges;
};

using ParentChildRangesList = std::vector<ParentChildIndexRanges>;

ParentChildRangesList determineRowGroups(const QModelIndexList& indexes)
{
    ParentChildRangesList indexGroups;

    QModelIndexList sortedIndexes{indexes};
    std::ranges::sort(sortedIndexes, Fooyin::cmpTrackIndices);

    auto startOfSequence = sortedIndexes.cbegin();
    while(startOfSequence != sortedIndexes.cend()) {
        auto endOfSequence
            = std::adjacent_find(startOfSequence, sortedIndexes.cend(), [](const auto& lhs, const auto& rhs) {
                  return lhs.parent() != rhs.parent() || rhs.row() != lhs.row() + 1;
              });
        if(endOfSequence != sortedIndexes.cend()) {
            std::advance(endOfSequence, 1);
        }

        Fooyin::TrackIndexRange indexRange;
        indexRange.first = startOfSequence->row();
        indexRange.last  = std::prev(endOfSequence)->row();

        const QModelIndex parent = startOfSequence->parent();
        auto it = std::ranges::find_if(indexGroups, [&parent](const auto& range) { return range.parent == parent; });
        if(it != indexGroups.end()) {
            it->ranges.push_back(indexRange);
        }
        else {
            indexGroups.emplace_back(parent, std::vector<Fooyin::TrackIndexRange>{indexRange});
        }

        startOfSequence = endOfSequence;
    }

    return indexGroups;
}

Fooyin::TrackIndexRangeList determineTrackIndexGroups(const QModelIndexList& indexes)
{
    using Fooyin::PlaylistItem;

    Fooyin::TrackIndexRangeList indexGroups;

    QModelIndexList sortedIndexes{indexes};
    std::ranges::sort(sortedIndexes, Fooyin::cmpTrackIndices);

    auto startOfSequence = sortedIndexes.cbegin();
    while(startOfSequence != sortedIndexes.cend()) {
        auto endOfSequence
            = std::adjacent_find(startOfSequence, sortedIndexes.cend(), [](const auto& lhs, const auto& rhs) {
                  return lhs.parent() != rhs.parent() || rhs.row() != lhs.row() + 1;
              });
        if(endOfSequence != sortedIndexes.cend()) {
            std::advance(endOfSequence, 1);
        }

        indexGroups.emplace_back(startOfSequence->data(PlaylistItem::Index).toInt(),
                                 std::prev(endOfSequence)->data(PlaylistItem::Index).toInt());

        startOfSequence = endOfSequence;
    }

    return indexGroups;
}

struct MoveOperationItemGroup
{
    int index;
    Fooyin::PlaylistItemList items;
};

using MoveOperationMap = std::map<int, std::vector<Fooyin::PlaylistItemList>>;

MoveOperationMap determineMoveOperationGroups(Fooyin::PlaylistModelPrivate* model,
                                              const Fooyin::MoveOperation& operation)
{
    MoveOperationMap result;

    for(const auto& [index, tracks] : operation) {
        for(const auto& range : tracks) {
            Fooyin::PlaylistItemList rows;
            rows.reserve(range.count());

            for(int trackIndex{range.first}; trackIndex <= range.last; ++trackIndex) {
                rows.push_back(model->itemForTrackIndex(trackIndex).item);
            }
            result[index].push_back(rows);
        }
    }

    return result;
}

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

DropTargetResult canBeMerged(Fooyin::PlaylistModel* model, Fooyin::PlaylistItem*& currTarget, int& targetRow,
                             Fooyin::PlaylistItemList& sourceParents, int targetOffset)
{
    using Fooyin::PlaylistItem;
    using Fooyin::PlaylistItemList;

    PlaylistItem* checkItem = currTarget->child(targetRow + targetOffset);

    if(!checkItem) {
        return {};
    }

    if(sourceParents.empty() || sourceParents.front()->baseKey() != checkItem->baseKey()) {
        return {};
    }

    PlaylistItemList newSourceParents;
    bool diffFound{false};

    for(PlaylistItem* parent : sourceParents) {
        if(diffFound || parent->baseKey() != checkItem->baseKey()) {
            diffFound = true;
            newSourceParents.push_back(parent);
            continue;
        }

        currTarget     = checkItem;
        targetRow      = targetOffset >= 0 ? -1 : currTarget->childCount() - 1;
        auto* nextItem = checkItem->child(targetOffset >= 0 ? 0 : checkItem->childCount() - 1);

        if(nextItem && nextItem->type() != PlaylistItem::Track) {
            checkItem = nextItem;
        }
    }

    if(!diffFound) {
        return {.fullMergeTarget = model->indexOfItem(currTarget), .partMergeTarget = {}, .target = {}};
    }

    sourceParents = newSourceParents;
    return {.fullMergeTarget = {}, .partMergeTarget = model->indexOfItem(currTarget), .target = {}};
}

struct SplitParent
{
    QModelIndex source;
    Fooyin::PlaylistItem* target;
    int firstRow{0};
    int finalRow{0};
    std::vector<Fooyin::PlaylistItem*> children;
};

DropTargetResult findDropTarget(Fooyin::PlaylistModelPrivate* self, Fooyin::PlaylistItem* source,
                                Fooyin::PlaylistItem* target, int& row, Fooyin::ModelIndexSet& headersToUpdate)
{
    using Fooyin::PlaylistItem;
    using Fooyin::PlaylistItemList;

    DropTargetResult dropResult;

    if(source->baseKey() == target->baseKey()) {
        dropResult.fullMergeTarget = self->model->indexOfItem(target);
        return dropResult;
    }

    int targetRow{row};
    PlaylistItemList sourceParents;
    PlaylistItemList targetParents;

    PlaylistItem* currSource{source};
    PlaylistItem* currTarget{target};

    // Find common ancestor
    while(currSource->baseKey() != currTarget->baseKey()) {
        if(currTarget->type() != PlaylistItem::Root) {
            targetParents.push_back(currTarget);
            targetRow  = currTarget->row();
            currTarget = currTarget->parent();
        }

        if(currTarget->baseKey() == currSource->baseKey()) {
            break;
        }

        if(currSource->type() != PlaylistItem::Root) {
            sourceParents.push_back(currSource);
            currSource = currSource->parent();
        }
    }

    std::ranges::reverse(sourceParents);

    const bool targetIsRoot = target->type() == PlaylistItem::Root;
    const bool diffHeader   = std::ranges::any_of(
        targetParents, [](const PlaylistItem* parent) { return (parent->type() == PlaylistItem::Header); });

    const bool canMergeleft
        = targetIsRoot ? row > 0
                       : row == 0 && (!diffHeader || std::ranges::all_of(targetParents, [](PlaylistItem* parent) {
                             return parent->type() == PlaylistItem::Header || parent->row() == 0;
                         }));

    // Check left
    if(canMergeleft) {
        dropResult = canBeMerged(self->model, currTarget, targetRow, sourceParents, -1);
        if(dropResult.fullMergeTarget.isValid()) {
            row = targetRow + 1;
            return dropResult;
        }
        if(dropResult.partMergeTarget.isValid()) {
            row = targetRow + 1;
        }
    }

    const bool canMergeRight
        = targetIsRoot ? row <= target->childCount() - 1
                       : row == target->childCount()
                             && (!diffHeader || std::ranges::all_of(targetParents, [](const PlaylistItem* parent) {
                                    return parent->type() == PlaylistItem::Header
                                        || parent->row() == parent->parent()->childCount() - 1;
                                }));

    // Check right
    if(canMergeRight) {
        dropResult = canBeMerged(self->model, currTarget, targetRow, sourceParents, targetIsRoot ? 0 : 1);
        if(dropResult.fullMergeTarget.isValid()) {
            row = 0;
            return dropResult;
        }
        if(dropResult.partMergeTarget.isValid()) {
            row = 0;
        }
    }

    int newParentRow = targetRow + 1;
    PlaylistItem* prevParentItem{currTarget};

    if(!dropResult.partMergeTarget.isValid()) {
        // Create parents for tracks after drop index (if any)
        std::vector<SplitParent> splitParents;

        for(PlaylistItem* parent : targetParents) {
            const int finalRow = parent->childCount() - 1;
            if(finalRow >= row || !splitParents.empty()) {
                PlaylistItem* newParent = cloneParent(self->nodes, parent);

                PlaylistItemList children;
                const auto childrenToMove = parent->children() | std::views::drop(row);
                std::ranges::copy(childrenToMove, std::back_inserter(children));

                const QModelIndex parentIndex = self->model->indexOfItem(parent);
                splitParents.emplace_back(parentIndex, newParent, row, finalRow, children);
            }
            row = parent->row() + (row > 0 ? 1 : 0);
        }

        std::ranges::reverse(splitParents);

        // Move tracks after drop index to new parents
        for(const SplitParent& parent : splitParents) {
            const QModelIndex prevParent = self->model->indexOfItem(prevParentItem);

            self->insertPlaylistRows(prevParent, newParentRow, newParentRow, {parent.target});

            const QModelIndex newParentIndex = self->model->indexOfItem(parent.target);
            self->movePlaylistRows(parent.source, parent.firstRow, parent.finalRow, newParentIndex, 0, parent.children);

            prevParentItem = parent.target;
            newParentRow   = 0;
        }

        for(const auto& parent : splitParents) {
            headersToUpdate.emplace(parent.source);
            headersToUpdate.emplace(self->model->indexOfItem(parent.target));
        }
    }

    prevParentItem = currTarget;
    newParentRow   = row;

    // Create parents for dropped rows
    for(PlaylistItem* parent : sourceParents) {
        const QModelIndex prevParent = self->model->indexOfItem(prevParentItem);
        PlaylistItem* newParent      = cloneParent(self->nodes, parent);

        self->insertPlaylistRows(prevParent, newParentRow, newParentRow, {newParent});

        prevParentItem = newParent;
        newParentRow   = 0;
    }

    if(!sourceParents.empty()) {
        row = 0;
    }

    if(dropResult.partMergeTarget.isValid()) {
        dropResult.partMergeTarget = self->model->indexOfItem(prevParentItem);
    }
    else {
        dropResult.target = self->model->indexOfItem(prevParentItem);
    }

    return dropResult;
}

QByteArray saveTracks(const QModelIndexList& indexes)
{
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);

    Fooyin::TrackIds trackIds;
    trackIds.reserve(indexes.size());

    std::ranges::transform(indexes, std::back_inserter(trackIds), [](const QModelIndex& index) {
        return index.data(Fooyin::PlaylistItem::Role::ItemData).value<Fooyin::Track>().id();
    });

    stream << trackIds;

    return result;
}

Fooyin::TrackList restoreTracks(Fooyin::MusicLibrary* library, QByteArray data)
{
    Fooyin::TrackIds ids;
    QDataStream stream(&data, QIODevice::ReadOnly);

    stream >> ids;

    Fooyin::TrackList tracks = library->tracksForIds(ids);

    return tracks;
}

QByteArray saveIndexes(const QModelIndexList& indexes, Fooyin::Playlist* playlist)
{
    if(!playlist) {
        return {};
    }

    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);

    stream << playlist->id();

    for(const QModelIndex& index : indexes) {
        QModelIndex localIndex = index;
        std::stack<int> indexParentStack;

        while(localIndex.isValid()) {
            indexParentStack.push(localIndex.row());
            localIndex = localIndex.parent();
        }

        stream << static_cast<qsizetype>(indexParentStack.size());

        while(!indexParentStack.empty()) {
            stream << indexParentStack.top();
            indexParentStack.pop();
        }
    }
    return result;
}

bool dropOnSamePlaylist(QByteArray data, Fooyin::Playlist* playlist)
{
    if(!playlist) {
        return {};
    }

    QDataStream stream(&data, QIODevice::ReadOnly);

    int playlistId;
    stream >> playlistId;

    return playlistId == playlist->id();
}

QModelIndexList restoreIndexes(QAbstractItemModel* model, QByteArray data, Fooyin::Playlist* playlist)
{
    if(!playlist) {
        return {};
    }

    QModelIndexList result;
    QDataStream stream(&data, QIODevice::ReadOnly);

    int playlistId;
    stream >> playlistId;

    if(playlistId != playlist->id()) {
        return {};
    }

    while(!stream.atEnd()) {
        int childDepth{0};
        stream >> childDepth;

        QModelIndex currentIndex;
        for(int i{0}; i < childDepth; ++i) {
            int row{0};
            stream >> row;
            currentIndex = model->index(row, 0, currentIndex);
        }
        if(currentIndex.isValid()) {
            result << currentIndex;
        }
    }
    return result;
}

template <typename Container>
int moveRows(Fooyin::PlaylistModel* model, const QModelIndex& source, const Container& rows, const QModelIndex& target,
             int row)
{
    int currRow{row};
    auto* targetParent = model->itemForIndex(target);
    if(!targetParent) {
        return currRow;
    }

    auto* sourceParent = model->itemForIndex(source);
    for(Fooyin::PlaylistItem* childItem : rows) {
        childItem->resetRow();
        const int oldRow = childItem->row();
        if(oldRow < currRow) {
            targetParent->insertChild(currRow, childItem);
            sourceParent->removeChild(oldRow);

            if(source != target) {
                ++currRow;
            }
        }
        else {
            sourceParent->removeChild(oldRow);
            targetParent->insertChild(currRow, childItem);

            ++currRow;
        }
    }
    sourceParent->resetChildren();
    targetParent->resetChildren();

    return currRow;
}

template <typename Container>
int copyRowsRecursive(Fooyin::PlaylistModel* model, Fooyin::ItemKeyMap& nodes, const QModelIndex& source,
                      const Container& rows, const QModelIndex& target, int row)
{
    int currRow{row};
    auto* targetParent = model->itemForIndex(target);
    if(!targetParent) {
        return currRow;
    }

    auto* sourceParent = model->itemForIndex(source);
    for(Fooyin::PlaylistItem* childItem : rows) {
        childItem->resetRow();
        const QString newKey = Fooyin::Utils::generateRandomHash();
        auto* newChild       = &nodes.emplace(newKey, *childItem).first->second;
        newChild->clearChildren();
        newChild->setKey(newKey);

        targetParent->insertChild(currRow, newChild);

        const QModelIndex childSourceIndex = model->indexOfItem(childItem);
        const QModelIndex childTargetIndex = model->indexOfItem(newChild);
        copyRowsRecursive(model, nodes, childSourceIndex, childItem->children(), childTargetIndex, 0);

        ++currRow;
    }

    sourceParent->resetChildren();
    targetParent->resetChildren();

    return currRow;
}

template <typename Container>
int copyRows(Fooyin::PlaylistModel* model, Fooyin::ItemKeyMap& nodes, const QModelIndex& source, const Container& rows,
             const QModelIndex& target, int row)
{
    return copyRowsRecursive(model, nodes, source, rows, target, row);
}

template <typename Container>
int insertRows(Fooyin::PlaylistModel* model, Fooyin::ItemKeyMap& nodes, const Container& rows,
               const QModelIndex& target, int row)
{
    auto* targetParent = model->itemForIndex(target);
    if(!targetParent) {
        return row;
    }

    for(Fooyin::PlaylistItem* childItem : rows) {
        childItem->resetRow();
        auto* newChild = &nodes.emplace(childItem->key(), *childItem).first->second;

        targetParent->insertChild(row, newChild);
        newChild->setPending(false);
        row++;
    }
    targetParent->resetChildren();

    return row;
}

void updateHeaderChildren(Fooyin::PlaylistItem* header)
{
    if(!header) {
        return;
    }

    const auto type = header->type();

    if(type == Fooyin::PlaylistItem::Header || type == Fooyin::PlaylistItem::Subheader) {
        Fooyin::PlaylistContainerItem& container = std::get<1>(header->data());
        container.clearTracks();

        const auto& children = header->children();
        for(Fooyin::PlaylistItem* child : children) {
            if(child->type() == Fooyin::PlaylistItem::Track) {
                const Fooyin::Track& track = std::get<0>(child->data()).track();
                container.addTrack(track);
            }
            else {
                const Fooyin::TrackList tracks = std::get<1>(child->data()).tracks();
                container.addTracks(tracks);
            }
        }
    }
}
} // namespace

namespace Fooyin {
PlaylistModelPrivate::PlaylistModelPrivate(PlaylistModel* model, MusicLibrary* library, SettingsManager* settings)
    : model{model}
    , library{library}
    , settings{settings}
    , coverProvider{new CoverProvider(model)}
    , resetting{false}
    , playingIcon{QIcon::fromTheme(Constants::Icons::Play).pixmap(20)}
    , pausedIcon{QIcon::fromTheme(Constants::Icons::Pause).pixmap(20)}
    , altColours{settings->value<Settings::Gui::PlaylistAltColours>()}
    , coverSize{settings->value<Settings::Gui::PlaylistThumbnailSize>(),
                settings->value<Settings::Gui::PlaylistThumbnailSize>()}
    , isActivePlaylist{false}
    , currentPlaylist{nullptr}
    , currentPlayState{PlayState::Stopped}
{
    populator.moveToThread(&populatorThread);
    populatorThread.start();
}

void PlaylistModelPrivate::populateModel(PendingData& data)
{
    if(resetting) {
        model->beginResetModel();
        model->resetRoot();
        // Acts as a cache in case model hasn't been fully cleared
        oldNodes = std::move(nodes);
        nodes.clear();
        pendingNodes.clear();
        trackParents.clear();
    }

    nodes.merge(data.items);
    trackParents.merge(data.trackParents);

    if(resetting) {
        for(const auto& [parentKey, rows] : data.nodes) {
            auto* parent = parentKey == "0"_L1 ? model->itemForIndex({}) : &nodes.at(parentKey);

            for(const QString& row : rows) {
                PlaylistItem* child = &nodes.at(row);
                parent->appendChild(child);
                child->setPending(false);
            }
        }
        updateTrackIndexes();
        model->endResetModel();
        resetting = false;
    }
    else {
        for(auto& [parentKey, rows] : data.nodes) {
            std::ranges::copy(rows, std::back_inserter(pendingNodes[parentKey]));
        }
    }
}

void PlaylistModelPrivate::populateTracks(PendingData& data)
{
    model->tracksAboutToBeChanged();

    nodes.merge(data.items);
    trackParents.merge(data.trackParents);

    handleExternalDrop(data);

    model->tracksChanged();
}

void PlaylistModelPrivate::populateTrackGroup(PendingData& data)
{
    model->tracksAboutToBeChanged();

    if(nodes.empty()) {
        resetting = true;
        populateModel(data);
        model->tracksChanged();
        return;
    }

    nodes.merge(data.items);
    trackParents.merge(data.trackParents);

    handleTrackGroup(data);

    model->tracksChanged();
}

void PlaylistModelPrivate::updateModel(ItemKeyMap& data)
{
    if(!resetting) {
        for(auto& [key, header] : data) {
            nodes[key]                    = header;
            const QModelIndex headerIndex = model->indexOfItem(&nodes[key]);
            emit model->dataChanged(headerIndex, headerIndex, {});
        }
    }
}

void PlaylistModelPrivate::updateHeaders(const ModelIndexSet& headers)
{
    ItemPtrSet items;

    for(const QModelIndex& header : headers) {
        if(header.isValid()) {
            PlaylistItem* currHeader{model->itemForIndex(header)};
            while(currHeader->type() != PlaylistItem::Root) {
                if(currHeader->childCount() > 0) {
                    items.emplace(currHeader);
                }
                currHeader = currHeader->parent();
            }
        }
    }

    ItemList updatedHeaders;

    for(PlaylistItem* header : items) {
        updateHeaderChildren(header);
        updatedHeaders.emplace_back(*header);
    }

    QMetaObject::invokeMethod(&populator, [this, updatedHeaders]() { populator.updateHeaders(updatedHeaders); });
}

QVariant PlaylistModelPrivate::trackData(PlaylistItem* item, int role) const
{
    const auto track = std::get<PlaylistTrackItem>(item->data());

    switch(role) {
        case(PlaylistItem::Role::Left): {
            return track.left();
        }
        case(PlaylistItem::Role::Right): {
            return track.right();
        }
        case(PlaylistItem::Role::Playing): {
            if(isActivePlaylist) {
                return currentPlayingTrack.id() == track.track().id()
                    && currentPlaylist->currentTrackIndex() == item->index();
            }
            return false;
        }
        case(PlaylistItem::Role::ItemData): {
            return QVariant::fromValue<Track>(track.track());
        }
        case(PlaylistItem::Role::Indentation): {
            return item->indentation();
        }
        case(Qt::BackgroundRole): {
            if(!altColours) {
                return QPalette::Base;
            }
            return item->row() & 1 ? QPalette::Base : QPalette::AlternateBase;
        }
        case(Qt::SizeHintRole): {
            return QSize{0, currentPreset.track.rowHeight};
        }
        case(Qt::DecorationRole): {
            switch(currentPlayState) {
                case(PlayState::Playing):
                    return playingIcon;
                case(PlayState::Paused):
                    return pausedIcon;
                case(PlayState::Stopped):
                default:
                    return {};
            }
        }
        default:
            return {};
    }
}

QVariant PlaylistModelPrivate::headerData(PlaylistItem* item, int role) const
{
    const auto& header = std::get<PlaylistContainerItem>(item->data());

    switch(role) {
        case(PlaylistItem::Role::Title): {
            return header.title();
        }
        case(PlaylistItem::Role::ShowCover): {
            return currentPreset.header.showCover;
        }
        case(PlaylistItem::Role::Simple): {
            return currentPreset.header.simple;
        }
        case(PlaylistItem::Role::Cover): {
            if(!currentPreset.header.showCover || !header.trackCount()) {
                return {};
            }
            return coverProvider->trackCover(header.tracks().front(), coverSize, true);
        }
        case(PlaylistItem::Role::Subtitle): {
            return header.subtitle();
        }
        case(PlaylistItem::Role::Info): {
            return header.info();
        }
        case(PlaylistItem::Role::Right): {
            return header.sideText();
        }
        case(Qt::SizeHintRole): {
            return QSize{0, header.rowHeight()};
        }
        default:
            return {};
    }
}

QVariant PlaylistModelPrivate::subheaderData(PlaylistItem* item, int role) const
{
    const auto& header = std::get<PlaylistContainerItem>(item->data());

    switch(role) {
        case(PlaylistItem::Role::Title): {
            return header.title();
        }
        case(PlaylistItem::Role::Subtitle): {
            return header.info();
        }
        case(PlaylistItem::Role::Indentation): {
            return item->indentation();
        }
        case(Qt::SizeHintRole): {
            return QSize{0, header.rowHeight()};
        }
        default:
            return {};
    }
}

PlaylistItem* PlaylistModelPrivate::itemForKey(const QString& key)
{
    if(key == "0"_L1) {
        return model->rootItem();
    }
    if(nodes.contains(key)) {
        return &nodes.at(key);
    }
    return nullptr;
}

bool PlaylistModelPrivate::prepareDrop(const QMimeData* data, Qt::DropAction action, int row, int /*column*/,
                                       const QModelIndex& parent) const
{
    const QByteArray playlistData = data->data(Constants::Mime::PlaylistItems);
    const bool samePlaylist       = dropOnSamePlaylist(playlistData, currentPlaylist);

    const QModelIndex dropIndex = determineDropIndex(model, parent, row);
    const int playlistIndex     = dropIndex.isValid() ? dropIndex.data(PlaylistItem::Index).toInt() : -1;

    if(samePlaylist && action == Qt::MoveAction) {
        const QModelIndexList indexes
            = restoreIndexes(model, data->data(Constants::Mime::PlaylistItems), currentPlaylist);
        const TrackIndexRangeList indexRanges = determineTrackIndexGroups(indexes);

        MoveOperation operation;
        operation.emplace_back(playlistIndex, indexRanges);

        QMetaObject::invokeMethod(model, "tracksMoved", Q_ARG(const MoveOperation&, {operation}));

        return true;
    }

    const TrackList tracks = restoreTracks(library, data->data(Constants::Mime::TrackIds));
    if(tracks.empty()) {
        return false;
    }

    const TrackGroups groups{{playlistIndex, tracks}};
    QMetaObject::invokeMethod(model, "tracksInserted", Q_ARG(const TrackGroups&, groups));

    return true;
}

MoveOperation PlaylistModelPrivate::handleDrop(const MoveOperation& operation)
{
    model->tracksAboutToBeChanged();

    MoveOperation reverseOperation;
    ModelIndexSet headersToCheck;

    std::vector<MoveOperationItemGroup> pendingGroups;

    const MoveOperationMap moveOpGroups = determineMoveOperationGroups(this, operation);

    for(const auto& [index, moveGroups] : moveOpGroups | std::views::reverse) {
        const auto [targetItem, end] = itemForTrackIndex(index);

        const int targetRow            = targetItem->row() + (end ? 1 : 0);
        PlaylistItem* targetParentItem = targetItem->parent();
        QModelIndex targetParent       = model->indexOfItem(targetParentItem);
        const int targetIndex          = targetRow >= targetParentItem->childCount()
                                           ? targetParentItem->childCount()
                                           : targetParentItem->child(targetRow)->index();

        int row = targetRow;

        for(const auto& children : moveGroups) {
            if(children.empty()) {
                continue;
            }

            PlaylistItem* sourceParentItem = children.front()->parent();

            const auto targetResult = findDropTarget(this, sourceParentItem, targetParentItem, row, headersToCheck);

            targetParent     = targetResult.dropTarget();
            targetParentItem = model->itemForIndex(targetParent);

            const int firstRow = children.front()->row();
            const int lastRow  = children.back()->row();

            if(sourceParentItem->key() == targetParentItem->key() && row >= firstRow && row <= lastRow + 1) {
                // Already in right place
                row = lastRow + 1;
                continue;
            }

            const int childCount   = static_cast<int>(children.size());
            const int sourceIndex  = children.front()->index();
            const int reverseIndex = sourceIndex + (sourceIndex > targetIndex ? childCount : 0);

            const QModelIndex sourceParent = model->indexOfItem(sourceParentItem);

            model->beginMoveRows(sourceParent, firstRow, lastRow, targetParent, row);
            row = moveRows(model, sourceParent, children, targetParent, row);
            model->endMoveRows();

            updateTrackIndexes();
            model->rootItem()->resetChildren();

            pendingGroups.emplace_back(reverseIndex, children);
            headersToCheck.emplace(model->indexOfItem(sourceParentItem));
            headersToCheck.emplace(model->indexOfItem(targetParentItem));
        }
    }

    for(const auto& [index, children] : pendingGroups) {
        QModelIndexList childIndexes;
        std::ranges::transform(children, std::back_inserter(childIndexes),
                               [this](const PlaylistItem* child) { return model->indexOfItem(child); });
        const TrackIndexRangeList indexRanges = determineTrackIndexGroups(childIndexes);

        reverseOperation.emplace_back(index, indexRanges);
    }

    cleanupHeaders(headersToCheck);
    updateTrackIndexes();

    model->tracksChanged();

    return reverseOperation;
}

void PlaylistModelPrivate::handleExternalDrop(const PendingData& data)
{
    ModelIndexSet headersToCheck;

    auto* parentItem = itemForKey(data.parent);
    int row          = data.row;

    QModelIndex targetParent{model->indexOfItem(parentItem)};

    std::vector<std::pair<PlaylistItem*, PlaylistItemList>> itemData;

    std::ranges::transform(data.containerOrder, std::inserter(itemData, itemData.end()),
                           [this, &data](const QString& containerKey) {
                               PlaylistItem* item = itemForKey(containerKey);
                               PlaylistItemList children;
                               std::ranges::transform(data.nodes.at(containerKey), std::back_inserter(children),
                                                      [this](const QString& child) { return itemForKey(child); });
                               return std::pair{item, children};
                           });

    // We only care about the immediate track parents
    auto containers = std::views::filter(itemData, [](const auto& entry) {
        return !entry.second.empty() && entry.second.front()->type() == PlaylistItem::Track;
    });

    for(const auto& [sourceParentItem, children] : containers) {
        if(!sourceParentItem) {
            continue;
        }

        PlaylistItem* targetParentItem = model->itemForIndex(targetParent);

        const auto targetResult = findDropTarget(this, sourceParentItem, targetParentItem, row, headersToCheck);
        targetParent            = targetResult.dropTarget();

        const int total = row + static_cast<int>(children.size()) - 1;

        model->beginInsertRows(targetParent, row, total);
        row = insertRows(model, nodes, children, targetParent, row);
        model->endInsertRows();

        headersToCheck.emplace(targetParent);
    }

    model->rootItem()->resetChildren();

    cleanupHeaders(headersToCheck);
    updateTrackIndexes();
}

void PlaylistModelPrivate::handleTrackGroup(const PendingData& data)
{
    ModelIndexSet headersToCheck;

    updateTrackIndexes();

    auto cmpParentKeys = [data](const QString& key1, const QString& key2) {
        if(key1 == key2) {
            return false;
        }
        return std::ranges::find(data.containerOrder, key1) < std::ranges::find(data.containerOrder, key2);
    };
    using ParentItemMap = std::map<QString, PlaylistItemList, decltype(cmpParentKeys)>;
    std::map<int, ParentItemMap> itemData;

    for(const auto& [index, childKeys] : data.indexNodes) {
        ParentItemMap childrenMap(cmpParentKeys);
        for(const QString& childKey : childKeys) {
            if(PlaylistItem* child = itemForKey(childKey)) {
                if(child->parent()) {
                    childrenMap[child->parent()->key()].push_back(child);
                }
            }
        }
        itemData.emplace(index, childrenMap);
    }

    for(const auto& [index, childGroups] : itemData) {
        for(const auto& [sourceParentKey, children] : childGroups) {
            auto* sourceParentItem = itemForKey(sourceParentKey);
            if(!sourceParentItem) {
                continue;
            }

            const auto [beforeIndex, end] = indexForTrackIndex(index);
            QModelIndex targetParent{beforeIndex.parent()};
            int row = beforeIndex.row() + (end ? 1 : 0);

            PlaylistItem* targetParentItem = model->itemForIndex(targetParent);

            const auto targetResult = findDropTarget(this, sourceParentItem, targetParentItem, row, headersToCheck);
            targetParent            = targetResult.dropTarget();

            const int total = row + static_cast<int>(children.size()) - 1;

            model->beginInsertRows(targetParent, row, total);
            insertRows(model, nodes, children, targetParent, row);
            model->endInsertRows();

            headersToCheck.emplace(targetParent);
            updateTrackIndexes();
        }
    }

    model->rootItem()->resetChildren();

    cleanupHeaders(headersToCheck);
    updateTrackIndexes();
}

void PlaylistModelPrivate::storeMimeData(const QModelIndexList& indexes, QMimeData* mimeData) const
{
    if(mimeData) {
        QModelIndexList sortedIndexes{indexes};
        std::ranges::sort(sortedIndexes, cmpTrackIndices);
        mimeData->setData(Constants::Mime::PlaylistItems, saveIndexes(sortedIndexes, currentPlaylist));
        mimeData->setData(Constants::Mime::TrackIds, saveTracks(sortedIndexes));
    }
}

bool PlaylistModelPrivate::insertPlaylistRows(const QModelIndex& target, int firstRow, int lastRow,
                                              const PlaylistItemList& children) const
{
    const int diff = firstRow == lastRow ? 1 : lastRow - firstRow + 1;
    if(static_cast<int>(children.size()) != diff) {
        return false;
    }

    auto* parent = model->itemForIndex(target);

    model->beginInsertRows(target, firstRow, lastRow);
    for(PlaylistItem* child : children) {
        parent->insertChild(firstRow, child);
        child->setPending(false);
        firstRow += 1;
    }
    model->endInsertRows();

    return true;
}

bool PlaylistModelPrivate::movePlaylistRows(const QModelIndex& source, int firstRow, int lastRow,
                                            const QModelIndex& target, int row, const PlaylistItemList& children) const
{
    const int diff = firstRow == lastRow ? 1 : lastRow - firstRow + 1;
    if(static_cast<int>(children.size()) != diff) {
        return false;
    }

    model->beginMoveRows(source, firstRow, lastRow, target, row);
    moveRows(model, source, children, target, row);
    model->endMoveRows();

    return true;
}

bool PlaylistModelPrivate::removePlaylistRows(int row, int count, const QModelIndex& parent)
{
    auto* parentItem = model->itemForIndex(parent);
    if(!parentItem) {
        return false;
    }

    int lastRow = row + count - 1;
    model->beginRemoveRows(parent, row, lastRow);
    while(lastRow >= row) {
        deleteNodes(model->itemForIndex(model->index(lastRow, 0, parent)));
        parentItem->removeChild(lastRow);
        --lastRow;
    }
    parentItem->resetChildren();
    model->endRemoveRows();

    return true;
}

void PlaylistModelPrivate::cleanupHeaders(const ModelIndexSet& headers)
{
    ModelIndexSet cleanedHeaders{headers};

    removeEmptyHeaders(cleanedHeaders);
    mergeHeaders(cleanedHeaders);
    updateHeaders(cleanedHeaders);
}

void PlaylistModelPrivate::removeEmptyHeaders(ModelIndexSet& headers)
{
    std::queue<QModelIndex> headersToCheck;
    for(const QModelIndex& header : headers) {
        if(header.isValid()) {
            headersToCheck.push(header);
        }
    }

    std::set<QString> removedHeaderKeys;

    while(!headersToCheck.empty()) {
        const QModelIndex header{headersToCheck.front()};
        headersToCheck.pop();

        PlaylistItem* headerItem             = model->itemForIndex(header);
        const QModelIndex headerParent       = header.parent();
        const PlaylistItem* headerParentItem = model->itemForIndex(headerParent);

        if(!headerParentItem || removedHeaderKeys.contains(headerItem->key())) {
            continue;
        }

        if(headerItem && headerItem->childCount() < 1) {
            const QModelIndex leftSibling  = header.siblingAtRow(header.row() - 1);
            const QModelIndex rightSibling = header.siblingAtRow(header.row() + 1);

            if(leftSibling.isValid() && model->rowCount(leftSibling) > 0) {
                headers.emplace(leftSibling);
            }
            if(rightSibling.isValid() && model->rowCount(rightSibling) > 0) {
                headers.emplace(rightSibling);
            }

            QModelIndex currentParent = header.isValid() ? header.parent() : header;
            while(currentParent.isValid()) {
                headers.emplace(currentParent);
                currentParent = currentParent.parent();
            }
            headers.emplace(currentParent);

            const int headerRow = headerItem->row();
            removedHeaderKeys.emplace(headerItem->key());
            headers.erase(header);
            removePlaylistRows(headerRow, 1, headerParent);
        }
        if(headerParent.isValid()) {
            headersToCheck.push(headerParent);
        }
    }
    if(!removedHeaderKeys.empty()) {
        model->rootItem()->resetChildren();
    }
}

void PlaylistModelPrivate::mergeHeaders(ModelIndexSet& headersToUpdate)
{
    std::queue<QModelIndex> headers;
    for(const QModelIndex& header : headersToUpdate) {
        headers.push(header);
    }

    while(!headers.empty()) {
        const QModelIndex parent = headers.front();
        headers.pop();

        PlaylistItem* parentItem = model->itemForIndex(parent);
        if(!parentItem || parentItem->childCount() < 1) {
            continue;
        }

        int row{0};

        while(row < parentItem->childCount()) {
            const QModelIndex leftIndex  = model->indexOfItem(parentItem->child(row));
            const QModelIndex rightIndex = model->indexOfItem(parentItem->child(row + 1));

            PlaylistItem* leftSibling  = model->itemForIndex(leftIndex);
            PlaylistItem* rightSibling = model->itemForIndex(rightIndex);

            const bool bothHeaders
                = leftSibling->type() != PlaylistItem::Track && rightSibling->type() != PlaylistItem::Track;

            if(bothHeaders && leftIndex != rightIndex && leftSibling->baseKey() == rightSibling->baseKey()) {
                const auto rightChildren = rightSibling->children();
                const int targetRow      = leftSibling->childCount();
                const int lastRow        = rightSibling->childCount() - 1;

                headersToUpdate.erase(rightIndex);

                movePlaylistRows(rightIndex, 0, lastRow, leftIndex, targetRow, rightChildren);
                removePlaylistRows(rightSibling->row(), 1, rightIndex.parent());

                const int childCount = model->rowCount(leftIndex);
                for(int child{0}; child < childCount; ++child) {
                    const QModelIndex childIndex = model->index(child, 0, leftIndex);
                    headers.push(childIndex);
                }

                headers.push(leftIndex);
                headersToUpdate.emplace(leftIndex);
            }
            else {
                row++;
            }
        }
    }

    model->rootItem()->resetChildren();
}

void PlaylistModelPrivate::updateTrackIndexes()
{
    std::stack<PlaylistItem*> trackNodes;
    trackNodes.push(model->rootItem());
    int currentIndex{0};

    trackIndexes.clear();

    while(!trackNodes.empty()) {
        PlaylistItem* node = trackNodes.top();
        trackNodes.pop();

        if(!node) {
            continue;
        }

        if(node->type() == PlaylistItem::Track) {
            trackIndexes.emplace(currentIndex, node->key());
            node->setIndex(currentIndex++);
        }

        const auto children = node->children();
        for(PlaylistItem* child : children | std::views::reverse) {
            trackNodes.push(child);
        }
    }
}

void PlaylistModelPrivate::deleteNodes(PlaylistItem* node)
{
    if(node) {
        const int count = node->childCount();
        if(count > 0) {
            for(int row{0}; row < count; ++row) {
                if(PlaylistItem* child = node->child(row)) {
                    deleteNodes(child);
                }
            }
        }
        nodes.erase(node->key());
    }
}

void PlaylistModelPrivate::removeTracks(const QModelIndexList& indexes)
{
    const ParentChildRangesList indexGroups = determineRowGroups(indexes);

    model->tracksAboutToBeChanged();

    ModelIndexSet headersToCheck;

    for(const auto& [parent, groups] : indexGroups) {
        for(const auto& children : groups | std::views::reverse) {
            removePlaylistRows(children.first, children.count(), parent);
        }
        headersToCheck.emplace(parent);
    }

    cleanupHeaders(headersToCheck);
    updateTrackIndexes();

    model->tracksChanged();
}

void PlaylistModelPrivate::coverUpdated(const Track& track)
{
    if(!trackParents.contains(track.id())) {
        return;
    }

    const auto parents = trackParents.at(track.id());
    for(const QString& parentKey : parents) {
        if(nodes.contains(parentKey)) {
            auto* parentItem = &nodes.at(parentKey);
            if(parentItem->type() == PlaylistItem::Header) {
                const QModelIndex nodeIndex = model->indexOfItem(parentItem);
                emit model->dataChanged(nodeIndex, nodeIndex, {PlaylistItem::Role::Cover});
            }
        }
    }
}

IndexGroupsList PlaylistModelPrivate::determineIndexGroups(const QModelIndexList& indexes)
{
    IndexGroupsList indexGroups;

    QModelIndexList sortedIndexes{indexes};
    std::ranges::sort(sortedIndexes, cmpTrackIndices);

    QModelIndexList group;

    auto startOfSequence = sortedIndexes.cbegin();
    while(startOfSequence != sortedIndexes.cend()) {
        auto endOfSequence
            = std::adjacent_find(startOfSequence, sortedIndexes.cend(), [](const auto& lhs, const auto& rhs) {
                  return lhs.parent() != rhs.parent() || rhs.row() != lhs.row() + 1;
              });
        if(endOfSequence != sortedIndexes.cend()) {
            std::advance(endOfSequence, 1);
        }

        group.resize(0);
        for(auto it = startOfSequence; it != endOfSequence; ++it) {
            group.push_back(*it);
        }

        indexGroups.push_back(group);

        startOfSequence = endOfSequence;
    }

    return indexGroups;
}

TrackIndexResult PlaylistModelPrivate::indexForTrackIndex(int index)
{
    if(trackIndexes.contains(index)) {
        const QString key = trackIndexes.at(index);
        if(nodes.contains(key)) {
            return {model->indexOfItem(&nodes.at(key)), false};
        }
    }

    // End of playlist - return last track index
    PlaylistItem* current = model->itemForIndex({});
    while(current->childCount() > 0) {
        current = current->child(current->childCount() - 1);
    }
    return {model->indexOfItem(current), true};
}

TrackItemResult PlaylistModelPrivate::itemForTrackIndex(int index)
{
    if(trackIndexes.contains(index)) {
        const QString key = trackIndexes.at(index);
        if(nodes.contains(key)) {
            return {&nodes.at(key), false};
        }
    }

    // End of playlist - return last track item
    PlaylistItem* current = model->itemForIndex({});
    while(current->childCount() > 0) {
        current = current->child(current->childCount() - 1);
    }
    return {current, true};
}
} // namespace Fooyin
