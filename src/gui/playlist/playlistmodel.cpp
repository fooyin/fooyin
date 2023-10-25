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

#include "playlistitem.h"
#include "playlistpopulator.h"
#include "playlistpreset.h"

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
#include <QStack>
#include <QThread>

#include <queue>
#include <set>

using namespace Qt::Literals::StringLiterals;

namespace {
QByteArray saveIndexes(const QModelIndexList& indexes)
{
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);

    for(const QModelIndex& index : indexes) {
        QModelIndex localIndex = index;
        QStack<int> indexParentStack;
        while(localIndex.isValid()) {
            indexParentStack << localIndex.row();
            localIndex = localIndex.parent();
        }

        stream << indexParentStack.size();
        while(!indexParentStack.isEmpty()) {
            stream << indexParentStack.pop();
        }
    }
    return result;
}

QModelIndexList restoreIndexes(QAbstractItemModel* model, QByteArray data)
{
    QModelIndexList result;
    QDataStream stream(&data, QIODevice::ReadOnly);

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

bool cmpTrackIndices(const QModelIndex& index1, const QModelIndex& index2)
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
} // namespace

struct cmpIndicies
{
    bool operator()(const QModelIndex& index1, const QModelIndex& index2) const
    {
        return cmpTrackIndices(index1, index2);
    }
};

using ParentChildMap = std::map<QModelIndex, std::vector<std::vector<int>>, cmpIndicies>;

ParentChildMap determineIndexGroups(const QModelIndexList& indexes)
{
    ParentChildMap indexGroups;

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

        std::vector<int> group;
        for(auto it = startOfSequence; it != endOfSequence; ++it) {
            group.push_back(it->row());
        }

        const QModelIndex parent = startOfSequence->parent();
        indexGroups[parent].push_back(group);

        startOfSequence = endOfSequence;
    }

    return indexGroups;
}

namespace Fy::Gui::Widgets::Playlist {
static constexpr auto MimeType = "application/x-playlistitem-internal-pointer";

using ParentChildItemMap = std::map<QModelIndex, std::vector<std::vector<PlaylistItem*>>, cmpIndicies>;

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
        Container& container = std::get<1>(header->data());
        container.clearTracks();

