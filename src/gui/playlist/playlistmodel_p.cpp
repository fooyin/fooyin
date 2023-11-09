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

#include "playlistmodel.h"

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
using Fy::Gui::Widgets::Playlist::PlaylistItem;
using Fy::Gui::Widgets::Playlist::PlaylistModel;

void optimiseSelection(QAbstractItemModel* model, QModelIndexList& selection)
{
    QModelIndexList optimisedSelection;

    for(const QModelIndex& index : selection) {
        const QModelIndex parent = index.parent();
        bool allChildrenSelected{true};

        if(parent.isValid()) {
            const int rowCount = model->rowCount(parent);
            for(int row{0}; row < rowCount; ++row) {
                const QModelIndex child = model->index(row, 0, parent);
                if(!selection.contains(child)) {
                    allChildrenSelected = false;
                    break;
                }
            }
        }

        if(!allChildrenSelected || !parent.isValid()) {
            optimisedSelection.append(index);
        }
        else if(!optimisedSelection.contains(parent)) {
            optimisedSelection.append(parent);
        }
    }

    if(optimisedSelection.size() != selection.size()) {
        selection = optimisedSelection;
        optimiseSelection(model, selection);
    }
}

QByteArray saveTracks(const QModelIndexList& indexes)
{
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);

    Fy::Core::TrackList tracks;
    tracks.reserve(indexes.size());

    std::ranges::transform(indexes, std::back_inserter(tracks), [](const QModelIndex& index) {
        return index.data(Fy::Gui::Widgets::Playlist::PlaylistItem::Role::ItemData).value<Fy::Core::Track>();
    });

    stream << tracks;

    return result;
}

Fy::Core::TrackList restoreTracks(QByteArray data)
{
    Fy::Core::TrackList result;
    QDataStream stream(&data, QIODevice::ReadOnly);

    stream >> result;

    return result;
}

QByteArray saveIndexes(const QModelIndexList& indexes, Fy::Core::Playlist::Playlist* playlist)
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

