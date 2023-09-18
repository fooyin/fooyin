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

#include "gui/guiconstants.h"
#include "gui/guisettings.h"
#include "playlistitem.h"
#include "playlistpopulator.h"
#include "playlistpreset.h"

#include <core/library/coverprovider.h>
#include <core/player/playermanager.h>
#include <core/playlist/playlisthandler.h>

#include <utils/crypto.h>
#include <utils/settings/settingsmanager.h>

#include <QIODevice>
#include <QIcon>
#include <QMimeData>
#include <QPalette>
#include <QStack>
#include <QStringBuilder>
#include <QThread>

#include <queue>

namespace {
bool compareIndices(const QModelIndex& index1, const QModelIndex& index2)
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
    return item1.row() < item2.row();
}

struct cmpParents
{
    bool operator()(const QModelIndex& index1, const QModelIndex& index2) const
    {
        return compareIndices(index1, index2);
    }
};
} // namespace

namespace Fy::Gui::Widgets::Playlist {
static constexpr auto MimeType = "application/x-playlistitem-internal-pointer";

using ParentChildMap     = std::map<QModelIndex, std::vector<std::vector<int>>, cmpParents>;
using ParentChildItemMap = std::map<QModelIndex, std::vector<std::vector<PlaylistItem*>>, cmpParents>;

struct SplitParent
{
    QModelIndex source;
    PlaylistItem* target;
    int firstRow{0};
    int finalRow{0};
    std::vector<PlaylistItem*> children;
};

struct PlaylistModel::Private : public QObject
{
    PlaylistModel* model;
    Core::Player::PlayerManager* playerManager;
    Utils::SettingsManager* settings;
    Core::Library::CoverProvider coverProvider;

    PlaylistPreset currentPreset;
    Core::Playlist::Playlist currentPlaylist;

    bool altColours;

    bool resetting{false};

    QThread populatorThread;
    PlaylistPopulator populator;

    NodeMap pendingNodes;
    ItemMap nodes;
    ItemMap oldNodes;

    QPixmap playingIcon;
    QPixmap pausedIcon;

    Private(PlaylistModel* model, Core::Player::PlayerManager* playerManager, Utils::SettingsManager* settings)
        : model{model}
        , playerManager{playerManager}
        , settings{settings}
        , altColours{settings->value<Settings::PlaylistAltColours>()}
        , playingIcon{QIcon::fromTheme(Constants::Icons::Play).pixmap(20)}
        , pausedIcon{QIcon::fromTheme(Constants::Icons::Pause).pixmap(20)}
    {
        populator.moveToThread(&populatorThread);
        populatorThread.start();
    }

