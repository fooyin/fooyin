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

#include "playlistmodel_p.h"

#include "internalguisettings.h"
#include "playlistscriptregistry.h"

#include <core/library/musiclibrary.h>
#include <core/player/playermanager.h>
#include <core/playlist/playlist.h>
#include <gui/coverprovider.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <utils/crypto.h>
#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>

#include <QFontMetrics>
#include <QIODevice>
#include <QIcon>
#include <QMimeData>
#include <QPalette>

#include <queue>
#include <set>
#include <span>
#include <stack>

constexpr auto CellMargin = 5;

namespace {
bool cmpTrackIndices(const QModelIndex& index1, const QModelIndex& index2)
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

int determineDropIndex(Fooyin::PlaylistModel* model, const QModelIndex& parent, int row)
{
    if(!parent.isValid() && row >= model->rowCount(parent)) {
        return -1;
    }

    const auto getPlaylistIndex = [](const QModelIndex& index) {
        return index.data(Fooyin::PlaylistItem::Index).toInt();
    };

    if(parent.isValid() && model->hasIndex(row, 0, parent)) {
        return getPlaylistIndex(model->index(row, 0, parent));
    }

    if(row == model->rowCount(parent)) {
        const QModelIndex prevIndex = model->index(row - 1, 0, parent);
        return getPlaylistIndex(prevIndex) + 1;
    }

    QModelIndex current = model->index(row, 0, parent);
    while(model->hasChildren(current)) {
        current = model->index(0, 0, current);
    }

    return getPlaylistIndex(current);
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

Fooyin::ItemPtrSet optimiseHeaders(const Fooyin::ItemPtrSet& selection)
{
    using Fooyin::PlaylistItem;

    std::queue<PlaylistItem*> stack;

    for(PlaylistItem* index : selection) {
        stack.push(index);
    }

    Fooyin::ItemPtrSet optimisedSelection;
    Fooyin::ItemPtrSet selectedParents;

    while(!stack.empty()) {
        PlaylistItem* current = stack.front();
        stack.pop();
        PlaylistItem* parent    = current->parent();
        const bool parentIsRoot = parent->type() == Fooyin::PlaylistItem::Root;

        if(selection.contains(parent) || selectedParents.contains(parent)) {
            continue;
        }

        bool allChildrenSelected{true};

        if(!parentIsRoot) {
            const int rowCount = parent->childCount();
            for(int row{0}; row < rowCount; ++row) {
                PlaylistItem* child = parent->child(row);
                if(!selection.contains(child) && !selectedParents.contains(child)) {
                    allChildrenSelected = false;
                    break;
                }
            }
        }

        if(!allChildrenSelected || parentIsRoot) {
            optimisedSelection.emplace(current);
        }
        else if(!optimisedSelection.contains(parent)) {
            selectedParents.emplace(parent);
            stack.push(parent);
        }
    }

    return optimisedSelection;
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
    std::ranges::sort(sortedIndexes, cmpTrackIndices);

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

Fooyin::TrackIndexRangeList determineTrackIndexGroups(const QModelIndexList& indexes, const QModelIndex& target = {},
                                                      int row = -1)
{
    using Fooyin::PlaylistItem;

    Fooyin::TrackIndexRangeList indexGroups;

    QModelIndexList sortedIndexes{indexes};
    std::ranges::sort(sortedIndexes, cmpTrackIndices);

    const auto getIndex = [](const QModelIndex& index) {
        return index.data(PlaylistItem::Index).toInt();
    };

    bool previousSplit{false};

    auto startOfSequence = sortedIndexes.cbegin();
    while(startOfSequence != sortedIndexes.cend()) {
        auto endOfSequence
            = std::adjacent_find(startOfSequence, sortedIndexes.cend(), [](const auto& lhs, const auto& rhs) {
                  return lhs.parent() != rhs.parent() || rhs.row() != lhs.row() + 1;
              });
        if(endOfSequence != sortedIndexes.cend()) {
            std::advance(endOfSequence, 1);
        }

        const bool shouldSplit = row >= 0 && previousSplit && startOfSequence->parent() == target
                              && row >= startOfSequence->row() && row <= std::prev(endOfSequence)->row();
        if(shouldSplit) {
            const auto range   = std::span{startOfSequence, endOfSequence};
            const auto splitIt = std::ranges::find_if(range, [row](const auto& index) { return index.row() >= row; });

            auto beforeRow = std::ranges::subrange(range.begin(), splitIt);
            auto afterRow  = std::ranges::subrange(splitIt, range.end());

            if(!beforeRow.empty()) {
                indexGroups.emplace_back(getIndex(*beforeRow.begin()), getIndex(*std::prev(beforeRow.end())));
            }
            if(!afterRow.empty()) {
                indexGroups.emplace_back(getIndex(*afterRow.begin()), getIndex(*std::prev(afterRow.end())));
            }
        }
        else {
            indexGroups.emplace_back(getIndex(*startOfSequence), getIndex(*std::prev(endOfSequence)));
        }

        if(!previousSplit) {
            previousSplit = startOfSequence->parent() != target;
        }
        startOfSequence = endOfSequence;
    }

    return indexGroups;
}

using MoveOperationItemGroups = std::vector<Fooyin::PlaylistItemList>;

struct MoveOperationTargetGroups
{
    int index;
    MoveOperationItemGroups groups;
};

using MoveOperationMap = std::vector<MoveOperationTargetGroups>;

MoveOperationMap determineMoveOperationGroups(Fooyin::PlaylistModelPrivate* model,
                                              const Fooyin::MoveOperation& operation, bool merge = true)
{
    MoveOperationMap result;

    for(const auto& [index, tracks] : operation) {
        for(const auto& range : tracks) {
            Fooyin::PlaylistItemList rows;
            rows.reserve(range.count());

            for(int trackIndex{range.first}; trackIndex <= range.last; ++trackIndex) {
                rows.push_back(model->itemForTrackIndex(trackIndex).item);
            }

            auto groupIt = std::ranges::find_if(result, [index](const auto& group) { return group.index == index; });
            if(merge && groupIt != result.end()) {
                groupIt->groups.push_back(rows);
            }
            else {
                result.emplace_back(index, MoveOperationItemGroups{rows});
            }
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
    Fooyin::PlaylistItem* source;
    Fooyin::PlaylistItem* target;
    int firstRow{0};
    int finalRow{0};
    std::vector<Fooyin::PlaylistItem*> children;
};

DropTargetResult findDropTarget(Fooyin::PlaylistModelPrivate* self, Fooyin::PlaylistItem* source,
                                Fooyin::PlaylistItem* target, int& row)
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
    if(canMergeRight && !dropResult.partMergeTarget.isValid()) {
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
                PlaylistItemList parentChildren = parent->children();
                const auto childrenToMove       = parentChildren | std::views::drop(row);
                std::ranges::copy(childrenToMove, std::back_inserter(children));

                splitParents.emplace_back(parent, newParent, row, finalRow, children);
            }
            row = parent->row() + (row > 0 ? 1 : 0);
        }

        std::ranges::reverse(splitParents);

        // Move tracks after drop index to new parents
        for(const SplitParent& parent : splitParents) {
            const QModelIndex prevParent = self->model->indexOfItem(prevParentItem);

            self->insertPlaylistRows(prevParent, newParentRow, newParentRow, {parent.target});

            const QModelIndex oldParentIndex = self->model->indexOfItem(parent.source);
            const QModelIndex newParentIndex = self->model->indexOfItem(parent.target);
            self->movePlaylistRows(oldParentIndex, parent.firstRow, parent.finalRow, newParentIndex, 0,
                                   parent.children);

            prevParentItem = parent.target;
            newParentRow   = 0;
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
PlaylistModelPrivate::PlaylistModelPrivate(PlaylistModel* model_, MusicLibrary* library_, SettingsManager* settings_)
    : model{model_}
    , library{library_}
    , settings{settings_}
    , coverProvider{new CoverProvider(model)}
    , resetting{false}
    , altColours{settings->value<Settings::Gui::Internal::PlaylistAltColours>()}
    , coverSize{settings->value<Settings::Gui::Internal::PlaylistThumbnailSize>(),
                settings->value<Settings::Gui::Internal::PlaylistThumbnailSize>()}
    , isActivePlaylist{false}
    , currentPlaylist{nullptr}
    , currentPlayState{PlayState::Stopped}
    , currentIndex{-1}
{
    auto updateIcons = [this]() {
        playingIcon = QIcon::fromTheme(Constants::Icons::Play).pixmap(20);
        pausedIcon  = QIcon::fromTheme(Constants::Icons::Pause).pixmap(20);
        missingIcon = QIcon::fromTheme(Constants::Icons::Close).pixmap(15);
    };

    updateIcons();

    settings->subscribe<Settings::Gui::IconTheme>(model, updateIcons);

    populator.moveToThread(&populatorThread);
    populatorThread.start();
}

void PlaylistModelPrivate::populateModel(PendingData& data)
{
    if(currentPlaylist && currentPlaylist->id() != data.playlistId) {
        return;
    }

    if(resetting) {
        model->beginResetModel();
        model->resetRoot();
        nodes.clear();
        pendingNodes.clear();
        trackParents.clear();
    }

    nodes.merge(data.items);
    trackParents.merge(data.trackParents);

    if(resetting) {
        for(const auto& [parentKey, rows] : data.nodes) {
            auto* parent = parentKey == QStringLiteral("0") ? model->itemForIndex({}) : &nodes.at(parentKey);

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

void PlaylistModelPrivate::populateTrackGroup(PendingData& data)
{
    if(currentPlaylist && currentPlaylist->id() != data.playlistId) {
        return;
    }

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
            nodes[key] = header;
            auto* node = &nodes.at(key);
            node->setState(PlaylistItem::State::None);
            const QModelIndex headerIndex = model->indexOfItem(node);
            emit model->dataChanged(headerIndex, headerIndex, {});
        }
    }
}

QVariant PlaylistModelPrivate::trackData(PlaylistItem* item, int column, int role) const
{
    const auto track = std::get<PlaylistTrackItem>(item->data());

    auto isPlaying = [this, &track, item]() {
        return isActivePlaylist && currentPlayingTrack.id() == track.track().id()
            && currentPlaylist->currentTrackIndex() == item->index();
    };

    switch(role) {
        case(Qt::DisplayRole): {
            if(column >= 0 && column < static_cast<int>(track.left().size())) {
                return track.left().at(column).text;
            }
            return {};
        }
        case(PlaylistItem::Role::Left): {
            return track.left();
        }
        case(PlaylistItem::Role::Right): {
            return track.right();
        }
        case(PlaylistItem::Role::Playing): {
            return isPlaying();
        }
        case(PlaylistItem::Role::ItemData): {
            return QVariant::fromValue<Track>(track.track());
        }
        case(PlaylistItem::Role::Indentation): {
            return item->indentation();
        }
        case(PlaylistItem::Role::CellMargin): {
            return CellMargin;
        }
        case(PlaylistItem::Role::Enabled): {
            return track.track().enabled();
        }
        case(Qt::BackgroundRole): {
            if(!altColours) {
                return QPalette::Base;
            }
            return item->row() & 1 ? QPalette::Base : QPalette::AlternateBase;
        }
        case(Qt::SizeHintRole): {
            if(!columns.empty() && column >= 0 && column < static_cast<int>(track.left().size())) {
                const QFontMetrics fm{track.left().at(column).font};
                QRect rect = fm.boundingRect(track.left().at(column).text);
                rect.setWidth(rect.width() + (2 * CellMargin + 1));
                rect.setHeight(currentPreset.track.rowHeight);

                return rect.size();
            }
            return QSize{0, currentPreset.track.rowHeight};
        }
        case(Qt::DecorationRole): {
            if(columns.empty() || columns.at(column).field == PlayingIcon) {
                if(!track.track().enabled()) {
                    return missingIcon;
                }

                if(isPlaying()) {
                    switch(currentPlayState) {
                        case(PlayState::Playing):
                            return playingIcon;
                        case(PlayState::Paused):
                            return pausedIcon;
                        case(PlayState::Stopped):
                            return {};
                    }
                }
            }

            return {};
        }
        default:
            return {};
    }
}

QVariant PlaylistModelPrivate::headerData(PlaylistItem* item, int column, int role) const
{
    if(column != 0) {
        return {};
    }

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

QVariant PlaylistModelPrivate::subheaderData(PlaylistItem* item, int column, int role) const
{
    if(column != 0) {
        return {};
    }

    const auto& header = std::get<PlaylistContainerItem>(item->data());

    switch(role) {
        case(PlaylistItem::Role::Title): {
            return header.title();
        }
        case(PlaylistItem::Role::Subtitle): {
            return header.subtitle();
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
    if(key == QStringLiteral("0")) {
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
    const int dropIndex = determineDropIndex(model, parent, row);

    if(data->hasUrls()) {
        QMetaObject::invokeMethod(model, "filesDropped", Q_ARG(const QList<QUrl>&, data->urls()),
                                  Q_ARG(int, dropIndex));
        return true;
    }

    const QByteArray playlistData = data->data(Constants::Mime::PlaylistItems);
    const bool samePlaylist       = dropOnSamePlaylist(playlistData, currentPlaylist);

    if(samePlaylist && action == Qt::MoveAction) {
        const QModelIndexList indexes
            = restoreIndexes(model, data->data(Constants::Mime::PlaylistItems), currentPlaylist);
        const TrackIndexRangeList indexRanges = determineTrackIndexGroups(indexes, parent, row);

        const bool validMove = !std::ranges::all_of(indexRanges, [dropIndex](const auto& range) {
            return (dropIndex >= range.first && dropIndex <= range.last + 1);
        });

        if(!validMove) {
            return false;
        }

        MoveOperation operation;
        operation.emplace_back(dropIndex, indexRanges);

        QMetaObject::invokeMethod(model, "tracksMoved", Q_ARG(const MoveOperation&, {operation}));

        return true;
    }

    const TrackList tracks = restoreTracks(library, data->data(Constants::Mime::TrackIds));
    if(tracks.empty()) {
        return false;
    }

    const TrackGroups groups{{dropIndex, tracks}};
    QMetaObject::invokeMethod(model, "tracksInserted", Q_ARG(const TrackGroups&, groups));

    return true;
}

MoveOperation PlaylistModelPrivate::handleMove(const MoveOperation& operation)
{
    MoveOperation reverseOperation;
    MoveOperationMap pendingGroups;

    const MoveOperationMap moveOpGroups = determineMoveOperationGroups(this, operation);

    model->tracksAboutToBeChanged();

    for(const auto& [index, moveGroups] : moveOpGroups | std::views::reverse) {
        const auto [targetItem, end] = itemForTrackIndex(index);

        const int targetRow            = end ? model->rowCount({}) : targetItem->row();
        PlaylistItem* targetParentItem = end ? model->rootItem() : targetItem->parent();
        QModelIndex targetParent       = model->indexOfItem(targetParentItem);
        const int targetIndex          = end ? targetItem->index() + 1
                                       : targetRow >= targetParentItem->childCount()
                                           ? targetParentItem->childCount()
                                           : targetParentItem->child(targetRow)->index();

        int row = targetRow;

        for(const auto& children : moveGroups) {
            if(children.empty()) {
                continue;
            }

            PlaylistItem* sourceParentItem = children.front()->parent();

            const auto targetResult = findDropTarget(this, sourceParentItem, targetParentItem, row);

            targetParent     = targetResult.dropTarget();
            targetParentItem = model->itemForIndex(targetParent);

            const int firstRow = children.front()->row();
            const int lastRow  = children.back()->row();

            if(sourceParentItem->key() == targetParentItem->key() && row >= firstRow && row <= lastRow + 1) {
                // Already in the right place
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

            pendingGroups.emplace_back(reverseIndex, MoveOperationItemGroups{children});
        }
    }

    for(const auto& [index, groups] : pendingGroups) {
        for(const auto& children : groups) {
            QModelIndexList childIndexes;
            std::ranges::transform(children, std::back_inserter(childIndexes),
                                   [this](const PlaylistItem* child) { return model->indexOfItem(child); });
            const TrackIndexRangeList indexRanges = determineTrackIndexGroups(childIndexes);

            reverseOperation.emplace_back(index, indexRanges);
        }
    }

    cleanupHeaders();
    updateTrackIndexes();

    model->tracksChanged();

    return reverseOperation;
}

void PlaylistModelPrivate::handleTrackGroup(const PendingData& data)
{
    ItemPtrSet headersToCheck;

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

            const auto targetResult = findDropTarget(this, sourceParentItem, targetParentItem, row);
            targetParent            = targetResult.dropTarget();

            const int total = row + static_cast<int>(children.size()) - 1;

            model->beginInsertRows(targetParent, row, total);
            insertRows(model, nodes, children, targetParent, row);
            model->endInsertRows();

            updateTrackIndexes();
        }
    }

    model->rootItem()->resetChildren();

    cleanupHeaders();
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

    const int rowCount = model->rowCount(parent);
    if(row < 0 || (row + count - 1) >= rowCount) {
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

void PlaylistModelPrivate::fetchChildren(PlaylistItem* parent, PlaylistItem* child)
{
    const QString key = child->key();

    if(pendingNodes.contains(key)) {
        auto& childRows = pendingNodes.at(key);

        for(const QString& childRow : childRows) {
            PlaylistItem& childItem = nodes.at(childRow);
            fetchChildren(child, &childItem);
        }
        pendingNodes.erase(key);
    }

    parent->appendChild(child);
}

void PlaylistModelPrivate::cleanupHeaders()
{
    removeEmptyHeaders();
    mergeHeaders();
    updateHeaders();
}

void PlaylistModelPrivate::removeEmptyHeaders()
{
    ItemPtrSet headersToRemove;
    for(auto& [_, node] : nodes) {
        if(node.state() == PlaylistItem::State::Delete) {
            headersToRemove.emplace(&node);
        }
    }

    if(headersToRemove.empty()) {
        return;
    }

    const ItemPtrSet topLevelHeaders = optimiseHeaders(headersToRemove);

    for(PlaylistItem* item : topLevelHeaders) {
        const QModelIndex index = model->indexOfItem(item);
        removePlaylistRows(index.row(), 1, index.parent());
    }
}

void PlaylistModelPrivate::mergeHeaders()
{
    std::queue<PlaylistItem*> headers;
    headers.emplace(model->rootItem());

    auto addChildren = [&headers](PlaylistItem* parent) {
        const auto children = parent->children();
        for(PlaylistItem* child : children) {
            headers.push(child);
        }
    };

    while(!headers.empty()) {
        const PlaylistItem* parent = headers.front();
        headers.pop();

        if(!parent || parent->childCount() < 1) {
            continue;
        }

        int row{0};
        const int childCount = parent->childCount();

        while(row < childCount) {
            PlaylistItem* leftSibling  = parent->child(row);
            PlaylistItem* rightSibling = parent->child(row + 1);

            if(leftSibling) {
                headers.push(leftSibling);
                addChildren(leftSibling);
            }

            if(!leftSibling || !rightSibling) {
                ++row;
                continue;
            }

            const QModelIndex leftIndex  = model->indexOfItem(leftSibling);
            const QModelIndex rightIndex = model->indexOfItem(rightSibling);

            const bool bothHeaders
                = leftSibling->type() != PlaylistItem::Track && rightSibling->type() != PlaylistItem::Track;

            if(bothHeaders && leftIndex != rightIndex && leftSibling->baseKey() == rightSibling->baseKey()) {
                const auto rightChildren = rightSibling->children();
                const int targetRow      = leftSibling->childCount();
                const int lastRow        = rightSibling->childCount() - 1;

                movePlaylistRows(rightIndex, 0, lastRow, leftIndex, targetRow, rightChildren);
                removePlaylistRows(rightSibling->row(), 1, rightIndex.parent());
            }
            else {
                ++row;
            }
        }
    }

    model->rootItem()->resetChildren();
}

void PlaylistModelPrivate::updateHeaders()
{
    ItemPtrSet items;

    auto headersToUpdate = nodes | std::views::filter([](const auto& pair) {
                               return pair.second.state() == PlaylistItem::State::Update;
                           });

    for(auto& [_, header] : headersToUpdate) {
        PlaylistItem* currHeader{&header};
        while(currHeader->type() != PlaylistItem::Root) {
            if(currHeader->childCount() > 0) {
                items.emplace(currHeader);
            }
            currHeader = currHeader->parent();
        }
    }

    ItemList updatedHeaders;

    for(PlaylistItem* header : items) {
        updateHeaderChildren(header);
        updatedHeaders.emplace_back(*header);
    }

    QMetaObject::invokeMethod(&populator, [this, updatedHeaders]() { populator.updateHeaders(updatedHeaders); });
}

void PlaylistModelPrivate::updateTrackIndexes()
{
    std::stack<PlaylistItem*> trackNodes;
    trackNodes.push(model->rootItem());
    int index{0};

    trackIndexes.clear();

    while(!trackNodes.empty()) {
        PlaylistItem* node = trackNodes.top();
        trackNodes.pop();

        if(!node) {
            continue;
        }

        if(node->type() == PlaylistItem::Track) {
            trackIndexes.emplace(index, node->key());
            node->setIndex(index++);
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

    for(const auto& [parent, groups] : indexGroups) {
        for(const auto& children : groups | std::views::reverse) {
            removePlaylistRows(children.first, children.count(), parent);
        }
    }

    cleanupHeaders();
    updateTrackIndexes();
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

TrackIndexResult PlaylistModelPrivate::indexForTrackIndex(int index) const
{
    while(!trackIndexes.contains(index) && model->canFetchMore({})) {
        model->fetchMore({});
    }

    if(trackIndexes.contains(index)) {
        const QString key = trackIndexes.at(index);
        if(nodes.contains(key)) {
            auto& item = nodes.at(key);
            return {model->indexOfItem(&item), false};
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
    while(!trackIndexes.contains(index) && model->canFetchMore({})) {
        model->fetchMore({});
    }

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