QModelIndexList restoreIndexes(QAbstractItemModel* model, QByteArray data, Fy::Core::Playlist::Playlist* playlist)
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
        int childDepth = 0;
        stream >> childDepth;

        QModelIndex currentIndex;
        for(int i = 0; i < childDepth; ++i) {
            int row = 0;
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
int moveRows(PlaylistModel* model, const QModelIndex& source, const Container& rows, const QModelIndex& target, int row)
{
    int currRow{row};
    auto* targetParent = model->itemForIndex(target);
    if(!targetParent) {
        return currRow;
    }

    auto* sourceParent = model->itemForIndex(source);
    for(PlaylistItem* childItem : rows) {
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
int copyRows(PlaylistModel* model, Fy::Gui::Widgets::Playlist::ItemKeyMap& nodes, const QModelIndex& source,
             const Container& rows, const QModelIndex& target, int row)
{
    int currRow{row};
    auto* targetParent = model->itemForIndex(target);
    if(!targetParent) {
        return currRow;
    }

    auto* sourceParent = model->itemForIndex(source);
    for(PlaylistItem* childItem : rows) {
        childItem->resetRow();
        auto* newChild = &nodes.emplace(Fy::Utils::generateRandomHash(), *childItem).first->second;
        targetParent->insertChild(currRow, newChild);
        ++currRow;
    }
    sourceParent->resetChildren();
    targetParent->resetChildren();

    return currRow;
}

template <typename Container>
int insertRows(PlaylistModel* model, Fy::Gui::Widgets::Playlist::ItemKeyMap& nodes, const Container& rows,
               const QModelIndex& target, int row)
{
    auto* targetParent = model->itemForIndex(target);
    if(!targetParent) {
        return row;
    }

    for(PlaylistItem* childItem : rows) {
        childItem->resetRow();
        auto* newChild = &nodes.emplace(childItem->key(), *childItem).first->second;
        targetParent->insertChild(row, newChild);
        newChild->setPending(false);
        row++;
    }
    targetParent->resetChildren();

    return row;
}

bool cmpItemsReverse(PlaylistItem* pItem1, PlaylistItem* pItem2)
{
    PlaylistItem* item1{pItem1};
    PlaylistItem* item2{pItem2};

    while(item1->parent() != item2->parent()) {
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
        return true;
    }
    return item1->row() > item2->row();
};

struct cmpItems
{
    bool operator()(PlaylistItem* pItem1, PlaylistItem* pItem2) const
    {
        return cmpItemsReverse(pItem1, pItem2);
    }
};

using ItemPtrSet = std::set<PlaylistItem*, cmpItems>;

void updateHeaderChildren(PlaylistItem* header)
{
    if(!header) {
        return;
    }

    const auto type = header->type();

    if(type == PlaylistItem::Header || type == PlaylistItem::Subheader) {
        Fy::Gui::Widgets::Playlist::Container& container = std::get<1>(header->data());
        container.clearTracks();

        const auto& children = header->children();
        for(PlaylistItem* child : children) {
            if(child->type() == PlaylistItem::Track) {
                const Fy::Core::Track& track = std::get<0>(child->data()).track();
                container.addTrack(track);
            }
            else {
                const Fy::Core::TrackList tracks = std::get<1>(child->data()).tracks();
                container.addTracks(tracks);
            }
        }
    }
}

struct SplitParent
{
    QModelIndex source;
    PlaylistItem* target;
    int firstRow{0};
    int finalRow{0};
    std::vector<PlaylistItem*> children;
};
} // namespace

namespace Fy::Gui::Widgets::Playlist {
PlaylistModelPrivate::PlaylistModelPrivate(PlaylistModel* model, Utils::SettingsManager* settings)
    : model{model}
    , settings{settings}
    , coverProvider{new Library::CoverProvider(model)}
    , resetting{false}
    , playingIcon{QIcon::fromTheme(Constants::Icons::Play).pixmap(20)}
    , pausedIcon{QIcon::fromTheme(Constants::Icons::Pause).pixmap(20)}
    , altColours{settings->value<Settings::PlaylistAltColours>()}
    , coverSize{settings->value<Settings::PlaylistThumbnailSize>(), settings->value<Settings::PlaylistThumbnailSize>()}
    , isActivePlaylist{false}
    , currentPlaylist{nullptr}
    , currentPlayState{Core::Player::PlayState::Stopped}
{
    populator.moveToThread(&populatorThread);
    populatorThread.start();
}

void PlaylistModelPrivate::populateModel(PendingData& data)
{
    if(resetting) {
        model->beginResetModel();
        model->resetRoot();
        beginReset();
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

void PlaylistModelPrivate::updateHeaders(const QModelIndexList& headers)
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

void PlaylistModelPrivate::beginReset()
{
    // Acts as a cache in case model hasn't been fully cleared
    oldNodes = std::move(nodes);
    nodes.clear();
    pendingNodes.clear();
    trackParents.clear();
}

QVariant PlaylistModelPrivate::trackData(PlaylistItem* item, int role) const
{
    const auto track = std::get<Track>(item->data());

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
            return QVariant::fromValue<Core::Track>(track.track());
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
                case(Core::Player::PlayState::Playing):
                    return playingIcon;
                case(Core::Player::PlayState::Paused):
                    return pausedIcon;
                case(Core::Player::PlayState::Stopped):
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
    const auto& header = std::get<Container>(item->data());

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
    const auto& header = std::get<Container>(item->data());

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

PlaylistItem* PlaylistModelPrivate::cloneParent(PlaylistItem* parent)
{
    const QString parentKey = Utils::generateRandomHash();
    auto* newParent         = &nodes.emplace(parentKey, *parent).first->second;
    newParent->setKey(parentKey);
    newParent->resetRow();
    newParent->clearChildren();

    return newParent;
}

MergeResult PlaylistModelPrivate::canBeMerged(PlaylistItem*& currTarget, int& targetRow,
                                              std::vector<PlaylistItem*>& sourceParents, int targetOffset) const
{
    PlaylistItem* checkItem = currTarget->child(targetRow + targetOffset);

    if(!checkItem) {
        return {};
    }

    if(sourceParents.empty() || sourceParents.front()->baseKey() != checkItem->baseKey()) {
        return {};
    }

    std::vector<PlaylistItem*> newSourceParents;
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
        return {.fullMergeTarget = model->indexOfItem(checkItem), .partMergeTarget = {}};
    }

    sourceParents = newSourceParents;
    return {.fullMergeTarget = {}, .partMergeTarget = model->indexOfItem(currTarget)};
}

bool PlaylistModelPrivate::handleDrop(const QMimeData* data, Qt::DropAction action, int row, int /*column*/,
                                      const QModelIndex& parent)
{
    if(!parent.isValid() && row < 0) {
        row = 0;
    }

    model->tracksAboutToBeChanged();

    const QModelIndexList indexes = restoreIndexes(model, data->data(Constants::Mime::PlaylistItems), currentPlaylist);
    if(indexes.isEmpty()) {
        const Core::TrackList tracks = restoreTracks(data->data(Constants::Mime::TrackList));
        if(tracks.empty()) {
            return false;
        }
        QMetaObject::invokeMethod(&populator, [this, tracks, parent, row] {
            populator.runTracks(currentPreset, tracks, model->itemForIndex(parent)->key(), row);
        });
        return true;
    }

    const ParentChildItemGroupMap indexItemGroups = determineItemGroups(model, indexes);

    QModelIndexList headersToCheck;
    QModelIndex targetParent{parent};

    for(const auto& [sourceParent, groups] : indexItemGroups) {
        PlaylistItem* sourceParentItem = model->itemForIndex(sourceParent);
        PlaylistItem* targetParentItem = model->itemForIndex(targetParent);

        const bool sameParents = sourceParentItem->baseKey() == targetParentItem->baseKey();

        if(!sameParents) {
            targetParent     = handleDiffParentDrop(sourceParentItem, targetParentItem, row, headersToCheck);
            targetParentItem = model->itemForIndex(targetParent);
        }

        for(const auto& childrenGroup : groups) {
            // If dropped within a group and a previous groups parents were different,
            // the children will be split and have different parents.
            // Regroup to prevent issues with beginMoveRows/beginInsertRows.
            const auto regroupedChildren = groupChildren(model, childrenGroup);
            for(const auto& [groupParent, children] : regroupedChildren) {
                if(action == Qt::MoveAction) {
                    const int firstRow = children.front()->row();
                    const int lastRow  = children.back()->row();

                    auto* groupParentItem = model->itemForIndex(groupParent);

                    const bool identicalParents = groupParentItem->key() == targetParentItem->key();

                    if(identicalParents && row >= firstRow && row <= lastRow + 1) {
                        row = lastRow + 1;
                        continue;
                    }

                    model->beginMoveRows(groupParent, firstRow, lastRow, targetParent, row);
                    row = moveRows(model, groupParent, children, targetParent, row);
                    model->endMoveRows();
                }
                else {
                    const int total = row + static_cast<int>(children.size()) - 1;

                    model->beginInsertRows(targetParent, row, total);
                    row = copyRows(model, nodes, groupParent, children, targetParent, row);
                    model->endInsertRows();
                }
                headersToCheck.emplace_back(groupParent);
            }
        }
        headersToCheck.append(targetParent);

        model->rootItem()->resetChildren();

        removeEmptyHeaders(headersToCheck);
        mergeHeaders(headersToCheck);
    }

    updateTrackIndexes();
    updateHeaders(headersToCheck);

    model->tracksChanged();

    return true;
}

QModelIndex PlaylistModelPrivate::handleDiffParentDrop(PlaylistItem* source, PlaylistItem* target, int& row,
                                                       QModelIndexList& headersToUpdate)
{
    int targetRow{row};
    std::vector<PlaylistItem*> sourceParents;
    std::vector<PlaylistItem*> targetParents;

    PlaylistItem* currSource{source};
    PlaylistItem* currTarget{target};

    // Find common ancestor
    while(currSource->baseKey() != currTarget->baseKey()) {
        if(currSource->type() != PlaylistItem::Root) {
            sourceParents.push_back(currSource);
            currSource = currSource->parent();
        }
        if(currTarget->type() != PlaylistItem::Root) {
            targetParents.push_back(currTarget);
            targetRow  = currTarget->row();
            currTarget = currTarget->parent();
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

    MergeResult mergeResult;

    // Check left
    if(canMergeleft) {
        mergeResult = canBeMerged(currTarget, targetRow, sourceParents, -1);
        if(mergeResult.fullMergeTarget.isValid()) {
            row = targetRow + 1;
            return mergeResult.fullMergeTarget;
        }
        if(mergeResult.partMergeTarget.isValid()) {
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
        mergeResult = canBeMerged(currTarget, targetRow, sourceParents, targetIsRoot ? 0 : 1);
        if(mergeResult.fullMergeTarget.isValid()) {
            row = 0;
            return mergeResult.fullMergeTarget;
        }
        if(mergeResult.partMergeTarget.isValid()) {
            row = 0;
        }
    }

    int newParentRow = targetRow + 1;
    PlaylistItem* prevParentItem{currTarget};

    if(!mergeResult.partMergeTarget.isValid()) {
        // Create parents for tracks after drop index (if any)
        std::vector<SplitParent> splitParents;

        for(PlaylistItem* parent : targetParents) {
            const int finalRow = parent->childCount() - 1;
            if(finalRow >= row || !splitParents.empty()) {
                PlaylistItem* newParent = cloneParent(parent);

                std::vector<PlaylistItem*> children;
                const auto childrenToMove = parent->children() | std::views::drop(row);
                std::ranges::copy(childrenToMove, std::back_inserter(children));

                const QModelIndex parentIndex = model->indexOfItem(parent);
                splitParents.emplace_back(parentIndex, newParent, row, finalRow, children);
            }
            row = parent->row() + (row > 0 ? 1 : 0);
        }

        std::ranges::reverse(splitParents);

        // Move tracks after drop index to new parents
        for(const SplitParent& parent : splitParents) {
            const QModelIndex prevParent = model->indexOfItem(prevParentItem);

            insertPlaylistRows(prevParent, newParentRow, newParentRow, {parent.target});

            const QModelIndex newParentIndex = model->indexOfItem(parent.target);
            movePlaylistRows(parent.source, parent.firstRow, parent.finalRow, newParentIndex, 0, parent.children);

            prevParentItem = parent.target;
            newParentRow   = 0;
        }

        for(const SplitParent& parent : splitParents) {
            headersToUpdate.append(parent.source);
            headersToUpdate.append(model->indexOfItem(parent.target));
        }
    }

    prevParentItem = currTarget;
    newParentRow   = row;

    // Create parents for dropped rows
    for(PlaylistItem* parent : sourceParents) {
        const QModelIndex prevParent = model->indexOfItem(prevParentItem);
        PlaylistItem* newParent      = cloneParent(parent);

        insertPlaylistRows(prevParent, newParentRow, newParentRow, {newParent});

        prevParentItem = newParent;
        newParentRow   = 0;
    }

    row = 0;
    return model->indexOfItem(prevParentItem);
}

void PlaylistModelPrivate::handleExternalDrop(const PendingData& data)
{
    QModelIndexList headersToCheck;

    auto* parentItem = itemForKey(data.parent);
    int row          = data.row;

    QModelIndex targetParent{model->indexOfItem(parentItem)};

    std::vector<std::pair<PlaylistItem*, std::vector<PlaylistItem*>>> itemData;

    std::ranges::transform(data.containerOrder, std::inserter(itemData, itemData.end()),
                           [this, &data](const QString& containerKey) {
                               PlaylistItem* item = itemForKey(containerKey);
                               std::vector<PlaylistItem*> children;
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

        const bool sameParents = sourceParentItem->baseKey() == targetParentItem->baseKey();

        if(!sameParents) {
            targetParent = handleDiffParentDrop(sourceParentItem, targetParentItem, row, headersToCheck);
        }

        const int total = row + static_cast<int>(children.size()) - 1;

        model->beginInsertRows(targetParent, row, total);
        row = insertRows(model, nodes, children, targetParent, row);
        model->endInsertRows();

        headersToCheck.emplace_back(targetParent);
    }

    model->rootItem()->resetChildren();

    removeEmptyHeaders(headersToCheck);
    mergeHeaders(headersToCheck);
    updateHeaders(headersToCheck);
    updateTrackIndexes();
}

void PlaylistModelPrivate::handleTrackGroup(const PendingData& data)
{
    QModelIndexList headersToCheck;

    updateTrackIndexes();

    auto cmpParentKeys = [data](const QString& key1, const QString& key2) {
        if(key1 == key2) {
            return false;
        }
        return std::ranges::find(data.containerOrder, key1) < std::ranges::find(data.containerOrder, key2);
    };
    using ParentItemMap = std::map<QString, std::vector<PlaylistItem*>, decltype(cmpParentKeys)>;
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

            const bool sameParents = sourceParentItem->baseKey() == targetParentItem->baseKey();

            if(!sameParents) {
                targetParent = handleDiffParentDrop(sourceParentItem, targetParentItem, row, headersToCheck);
            }

            const int total = row + static_cast<int>(children.size()) - 1;

            model->beginInsertRows(targetParent, row, total);
            insertRows(model, nodes, children, targetParent, row);
            model->endInsertRows();

            headersToCheck.emplace_back(targetParent);
            updateTrackIndexes();
        }
    }

    model->rootItem()->resetChildren();

    removeEmptyHeaders(headersToCheck);
    mergeHeaders(headersToCheck);
    updateHeaders(headersToCheck);
    updateTrackIndexes();
}

void PlaylistModelPrivate::storeMimeData(const QModelIndexList& indexes, QMimeData* mimeData)
{
    if(mimeData) {
        QModelIndexList sortedIndexes{indexes};
        std::ranges::sort(sortedIndexes, cmpTrackIndices);
        mimeData->setData(Constants::Mime::PlaylistItems, saveIndexes(sortedIndexes, currentPlaylist));
        mimeData->setData(Constants::Mime::TrackList, saveTracks(sortedIndexes));
    }
}

bool PlaylistModelPrivate::insertPlaylistRows(const QModelIndex& target, int firstRow, int lastRow,
                                              const std::vector<PlaylistItem*>& children)
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
                                            const QModelIndex& target, int row,
                                            const std::vector<PlaylistItem*>& children)
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

void PlaylistModelPrivate::removeEmptyHeaders(QModelIndexList& headers)
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

            if(leftSibling.isValid()) {
                headers.push_back(leftSibling);
            }
            if(rightSibling.isValid()) {
                headers.push_back(rightSibling);
            }

            QModelIndex currentParent = header.isValid() ? header.parent() : header;
            while(currentParent.isValid()) {
                headers.push_back(currentParent);
                currentParent = currentParent.parent();
            }
            headers.push_back(currentParent);

            const int headerRow = headerItem->row();
            removedHeaderKeys.emplace(headerItem->key());
            removePlaylistRows(headerRow, 1, headerParent);
            headers.removeAll(header);
        }
        if(headerParent.isValid()) {
            headersToCheck.push(headerParent);
        }
    }
    if(!removedHeaderKeys.empty()) {
        model->rootItem()->resetChildren();
    }
}

void PlaylistModelPrivate::mergeHeaders(QModelIndexList& headersToUpdate)
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

                headersToUpdate.removeAll(rightIndex);

                movePlaylistRows(rightIndex, 0, lastRow, leftIndex, targetRow, rightChildren);
                removePlaylistRows(rightSibling->row(), 1, rightIndex.parent());

                const int childCount = model->rowCount(leftIndex);
                for(int child{0}; child < childCount; ++child) {
                    const QModelIndex childIndex = model->index(child, 0, leftIndex);
                    headers.push(childIndex);
                }

                headers.push(leftIndex);

                if(!headersToUpdate.contains(leftIndex)) {
                    headersToUpdate.push_back(leftIndex);
                }
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
    const ParentChildRowMap indexGroups = determineRowGroups(indexes);

    model->tracksAboutToBeChanged();

    QModelIndexList headersToCheck;

    for(const auto& [parent, groups] : indexGroups) {
        for(const auto& children : groups | std::views::reverse) {
            removePlaylistRows(children.first, children.count(), parent);
        }
        headersToCheck.emplace_back(parent);
    }

    removeEmptyHeaders(headersToCheck);
    mergeHeaders(headersToCheck);
    updateHeaders(headersToCheck);
    updateTrackIndexes();

    model->tracksChanged();
}

void PlaylistModelPrivate::coverUpdated(const Core::Track& track)
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

ParentChildIndexMap PlaylistModelPrivate::determineIndexGroups(const QModelIndexList& indexes)
{
    ParentChildIndexMap indexGroups;

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

        std::vector<QModelIndex> group;
        for(auto it = startOfSequence; it != endOfSequence; ++it) {
            group.push_back(*it);
        }

        indexGroups.push_back(group);

        startOfSequence = endOfSequence;
    }

    return indexGroups;
}

ParentChildRowMap PlaylistModelPrivate::determineRowGroups(const QModelIndexList& indexes)
{
    ParentChildRowMap indexGroups;

    QModelIndexList sortedIndexes{indexes};
    optimiseSelection(model, sortedIndexes);

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

        IndexRange range;
        range.first = startOfSequence->row();
        range.last  = std::prev(endOfSequence)->row();

        const QModelIndex parent = startOfSequence->parent();
        indexGroups[parent].push_back(range);

        startOfSequence = endOfSequence;
    }

    return indexGroups;
}