        const auto& children = header->children();
        for(PlaylistItem* child : children) {
            if(child->type() == PlaylistItem::Track) {
                const Core::Track& track = std::get<0>(child->data()).track();
                container.addTrack(track);
            }
            else {
                const Core::TrackList tracks = std::get<1>(child->data()).tracks();
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

class PlaylistModelPrivate
{
public:
    PlaylistModelPrivate(PlaylistModel* self, Core::Player::PlayerManager* playerManager,
                         Utils::SettingsManager* settings);

    void populateModel(PendingData& data);
    void updateModel(ItemKeyMap& data);
    void updateHeaders(const ItemPtrSet& headers);

    void beginReset();

    QVariant trackData(PlaylistItem* item, int role) const;
    QVariant headerData(PlaylistItem* item, int role) const;
    QVariant subheaderData(PlaylistItem* item, int role) const;

    ParentChildItemMap determineItemGroups(const QModelIndexList& indexes) const;
    std::map<QModelIndex, std::vector<PlaylistItem*>, cmpIndicies>
    groupChildren(const std::vector<PlaylistItem*>& children) const;

    template <typename Container>
    int moveRows(const QModelIndex& source, const Container& rows, const QModelIndex& target, int row);
    template <typename Container>
    int copyRows(const QModelIndex& source, const Container& rows, const QModelIndex& target, int row);

    void removeEmptyHeaders(const QModelIndexList& headers);
    void mergeHeaders(const QModelIndex& parentIndex, ItemPtrSet& headersToUpdate);

    PlaylistItem* cloneParent(PlaylistItem* parent);
    QModelIndex canBeMerged(PlaylistItem*& currTarget, int& targetRow, std::vector<PlaylistItem*>& sourceParents,
                            int targetOffset) const;

    QModelIndex handleDiffParentDrop(PlaylistItem* source, PlaylistItem* target, int& row, ItemPtrSet& headersToUpdate);

    void coverUpdated(const Core::Track& track);

    PlaylistModel* self;

    Core::Player::PlayerManager* playerManager;
    Utils::SettingsManager* settings;
    Library::CoverProvider* coverProvider;

    PlaylistPreset currentPreset;

    bool altColours;
    QSize coverSize;

    bool resetting{false};

    QThread populatorThread;
    PlaylistPopulator populator;

    QString headerText;

    NodeKeyMap pendingNodes;
    ItemKeyMap nodes;
    ItemKeyMap oldNodes;
    TrackIdNodeMap trackParents;

    QPixmap playingIcon;
    QPixmap pausedIcon;
};

PlaylistModelPrivate::PlaylistModelPrivate(PlaylistModel* self, Core::Player::PlayerManager* playerManager,
                                           Utils::SettingsManager* settings)
    : self{self}
    , playerManager{playerManager}
    , settings{settings}
    , coverProvider{new Library::CoverProvider(self)}
    , altColours{settings->value<Settings::PlaylistAltColours>()}
    , coverSize{settings->value<Settings::PlaylistThumbnailSize>(), settings->value<Settings::PlaylistThumbnailSize>()}
    , playingIcon{QIcon::fromTheme(Constants::Icons::Play).pixmap(20)}
    , pausedIcon{QIcon::fromTheme(Constants::Icons::Pause).pixmap(20)}
{
    populator.moveToThread(&populatorThread);
    populatorThread.start();
}

void PlaylistModelPrivate::populateModel(PendingData& data)
{
    nodes.merge(data.items);
    trackParents.merge(data.trackParents);

    if(resetting) {
        for(const auto& [parentKey, rows] : data.nodes) {
            auto* parent = parentKey == "0"_L1 ? self->itemForIndex({}) : &nodes.at(parentKey);

            for(const QString& row : rows) {
                PlaylistItem* child = &nodes.at(row);
                parent->appendChild(child);
            }
        }
    }
    else {
        for(auto& [parentKey, rows] : data.nodes) {
            std::ranges::copy(rows, std::back_inserter(pendingNodes[parentKey]));
        }
    }
}

void PlaylistModelPrivate::updateModel(ItemKeyMap& data)
{
    if(!resetting) {
        for(auto& [key, header] : data) {
            nodes[key]                    = header;
            const QModelIndex headerIndex = self->indexOfItem(&nodes[key]);
            emit self->dataChanged(headerIndex, headerIndex, {});
        }
    }
}

void PlaylistModelPrivate::updateHeaders(const ItemPtrSet& headers)
{
    ItemPtrSet items;

    for(PlaylistItem* header : headers) {
        PlaylistItem* currHeader{header};
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

    QMetaObject::invokeMethod(&populator, "updateHeaders", Q_ARG(ItemList&, updatedHeaders));
}

void PlaylistModelPrivate::beginReset()
{
    // Acts as a cache in case model hasn't been fully cleared
    oldNodes = std::move(nodes);
    nodes.clear();
    pendingNodes.clear();
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
            return playerManager->currentTrack() == track.track();
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
            switch(playerManager->playState()) {
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
            return QSize{0, currentPreset.header.rowHeight};
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
            return QSize{0, currentPreset.subHeader.rowHeight};
        }
        default:
            return {};
    }
}

ParentChildItemMap PlaylistModelPrivate::determineItemGroups(const QModelIndexList& indexes) const
{
    const ParentChildMap indexGroups = determineIndexGroups(indexes);
    ParentChildItemMap indexItemGroups;

    for(const auto& [groupParent, groups] : indexGroups) {
        std::vector<std::vector<PlaylistItem*>> transformedVector;
        for(const auto& group : groups) {
            std::vector<PlaylistItem*> transformedInnerVector;
            for(const int childRow : group) {
                if(PlaylistItem* playlistItem
                   = static_cast<PlaylistItem*>(self->index(childRow, 0, groupParent).internalPointer())) {
                    transformedInnerVector.push_back(playlistItem);
                }
            }
            transformedVector.push_back(transformedInnerVector);
        }
        indexItemGroups.emplace(groupParent, transformedVector);
    }
    return indexItemGroups;
}

std::map<QModelIndex, std::vector<PlaylistItem*>, cmpIndicies>
PlaylistModelPrivate::groupChildren(const std::vector<PlaylistItem*>& children) const
{
    std::map<QModelIndex, std::vector<PlaylistItem*>, cmpIndicies> groupedChildren;
    for(PlaylistItem* child : children) {
        const QModelIndex parent = self->indexOfItem(child->parent());
        groupedChildren[parent].push_back(child);
    }
    return groupedChildren;
}

void PlaylistModelPrivate::removeEmptyHeaders(const QModelIndexList& headers)
{
    std::queue<QModelIndex> headersToCheck;
    for(const QModelIndex& header : headers) {
        if(header.isValid()) {
            headersToCheck.push(header);
        }
    }

    while(!headersToCheck.empty()) {
        const QModelIndex header{headersToCheck.front()};
        headersToCheck.pop();

        PlaylistItem* headerItem             = self->itemForIndex(header);
        const QModelIndex headerParent       = header.parent();
        const PlaylistItem* headerParentItem = self->itemForIndex(headerParent);

        if(!headerParentItem) {
            continue;
        }

        if(headerItem && headerItem->childCount() < 1) {
            const int headerRow = headerItem->row();
            self->removePlaylistRows(headerRow, 1, headerParent);
            nodes.erase(headerItem->key());
        }
        if(headerParent.isValid()) {
            headersToCheck.push(headerParent);
        }
    }
}

void PlaylistModelPrivate::mergeHeaders(const QModelIndex& parentIndex, ItemPtrSet& headersToUpdate)
{
    PlaylistItem* parentItem = self->itemForIndex(parentIndex);
    if(!parentItem || parentItem->childCount() < 1) {
        return;
    }

    int row = 0;
    while(row < parentItem->childCount()) {
        const QModelIndex leftIndex  = self->indexOfItem(parentItem->child(row));
        const QModelIndex rightIndex = self->indexOfItem(parentItem->child(row + 1));

        PlaylistItem* leftSibling  = self->itemForIndex(leftIndex);
        PlaylistItem* rightSibling = self->itemForIndex(rightIndex);

        const bool bothHeaders
            = leftSibling->type() != PlaylistItem::Track && rightSibling->type() != PlaylistItem::Track;

        if(bothHeaders && leftIndex != rightIndex && leftSibling->baseKey() == rightSibling->baseKey()) {
            const auto rightChildren = rightSibling->children();
            const int targetRow      = leftSibling->childCount();
            const int lastRow        = rightSibling->childCount() - 1;

            self->movePlaylistRows(rightIndex, 0, lastRow, leftIndex, targetRow, rightChildren);

            self->removePlaylistRows(rightSibling->row(), 1, rightIndex.parent());

            headersToUpdate.erase(rightSibling);
            nodes.erase(rightSibling->key());
            headersToUpdate.emplace(leftSibling);
        }
        else {
            row++;
        }
    }

    for(const PlaylistItem* child : parentItem->children()) {
        const QModelIndex childIndex = self->indexOfItem(child);
        mergeHeaders(childIndex, headersToUpdate);
    }
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

QModelIndex PlaylistModelPrivate::canBeMerged(PlaylistItem*& currTarget, int& targetRow,
                                              std::vector<PlaylistItem*>& sourceParents, int targetOffset) const
{
    std::vector<PlaylistItem*> newSourceParents;

    PlaylistItem* checkItem = currTarget->child(targetRow + targetOffset);
    if(checkItem) {
        if(!sourceParents.empty() && sourceParents.front()->baseKey() == checkItem->baseKey()) {
            auto parent = sourceParents.cbegin();
            for(; parent != sourceParents.cend(); ++parent) {
                if((*parent)->baseKey() != checkItem->baseKey()) {
                    newSourceParents.push_back(*parent);
                    break;
                }
                currTarget     = checkItem;
                targetRow      = targetOffset >= 0 ? -1 : currTarget->childCount() - 1;
                auto* nextItem = checkItem->child(targetOffset > 0 ? checkItem->childCount() - 1 : 0);
                if(nextItem->type() != PlaylistItem::Track) {
                    checkItem = nextItem;
                }
            }
            if(parent == sourceParents.cend()) {
                return self->indexOfItem(checkItem);
            }
            sourceParents = newSourceParents;
        }
    }
    return {};
}

QModelIndex PlaylistModelPrivate::handleDiffParentDrop(PlaylistItem* source, PlaylistItem* target, int& row,
                                                       ItemPtrSet& headersToUpdate)
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

    // Check left
    if((targetIsRoot && row > 0) || (!targetIsRoot && row == 0)) {
        QModelIndex leftResult = canBeMerged(currTarget, targetRow, sourceParents, -1);
        if(leftResult.isValid()) {
            row = targetRow + 1;
            return leftResult;
        }
    }

    // Check right
    if((targetIsRoot && row < target->childCount() - 1) || (!targetIsRoot && row == target->childCount())) {
        QModelIndex rightResult = canBeMerged(currTarget, targetRow, sourceParents, targetIsRoot ? 0 : 1);
        if(rightResult.isValid()) {
            row = 0;
            return rightResult;
        }
    }

    int newParentRow = targetRow + 1;
    PlaylistItem* prevParentItem{currTarget};

    // Create parents for tracks after drop index (if any)
    std::vector<SplitParent> splitParents;

    for(PlaylistItem* parent : targetParents) {
        const int finalRow = parent->childCount() - 1;
        if(finalRow >= row || (parent->type() == PlaylistItem::Header && !splitParents.empty())) {
            PlaylistItem* newParent = cloneParent(parent);

            std::vector<PlaylistItem*> children;
            const auto childrenToMove = parent->children() | std::views::drop(row);
            std::ranges::copy(childrenToMove, std::back_inserter(children));

            const QModelIndex parentIndex = self->indexOfItem(parent);

            splitParents.push_back({.source   = parentIndex,
                                    .target   = newParent,
                                    .firstRow = row,
                                    .finalRow = finalRow,
                                    .children = children});
        }
        row = parent->row() + 1;
    }

    std::ranges::reverse(splitParents);

    // Move tracks after drop index to new parents
    for(const SplitParent& parent : splitParents) {
        const QModelIndex prevParent = self->indexOfItem(prevParentItem);

        self->insertPlaylistRows(prevParent, newParentRow, newParentRow, {parent.target});

        const QModelIndex newParentIndex = self->indexOfItem(parent.target);

        self->movePlaylistRows(parent.source, parent.firstRow, parent.finalRow, newParentIndex, 0, parent.children);

        prevParentItem = parent.target;
        newParentRow   = 0;
    }

    for(const SplitParent& parent : splitParents) {
        PlaylistItem* sourceItem = self->itemForIndex(parent.source);
        if(sourceItem->childCount() > 0) {
            headersToUpdate.emplace(sourceItem);
        }
        headersToUpdate.emplace(parent.target);
    }

    prevParentItem = currTarget;
    newParentRow   = targetRow + 1;

    if(currTarget == self->itemForIndex({}) && row == 0) {
        newParentRow = 0;
    }

    // Create parents for dropped rows
    for(PlaylistItem* parent : sourceParents) {
        const QModelIndex prevParent = self->indexOfItem(prevParentItem);
        PlaylistItem* newParent      = cloneParent(parent);

        self->insertPlaylistRows(prevParent, newParentRow, newParentRow, {newParent});

        prevParentItem = newParent;
        newParentRow   = 0;
    }

    row = 0;
    return self->indexOfItem(prevParentItem);
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
                const QModelIndex nodeIndex = self->indexOfItem(parentItem);
                emit self->dataChanged(nodeIndex, nodeIndex, {PlaylistItem::Role::Cover});
            }
        }
    }
}