    void populateModel(PendingData& data)
    {
        nodes.merge(data.items);

        if(resetting) {
            for(const auto& [parentKey, rows] : data.nodes) {
                auto* parent = parentKey == "0" ? model->rootItem() : &nodes.at(parentKey);

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

    void beginReset()
    {
        model->resetRoot();
        // Acts as a cache in case model hasn't been fully cleared
        oldNodes = std::move(nodes);
        nodes.clear();
        pendingNodes.clear();
    }

    QVariant trackData(PlaylistItem* item, int role) const
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

    QVariant headerData(PlaylistItem* item, int role) const
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
                if(!currentPreset.header.showCover) {
                    return {};
                }
                return coverProvider.albumThumbnail(header.coverPath());
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

    QVariant subheaderData(PlaylistItem* item, int role) const
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

    QModelIndexList restoreIndexes(QByteArray data)
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

    ParentChildMap determineIndexGroups(const QModelIndexList& indexes)
    {
        ParentChildMap indexGroups;

        QModelIndexList sortedIndexes{indexes};
        std::sort(sortedIndexes.begin(), sortedIndexes.end(), compareIndices);

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

    ParentChildItemMap determineItemGroups(const QModelIndexList& indexes)
    {
        const ParentChildMap indexGroups = determineIndexGroups(indexes);
        ParentChildItemMap indexItemGroups;

        for(const auto& [groupParent, groups] : indexGroups) {
            std::vector<std::vector<PlaylistItem*>> transformedVector;
            for(const auto& group : groups) {
                std::vector<PlaylistItem*> transformedInnerVector;
                for(const int childRow : group) {
                    if(PlaylistItem* playlistItem
                       = static_cast<PlaylistItem*>(model->index(childRow, 0, groupParent).internalPointer())) {
                        transformedInnerVector.push_back(playlistItem);
                    }
                }
                transformedVector.push_back(transformedInnerVector);
            }
            indexItemGroups.emplace(groupParent, transformedVector);
        }
        return indexItemGroups;
    }

    auto groupChildren(const std::vector<PlaylistItem*>& children)
    {
        std::map<QModelIndex, std::vector<PlaylistItem*>, cmpParents> groupedChildren;
        for(PlaylistItem* child : children) {
            const QModelIndex parent = model->indexOfItem(child->parent());
            groupedChildren[parent].push_back(child);
        }
        return groupedChildren;
    };

    template <typename Container>
    int moveRows(const QModelIndex& source, const Container& rows, const QModelIndex& target, int row)
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
    int copyRows(const QModelIndex& source, const Container& rows, const QModelIndex& target, int row)
    {
        int currRow{row};
        auto* targetParent = model->itemForIndex(target);
        if(!targetParent) {
            return currRow;
        }

        auto* sourceParent = model->itemForIndex(source);
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

    void removeEmptyHeaders(const QModelIndexList& headers)
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

            PlaylistItem* headerItem       = model->itemForIndex(header);
            const QModelIndex headerParent = header.parent();
            PlaylistItem* headerParentItem = model->itemForIndex(headerParent);

            if(!headerParentItem) {
                continue;
            }

            if(headerItem && headerItem->childCount() < 1) {
                const int headerRow = headerItem->row();
                model->removePlaylistRows(headerRow, 1, headerParent);
            }
            if(headerParent.isValid()) {
                headersToCheck.push(headerParent);
            }
        }
    }

    void mergeHeaders(const QModelIndex& parentIndex)
    {
        PlaylistItem* parentItem = model->itemForIndex(parentIndex);
        if(!parentItem || parentItem->childCount() < 1) {
            return;
        }

        int row = 0;
        while(row < parentItem->childCount()) {
            const QModelIndex leftIndex  = model->indexOfItem(parentItem->child(row));
            const QModelIndex rightIndex = model->indexOfItem(parentItem->child(row + 1));

            PlaylistItem* leftSibling  = model->itemForIndex(leftIndex);
            PlaylistItem* rightSibling = model->itemForIndex(rightIndex);

            const bool bothHeaders
                = leftSibling->type() != PlaylistItem::Track && rightSibling->type() != PlaylistItem::Track;

            if(bothHeaders && leftIndex != rightIndex && leftSibling->baseKey() == rightSibling->baseKey()) {
                const auto rightChildren = rightSibling->children();
                const int firstRow       = leftSibling->childCount();
                const int finalRow       = rightSibling->childCount() - 1;

                model->beginMoveRows(rightIndex, 0, finalRow, leftIndex, firstRow);
                moveRows(rightIndex, rightChildren, leftIndex, firstRow);
                model->endMoveRows();

                model->removePlaylistRows(rightSibling->row(), 1, rightIndex.parent());
            }
            else {
                row++;
            }
        }

        for(const PlaylistItem* child : parentItem->children()) {
            const QModelIndex childIndex = model->indexOfItem(child);
            mergeHeaders(childIndex);
        }
    }

    PlaylistItem* cloneParent(PlaylistItem* parent)
    {
        const QString parentKey = Utils::generateRandomHash();
        auto* newParent         = &nodes.emplace(parentKey, *parent).first->second;
        newParent->setKey(parentKey);
        newParent->resetRow();
        newParent->clearChildren();

        return newParent;
    }

    QModelIndex handleDiffParentDrop(PlaylistItem* source, PlaylistItem* target, int& row)
    {
        int targetRow{0};
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

        int newParentRow = targetRow + 1;
        PlaylistItem* prevParentItem{currTarget};

        // Create parents for tracks after drop index (if any)
        std::vector<SplitParent> splitParents;

        for(PlaylistItem* parent : targetParents) {
            const int finalRow = parent->childCount() - 1;
            if(finalRow > row || (parent->type() == PlaylistItem::Header && !splitParents.empty())) {
                PlaylistItem* newParent = cloneParent(parent);

                std::vector<PlaylistItem*> children;
                const auto childrenToMove = parent->children() | std::views::drop(row);
                std::ranges::copy(childrenToMove, std::back_inserter(children));

                const QModelIndex parentIndex = model->indexOfItem(parent);

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
            const QModelIndex prevParent = model->indexOfItem(prevParentItem);

            model->beginInsertRows(prevParent, newParentRow, newParentRow);
            prevParentItem->insertChild(newParentRow, parent.target);
            model->endInsertRows();

            const QModelIndex newParentIndex = model->indexOfItem(parent.target);

            model->beginMoveRows(parent.source, parent.firstRow, parent.finalRow, newParentIndex, 0);
            moveRows(parent.source, parent.children, newParentIndex, 0);
            model->endMoveRows();

            prevParentItem = parent.target;
            newParentRow   = 0;
        }

        prevParentItem = currTarget;
        newParentRow   = targetRow + 1;

        if(currTarget == model->rootItem() && row == 0) {
            newParentRow = 0;
        }

        // Create parents for dropped rows
        for(PlaylistItem* parent : sourceParents) {
            const QModelIndex prevParent = model->indexOfItem(prevParentItem);
            PlaylistItem* newParent      = cloneParent(parent);

            model->beginInsertRows(prevParent, newParentRow, newParentRow);
            prevParentItem->insertChild(newParentRow, newParent);
            model->endInsertRows();

            prevParentItem = newParent;
            newParentRow   = 0;
        }

        row = 0;
        return model->indexOfItem(prevParentItem);
    }
};

PlaylistModel::PlaylistModel(Core::Player::PlayerManager* playerManager, Utils::SettingsManager* settings,
                             QObject* parent)
    : TreeModel{parent}
    , p{std::make_unique<Private>(this, playerManager, settings)}
{
    p->settings->subscribe<Settings::PlaylistAltColours>(this, [this](bool enabled) {
        p->altColours = enabled;
        emit dataChanged({}, {}, {Qt::BackgroundRole});
    });

    QObject::connect(&p->populator, &PlaylistPopulator::populated, this, [this](PendingData data) {
        if(p->resetting) {
            beginResetModel();
            p->beginReset();
        }

        p->populateModel(data);

        if(p->resetting) {
            endResetModel();
        }
        p->resetting = false;
    });
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

QVariant PlaylistModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)
    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    if(!p->currentPlaylist.isValid()) {
        return {};
    }
    return QString("%1: %2 Tracks").arg(p->currentPlaylist.name()).arg(p->currentPlaylist.trackCount());
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
        mimeData->setData(MimeType, p->saveIndexes(indexes));
    }
    return mimeData;
}

bool PlaylistModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                 const QModelIndex& parent)
{
    if(!canDropMimeData(data, action, row, column, parent)) {
        return false;
    }

    const QModelIndexList indexes = p->restoreIndexes(data->data(MimeType));
    if(indexes.isEmpty()) {
        return false;
    }

    const ParentChildItemMap indexItemGroups = p->determineItemGroups(indexes);

    QModelIndexList headersToCheck;
    QModelIndex targetParent{parent};

    for(const auto& [sourceParent, groups] : indexItemGroups) {
        PlaylistItem* sourceParentItem = itemForIndex(sourceParent);
        PlaylistItem* targetParentItem = itemForIndex(targetParent);

        const bool sameParents = sourceParentItem->baseKey() == targetParentItem->baseKey();

        if(!sameParents) {
            targetParent = p->handleDiffParentDrop(sourceParentItem, targetParentItem, row);
        }

        for(const auto& groupChildren : groups) {
            // If dropped within a group and a previous groups parents were different,
            // the children will be split and have different parents.
            // Regroup to prevent issues with beginMoveRows/beginInsertRows.
            const auto regroupedChildren = p->groupChildren(groupChildren);
            for(const auto& [parent, children] : regroupedChildren) {
                if(action == Qt::MoveAction) {
                    const int firstRow = children.front()->row();
                    const int lastRow  = children.back()->row();

                    const bool identicalParents = sourceParentItem->key() == targetParentItem->key();

                    if(identicalParents && row >= firstRow && row <= lastRow + 1) {
                        row = lastRow + 1;
                        continue;
                    }

                    beginMoveRows(parent, firstRow, lastRow, targetParent, row);
                    row = p->moveRows(parent, children, targetParent, row);
                    endMoveRows();
                }
                else {
                    const int total = row + static_cast<int>(children.size()) - 1;

                    beginInsertRows(targetParent, row, total);
                    row = p->copyRows(parent, children, targetParent, row);
                    endInsertRows();
                }
                headersToCheck.emplace_back(parent);
            }
        }

        rootItem()->resetChildren();
        p->removeEmptyHeaders(headersToCheck);
        p->mergeHeaders({});
        rootItem()->resetChildren();
    }

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
    const ParentChildMap indexGroups = p->determineIndexGroups(indexes);

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

    p->resetting       = true;
    p->currentPlaylist = playlist;

    QMetaObject::invokeMethod(&p->populator, [this, playlist] {
        p->populator.run(p->currentPreset, playlist.tracks());
    });
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