ParentChildItemGroupMap PlaylistModelPrivate::determineItemGroups(PlaylistModel* model, const QModelIndexList& indexes)
{
    const ParentChildRowMap indexGroups = determineRowGroups(indexes);
    ParentChildItemGroupMap indexItemGroups;

    std::ranges::transform(
        indexGroups, std::inserter(indexItemGroups, indexItemGroups.begin()), [&model](const auto& groupPair) {
            const auto& [groupParent, groups] = groupPair;
            std::vector<std::vector<PlaylistItem*>> transformedVector;
            std::ranges::transform(
                groups, std::back_inserter(transformedVector), [&model, &groupParent](const auto& group) {
                    std::vector<PlaylistItem*> transformedInnerVector;
                    for(int row = group.first; row <= group.last; ++row) {
                        if(PlaylistItem* playlistItem
                           = static_cast<PlaylistItem*>(model->index(row, 0, groupParent).internalPointer())) {
                            transformedInnerVector.push_back(playlistItem);
                        }
                    }
                    return transformedInnerVector;
                });
            return std::make_pair(groupParent, transformedVector);
        });

    return indexItemGroups;
}

ParentChildItemMap PlaylistModelPrivate::groupChildren(PlaylistModel* model, const std::vector<PlaylistItem*>& children)
{
    ParentChildItemMap groupedChildren;
    for(PlaylistItem* child : children) {
        const QModelIndex parent = model->indexOfItem(child->parent());
        groupedChildren[parent].push_back(child);
    }
    return groupedChildren;
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
    PlaylistItem* parent = model->itemForIndex({});
    while(parent->childCount() > 0) {
        parent = parent->child(parent->childCount() - 1);
    }
    return {model->indexOfItem(parent), true};
}
} // namespace Fy::Gui::Widgets::Playlist