template <typename Container>
int PlaylistModelPrivate::moveRows(const QModelIndex& source, const Container& rows, const QModelIndex& target, int row)
{
    int currRow{row};
    auto* targetParent = self->itemForIndex(target);
    if(!targetParent) {
        return currRow;
    }

    auto* sourceParent = self->itemForIndex(source);
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
int PlaylistModelPrivate::copyRows(const QModelIndex& source, const Container& rows, const QModelIndex& target, int row)
{
    int currRow{row};
    auto* targetParent = self->itemForIndex(target);
    if(!targetParent) {
        return currRow;
    }

    auto* sourceParent = self->itemForIndex(source);
    for(PlaylistItem* childItem : rows) {
        childItem->resetRow();
        auto* newChild = &nodes.emplace(Utils::generateRandomHash(), *childItem).first->second;
        targetParent->insertChild(currRow, newChild);
        ++currRow;
    }
    sourceParent->resetChildren();
    targetParent->resetChildren();

    return currRow;
}

PlaylistModel::PlaylistModel(Core::Player::PlayerManager* playerManager, Utils::SettingsManager* settings,
                             QObject* parent)
    : TreeModel{parent}
    , p{std::make_unique<PlaylistModelPrivate>(this, playerManager, settings)}
{
    p->settings->subscribe<Settings::PlaylistAltColours>(this, [this](bool enabled) {
        p->altColours = enabled;
        emit dataChanged({}, {}, {Qt::BackgroundRole});
    });
    p->settings->subscribe<Settings::PlaylistThumbnailSize>(this, [this](int size) {
        p->coverSize = {size, size};
        p->coverProvider->clearCache();
        emit dataChanged({}, {}, {PlaylistItem::Role::Cover});
    });

    QObject::connect(&p->populator, &PlaylistPopulator::populated, this, [this](PendingData data) {
        if(p->resetting) {
            beginResetModel();
            resetRoot();
            p->beginReset();
        }

        p->populateModel(data);

        if(p->resetting) {
            endResetModel();
        }
        p->resetting = false;
    });

    QObject::connect(&p->populator, &PlaylistPopulator::headersUpdated, this,
                     [this](ItemKeyMap data) { p->updateModel(data); });

    QObject::connect(p->coverProvider, &Library::CoverProvider::coverAdded, this,
                     [this](const Core::Track& track) { p->coverUpdated(track); });
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
    }
    defaultFlags |= Qt::ItemIsDropEnabled;
    return defaultFlags;
}

QVariant PlaylistModel::headerData(int /*section*/, Qt::Orientation orientation, int role) const
{
    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
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

    switch(type) {
        case(PlaylistItem::Header):
            return p->headerData(item, role);
        case(PlaylistItem::Track):
            return p->trackData(item, role);
        case(PlaylistItem::Subheader):
            return p->subheaderData(item, role);
        case(PlaylistItem::Root):
            return {};
    }
    return {};
}

bool PlaylistModel::hasChildren(const QModelIndex& parent) const
{
    if(!parent.isValid()) {
        return true;
    }
    const auto* item = static_cast<PlaylistItem*>(parent.internalPointer());
    return (item->type() != PlaylistItem::ItemType::Track);
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

QStringList PlaylistModel::mimeTypes() const
{
    return {MimeType};
}

bool PlaylistModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                    const QModelIndex& parent) const
{
    if((action == Qt::MoveAction || action == Qt::CopyAction) && data->hasFormat(MimeType)) {
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
    if(mimeData) {
        mimeData->setData(MimeType, saveIndexes(indexes));
    }
    return mimeData;
}

bool PlaylistModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                 const QModelIndex& parent)
{
    if(!canDropMimeData(data, action, row, column, parent)) {
        return false;
    }

    const QModelIndexList indexes = restoreIndexes(this, data->data(MimeType));
    if(indexes.isEmpty()) {
        return false;
    }

    const ParentChildItemMap indexItemGroups = p->determineItemGroups(indexes);

    QModelIndexList headersToCheck;
    ItemPtrSet headersToUpdate;
    QModelIndex targetParent{parent};

    for(const auto& [sourceParent, groups] : indexItemGroups) {
        PlaylistItem* sourceParentItem = itemForIndex(sourceParent);
        PlaylistItem* targetParentItem = itemForIndex(targetParent);

        const bool sameParents = sourceParentItem->baseKey() == targetParentItem->baseKey();

        if(!sameParents) {
            targetParent     = p->handleDiffParentDrop(sourceParentItem, targetParentItem, row, headersToUpdate);
            targetParentItem = itemForIndex(targetParent);
        }

        for(const auto& groupChildren : groups) {
            // If dropped within a group and a previous groups parents were different,
            // the children will be split and have different parents.
            // Regroup to prevent issues with beginMoveRows/beginInsertRows.
            const auto regroupedChildren = p->groupChildren(groupChildren);
            for(const auto& [groupParent, children] : regroupedChildren) {
                if(action == Qt::MoveAction) {
                    const int firstRow = children.front()->row();
                    const int lastRow  = children.back()->row();

                    const bool identicalParents = sourceParentItem->key() == targetParentItem->key();

                    if(identicalParents && row >= firstRow && row <= lastRow + 1) {
                        row = lastRow + 1;
                        continue;
                    }

                    beginMoveRows(groupParent, firstRow, lastRow, targetParent, row);
                    row = p->moveRows(groupParent, children, targetParent, row);
                    endMoveRows();
                }
                else {
                    const int total = row + static_cast<int>(children.size()) - 1;

                    beginInsertRows(targetParent, row, total);
                    row = p->copyRows(groupParent, children, targetParent, row);
                    endInsertRows();
                }
                headersToCheck.emplace_back(groupParent);

                PlaylistItem* groupItem = itemForIndex(groupParent);
                if(groupItem->childCount() > 0) {
                    headersToUpdate.emplace(groupItem);
                }
            }
        }
        headersToUpdate.emplace(targetParentItem);

        rootItem()->resetChildren();

        p->removeEmptyHeaders(headersToCheck);
        p->mergeHeaders({}, headersToUpdate);

        rootItem()->resetChildren();
    }

    p->updateHeaders(headersToUpdate);

    return true;
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
    }
    endInsertRows();

    rows.erase(rows.begin(), rows.begin() + rowCount);
}

bool PlaylistModel::canFetchMore(const QModelIndex& parent) const
{
    auto* item = itemForIndex(parent);
    return p->pendingNodes.contains(item->key()) && !p->pendingNodes[item->key()].empty();
}

bool PlaylistModel::insertPlaylistRows(const QModelIndex& target, int firstRow, int lastRow,
                                       const std::vector<PlaylistItem*>& children)
{
    const int diff = firstRow == lastRow ? 1 : lastRow - firstRow + 1;
    if(static_cast<int>(children.size()) != diff) {
        return false;
    }

    auto* parent = itemForIndex(target);

    beginInsertRows(target, firstRow, lastRow);
    for(auto* child : children) {
        parent->insertChild(firstRow, child);
        firstRow += 1;
    }
    endInsertRows();

    return true;
}

bool PlaylistModel::movePlaylistRows(const QModelIndex& source, int firstRow, int lastRow, const QModelIndex& target,
                                     int row, const std::vector<PlaylistItem*>& children)
{
    const int diff = firstRow == lastRow ? 1 : lastRow - firstRow + 1;
    if(static_cast<int>(children.size()) != diff) {
        return false;
    }

    beginMoveRows(source, firstRow, lastRow, target, row);
    p->moveRows(source, children, target, row);
    endMoveRows();

    return true;
}

bool PlaylistModel::removePlaylistRows(int row, int count, const QModelIndex& parent)
{
    auto* parentItem = itemForIndex(parent);
    if(!parentItem) {
        return false;
    }

    int lastRow = row + count - 1;
    beginRemoveRows(parent, row, lastRow);
    while(lastRow >= row) {
        parentItem->removeChild(lastRow);
        --lastRow;
    }
    parentItem->resetChildren();
    endRemoveRows();

    return true;
}

void PlaylistModel::removeTracks(const QModelIndexList& indexes)
{
    const ParentChildMap indexGroups = determineIndexGroups(indexes);

    QModelIndexList headersToCheck;

    for(const auto& [parent, groups] : indexGroups) {
        for(const auto& children : groups | std::views::reverse) {
            removePlaylistRows(children.front(), static_cast<int>(children.size()), parent);
        }
        headersToCheck.emplace_back(parent);
    }
    p->removeEmptyHeaders(headersToCheck);
}

void PlaylistModel::reset(const Core::Playlist::Playlist& playlist)
{
    if(!playlist.isValid()) {
        return;
    }

    p->populator.stopThread();

    p->resetting = true;
    updateHeader(playlist);

    QMetaObject::invokeMethod(&p->populator,
                              [this, playlist] { p->populator.run(p->currentPreset, playlist.tracks()); });
}

void PlaylistModel::updateHeader(const Core::Playlist::Playlist& playlist)
{
    if(playlist.isValid()) {
        p->headerText = playlist.name() + ": " + QString::number(playlist.trackCount()) + " Tracks";
    }
}

void PlaylistModel::changeTrackState()
{
    emit dataChanged({}, {}, {PlaylistItem::Role::Playing});
}

void PlaylistModel::changePreset(const PlaylistPreset& preset)
{
    p->currentPreset = preset;
}
} // namespace Fy::Gui::Widgets::Playlist
