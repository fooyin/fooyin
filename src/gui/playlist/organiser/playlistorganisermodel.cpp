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

#include "playlistorganisermodel.h"

#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <gui/guiconstants.h>
#include <utils/datastream.h>
#include <utils/utils.h>

#include <QApplication>
#include <QIODevice>
#include <QMimeData>
#include <QPalette>

#include <stack>

using namespace Qt::StringLiterals;

constexpr auto OrganiserItems = "application/x-fooyin-playlistorganiseritems";

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

using TrackIndexRangeList = std::vector<QModelIndexList>;

TrackIndexRangeList determineTrackIndexGroups(const QModelIndexList& indexes)
{
    TrackIndexRangeList indexGroups;

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

        indexGroups.emplace_back(startOfSequence, endOfSequence);

        startOfSequence = endOfSequence;
    }

    return indexGroups;
}

QString groupKey(const QString& title)
{
    static const QString prefix = u"Group."_s;
    return prefix + title;
}

QString playlistKey(const QString& name)
{
    static const QString prefix = u"Playlist."_s;
    return prefix + name;
}
} // namespace

namespace Fooyin {
PlaylistOrganiserModel::PlaylistOrganiserModel(PlaylistHandler* playlistHandler, PlayerController* playerController)
    : m_playlistHandler{playlistHandler}
    , m_playerController{playerController}
    , m_playingColour{QApplication::palette().highlight().color()}
{
    m_playingColour.setAlpha(90);

    auto playlistChanged = [this](const QString& key, Qt::ItemDataRole role) {
        if(m_nodes.contains(key)) {
            const QModelIndex index = indexOfItem(&m_nodes.at(key));
            emit dataChanged(index, index, {role});
        }
    };

    QObject::connect(m_playlistHandler, &PlaylistHandler::activePlaylistChanged, this,
                     [this, playlistChanged](Playlist* playlist) {
                         if(playlist) {
                             const QString prevKey = std::exchange(m_activePlaylistKey, playlistKey(playlist->name()));
                             playlistChanged(prevKey, Qt::BackgroundRole);
                             playlistChanged(m_activePlaylistKey, Qt::BackgroundRole);
                         }
                     });
    QObject::connect(m_playerController, &PlayerController::playStateChanged, this,
                     [this, playlistChanged]() { playlistChanged(m_activePlaylistKey, Qt::DecorationRole); });
}

void PlaylistOrganiserModel::populate()
{
    beginResetModel();
    resetRoot();
    m_nodes.clear();

    const PlaylistList playlists = m_playlistHandler->playlists();

    for(const auto& playlist : playlists) {
        auto* item = &m_nodes.emplace(playlistKey(playlist->name()), PlaylistOrganiserItem{playlist, rootItem()})
                          .first->second;
        rootItem()->appendChild(item);
    }

    endResetModel();
}

void PlaylistOrganiserModel::populateMissing()
{
    const PlaylistList playlists = m_playlistHandler->playlists();

    for(const auto& playlist : playlists) {
        const QString key = playlistKey(playlist->name());
        if(!m_nodes.contains(key)) {
            const int row = rowCount({});

            beginInsertRows({}, row, row);
            auto* item = &m_nodes.emplace(key, PlaylistOrganiserItem{playlist, rootItem()}).first->second;
            rootItem()->appendChild(item);
            endInsertRows();
        }
    }
}

QByteArray PlaylistOrganiserModel::saveModel()
{
    QByteArray data;
    QDataStream stream{&data, QIODeviceBase::WriteOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    std::stack<PlaylistOrganiserItem*> itemsToSave;
    itemsToSave.push(rootItem());

    while(!itemsToSave.empty()) {
        PlaylistOrganiserItem* item = itemsToSave.top();
        itemsToSave.pop();

        stream << static_cast<int>(item->type());

        if(item->type() == PlaylistOrganiserItem::PlaylistItem) {
            stream << item->playlist()->dbId();
        }
        else if(item->type() == PlaylistOrganiserItem::GroupItem) {
            stream << item->title();
        }

        if(item->type() != PlaylistOrganiserItem::PlaylistItem) {
            const auto children = item->children();
            stream << static_cast<qsizetype>(children.size());
            for(PlaylistOrganiserItem* child : children | std::views::reverse) {
                itemsToSave.push(child);
            }
        }
    }

    data = qCompress(data, 9);

    return data;
}

bool PlaylistOrganiserModel::restoreModel(QByteArray data)
{
    if(data.isEmpty()) {
        return false;
    }

    data = qUncompress(data);

    QDataStream stream{&data, QIODeviceBase::ReadOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    beginResetModel();
    resetRoot();
    m_nodes.clear();

    while(!stream.atEnd()) {
        recurseSaveModel(stream, rootItem());
    }

    endResetModel();

    return true;
}

QModelIndex PlaylistOrganiserModel::createGroup(const QModelIndex& parent)
{
    auto* parentItem    = itemForIndex(parent);
    const QString title = findUniqueName(u"New Group"_s);
    auto* item          = &m_nodes.emplace(groupKey(title), PlaylistOrganiserItem{title, parentItem}).first->second;

    const int row = parentItem->childCount();
    beginInsertRows(parent, row, row);
    parentItem->appendChild(item);
    endInsertRows();

    return index(row, 0, parent);
}

QModelIndex PlaylistOrganiserModel::createPlaylist(Playlist* playlist, const QModelIndex& parent)
{
    auto* parentItem = itemForIndex(parent);
    auto* item
        = &m_nodes.emplace(playlistKey(playlist->name()), PlaylistOrganiserItem{playlist, parentItem}).first->second;

    const int row = parentItem->childCount();
    beginInsertRows(parent, row, row);
    parentItem->appendChild(item);
    endInsertRows();

    return index(row, 0, parent);
}

void PlaylistOrganiserModel::playlistAdded(Playlist* playlist)
{
    beginInsertRows({}, rowCount({}), rowCount({}));
    auto* item
        = &m_nodes.emplace(playlistKey(playlist->name()), PlaylistOrganiserItem{playlist, rootItem()}).first->second;
    rootItem()->appendChild(item);
    endInsertRows();
}

void PlaylistOrganiserModel::playlistInserted(Playlist* playlist, const QString& group, int index)
{
    const QString key = groupKey(group);
    if(!group.isEmpty() && !m_nodes.contains(key)) {
        return;
    }

    auto* groupItem              = group.isEmpty() ? rootItem() : &m_nodes.at(key);
    const QModelIndex groupIndex = indexOfItem(groupItem);
    int row{index};
    if(row < 0 || row >= rowCount(groupIndex)) {
        row = rowCount(groupIndex);
    }

    beginInsertRows(groupIndex, row, row);
    auto* item
        = &m_nodes.emplace(playlistKey(playlist->name()), PlaylistOrganiserItem{playlist, groupItem}).first->second;
    groupItem->insertChild(row, item);
    endInsertRows();
}

void PlaylistOrganiserModel::playlistRenamed(Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    const auto playlistIt = std::ranges::find_if(std::as_const(m_nodes), [playlist](const auto& pair) {
        return pair.second.type() == PlaylistOrganiserItem::PlaylistItem
            && pair.second.playlist()->id() == playlist->id();
    });

    if(playlistIt != m_nodes.end()) {
        auto playlistItem  = m_nodes.extract(playlistIt);
        playlistItem.key() = playlistKey(playlist->name());

        auto* item = &m_nodes.insert(std::move(playlistItem)).position->second;
        item->setTitle(playlist->name());

        const QModelIndex index = indexForPlaylist(playlist);
        emit dataChanged(index, index, {Qt::DisplayRole});
    }
}

void PlaylistOrganiserModel::playlistRemoved(Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    const QString key = playlistKey(playlist->name());
    if(!m_nodes.contains(key)) {
        return;
    }

    const auto* item = &m_nodes.at(key);
    const int row    = item->row();
    auto* parent     = item->parent();

    beginRemoveRows(indexOfItem(parent), row, row);
    parent->removeChild(row);
    parent->resetChildren();
    endRemoveRows();

    m_nodes.erase(key);
}

void PlaylistOrganiserModel::sortAllPlaylists(const SortOrder order)
{
    for(auto& m_node : m_nodes) {
        // find root
        if(m_node.second.parent()->type() == PlaylistOrganiserItem::Type::Root) {
            emit layoutAboutToBeChanged();

            // sort children of root
            m_node.second.parent()->sortChildren(
                [order](const PlaylistOrganiserItem* first, const PlaylistOrganiserItem* second) {
                    if(order == Descending) {
                        return 0 < QString::localeAwareCompare(first->title(), second->title());
                    }

                    return 0 >= QString::localeAwareCompare(first->title(), second->title());
                });

            emit layoutChanged();

            // exit
            break;
        }
    }
}

void PlaylistOrganiserModel::sortGroupPlaylists(const QModelIndexList& indices, const SortOrder order)
{
    // abort unless we only have a single playlist selected
    if(indices.size() != 1) {
        return;
    }

    auto* sortGroup = itemForIndex(indices.first());

    if(sortGroup->type() != PlaylistOrganiserItem::Type::GroupItem) {
        return;
    }

    emit layoutAboutToBeChanged();

    // TODO: share this function with sortAllPlaylist somehow
    sortGroup->sortChildren([order](const PlaylistOrganiserItem* first, const PlaylistOrganiserItem* second) {
        if(order == Descending) {
            return 0 < QString::localeAwareCompare(first->title(), second->title());
        }
        return 0 >= QString::localeAwareCompare(first->title(), second->title());
    });

    emit layoutChanged();
}

QModelIndex PlaylistOrganiserModel::indexForPlaylist(Playlist* playlist)
{
    if(!playlist) {
        return {};
    }

    const QString key = playlistKey(playlist->name());
    if(m_nodes.contains(key)) {
        return indexOfItem(&m_nodes.at(key));
    }
    return {};
}

Qt::ItemFlags PlaylistOrganiserModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags flags = QAbstractItemModel::flags(index);

    if(index.isValid()) {
        flags |= Qt::ItemIsDragEnabled | Qt::ItemIsEditable;
    }
    if(!hasChildren(index)) {
        flags |= Qt::ItemNeverHasChildren;
    }

    flags |= Qt::ItemIsDropEnabled;

    return flags;
}

bool PlaylistOrganiserModel::hasChildren(const QModelIndex& parent) const
{
    return parent.data(PlaylistOrganiserItem::ItemType).toInt() != PlaylistOrganiserItem::PlaylistItem;
}

QVariant PlaylistOrganiserModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item      = itemForIndex(index);
    const auto type = item->type();

    const bool currentIsActive = type == PlaylistOrganiserItem::PlaylistItem && m_playlistHandler->activePlaylist()
                              && item->playlist()->id() == m_playlistHandler->activePlaylist()->id();

    switch(role) {
        case(Qt::DisplayRole):
            if(type == PlaylistOrganiserItem::PlaylistItem) {
                return item->title();
            }
            if(type == PlaylistOrganiserItem::GroupItem) {
                return u"%1 [%2]"_s.arg(item->title()).arg(item->childCount());
            }
            break;
        case(Qt::EditRole):
            return item->title();
        case(Qt::BackgroundRole): {
            if(currentIsActive) {
                return m_playingColour;
            }
            break;
        }
        case(Qt::DecorationRole):
            if(currentIsActive) {
                const auto state = m_playerController->playState();
                if(state == Player::PlayState::Playing) {
                    return Utils::pixmapFromTheme(Constants::Icons::Play);
                }
                if(state == Player::PlayState::Paused) {
                    return Utils::pixmapFromTheme(Constants::Icons::Pause);
                }
            }
            break;

        case(PlaylistOrganiserItem::ItemType):
            return QVariant::fromValue(item->type());
        case(PlaylistOrganiserItem::PlaylistData):
            return QVariant::fromValue(item->playlist());
        default:
            break;
    }

    return {};
}

bool PlaylistOrganiserModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(role != Qt::EditRole) {
        return false;
    }

    auto* item      = itemForIndex(index);
    const auto type = item->type();

    if(type == PlaylistOrganiserItem::PlaylistItem) {
        const QString name = value.toString();
        if(item->title() != name) {
            m_playlistHandler->renamePlaylist(item->playlist()->id(), name);
        }
    }
    else if(type == PlaylistOrganiserItem::GroupItem) {
        const QString oldTitle = item->title();
        const QString title    = value.toString();
        if(oldTitle != title) {
            const QString newTitle = findUniqueName(title);
            item->setTitle(newTitle);

            auto groupItem  = m_nodes.extract(groupKey(oldTitle));
            groupItem.key() = groupKey(newTitle);
            m_nodes.insert(std::move(groupItem));
        }
    }

    emit dataChanged(index, index, {Qt::DisplayRole});

    return true;
}

QStringList PlaylistOrganiserModel::mimeTypes() const
{
    return {QString::fromLatin1(OrganiserItems), QString::fromLatin1(Constants::Mime::TrackIds)};
}

bool PlaylistOrganiserModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                             const QModelIndex& parent) const
{
    if(action == Qt::MoveAction && data->hasFormat(QString::fromLatin1(OrganiserItems))) {
        return parent.data(PlaylistOrganiserItem::ItemType).toInt() != PlaylistOrganiserItem::PlaylistItem;
    }

    if(auto* playlist = parent.data(PlaylistOrganiserItem::PlaylistData).value<Playlist*>()) {
        if(playlist->isAutoPlaylist()) {
            return false;
        }
    }

    if(data->hasFormat(QString::fromLatin1(Constants::Mime::TrackIds)) || data->hasUrls()) {
        return true;
    }

    return QAbstractItemModel::canDropMimeData(data, action, row, column, parent);
}

Qt::DropActions PlaylistOrganiserModel::supportedDropActions() const
{
    return Qt::MoveAction | Qt::CopyAction;
}

Qt::DropActions PlaylistOrganiserModel::supportedDragActions() const
{
    return Qt::MoveAction;
}

QMimeData* PlaylistOrganiserModel::mimeData(const QModelIndexList& indexes) const
{
    auto* mimeData = new QMimeData();
    mimeData->setData(QString::fromLatin1(OrganiserItems), saveIndexes(indexes));
    return mimeData;
}

bool PlaylistOrganiserModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                          const QModelIndex& parent)
{
    if(!canDropMimeData(data, action, row, column, parent)) {
        return false;
    }

    if(data->hasUrls()) {
        if(auto* item = itemForIndex(parent)) {
            if(item->type() == PlaylistOrganiserItem::PlaylistItem) {
                emit filesDroppedOnPlaylist(data->urls(), item->playlist()->id());
            }
            else {
                emit filesDroppedOnGroup(data->urls(), item->title(), row);
            }
            return true;
        }
    }
    else if(data->hasFormat(QString::fromLatin1(Constants::Mime::TrackIds))) {
        auto trackData = data->data(QString::fromLatin1(Constants::Mime::TrackIds));
        std::vector<int> ids;
        QDataStream stream(&trackData, QIODevice::ReadOnly);
        stream >> ids;

        if(auto* item = itemForIndex(parent)) {
            if(item->type() == PlaylistOrganiserItem::PlaylistItem) {
                emit tracksDroppedOnPlaylist(ids, item->playlist()->id());
            }
            else {
                emit tracksDroppedOnGroup(ids, item->title(), row);
            }
            return true;
        }
    }

    return itemsDropped(data, row, parent);
}

void PlaylistOrganiserModel::removeItems(const QModelIndexList& indexes)
{
    if(indexes.empty()) {
        return;
    }

    const auto indexGroups = determineTrackIndexGroups(indexes);

    for(const auto& group : indexGroups | std::views::reverse) {
        const auto children
            = std::views::transform(group, [this](const QModelIndex& index) { return itemForIndex(index); });

        const QModelIndex parent = group.constFirst().parent();
        auto* parentItem         = children.front()->parent();

        const int firstRow = children.front()->row();
        int lastRow        = static_cast<int>(firstRow + children.size()) - 1;

        beginRemoveRows(parent, firstRow, lastRow);
        while(lastRow >= firstRow) {
            deleteNodes(itemForIndex(index(lastRow, 0, parent)));
            parentItem->removeChild(lastRow);
            --lastRow;
        }
        parentItem->resetChildren();
        endRemoveRows();
    }
}

QByteArray PlaylistOrganiserModel::saveIndexes(const QModelIndexList& indexes) const
{
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);

    for(const QModelIndex& index : indexes) {
        if(!index.isValid()) {
            continue;
        }
        auto* item = itemForIndex(index);
        const QString key
            = item->type() == PlaylistOrganiserItem::GroupItem ? groupKey(item->title()) : playlistKey(item->title());
        stream << key;
    }
    return result;
}

QModelIndexList PlaylistOrganiserModel::restoreIndexes(QByteArray data)
{
    QModelIndexList result;
    QDataStream stream(&data, QIODevice::ReadOnly);

    while(!stream.atEnd()) {
        QString key;
        stream >> key;
        if(m_nodes.contains(key)) {
            result.push_back(indexOfItem(&m_nodes.at(key)));
        }
    }
    return result;
}

void PlaylistOrganiserModel::recurseSaveModel(QDataStream& stream, PlaylistOrganiserItem* parent)
{
    int itemType;
    stream >> itemType;

    PlaylistOrganiserItem* currParent{parent};

    if(itemType == static_cast<int>(PlaylistOrganiserItem::PlaylistItem)) {
        int playlistId;
        stream >> playlistId;
        if(Playlist* playlist = m_playlistHandler->playlistByDbId(playlistId)) {
            auto* item = &m_nodes.emplace(playlistKey(playlist->name()), PlaylistOrganiserItem{playlist, currParent})
                              .first->second;
            currParent->appendChild(item);
        }
    }
    else if(itemType == static_cast<int>(PlaylistOrganiserItem::GroupItem)) {
        QString title;
        stream >> title;
        auto* item = &m_nodes.emplace(groupKey(title), PlaylistOrganiserItem{title, currParent}).first->second;
        currParent->appendChild(item);
        currParent = item;
    }

    if(itemType != static_cast<int>(PlaylistOrganiserItem::PlaylistItem)) {
        qsizetype childCount;
        stream >> childCount;
        for(qsizetype row{0}; row < childCount; ++row) {
            recurseSaveModel(stream, currParent);
        }
    }
}

QString PlaylistOrganiserModel::findUniqueName(const QString& name) const
{
    return Utils::findUniqueString(name, m_nodes, [](const auto& pair) { return pair.second.title(); });
}

void PlaylistOrganiserModel::deleteNodes(PlaylistOrganiserItem* node)
{
    if(!node) {
        return;
    }

    const int count = node->childCount();
    if(count > 0) {
        for(int row{0}; row < count; ++row) {
            if(PlaylistOrganiserItem* child = node->child(row)) {
                deleteNodes(child);
            }
        }
    }

    const QString title = node->title();

    if(node->type() == PlaylistOrganiserItem::GroupItem) {
        m_nodes.erase(groupKey(title));
    }
    else if(node->type() == PlaylistOrganiserItem::PlaylistItem) {
        const UId id = node->playlist()->id();
        m_nodes.erase(playlistKey(title));
        m_playlistHandler->removePlaylist(id);
    }
}

bool PlaylistOrganiserModel::itemsDropped(const QMimeData* data, int row, const QModelIndex& parent)
{
    if(!data->hasFormat(QString::fromLatin1(OrganiserItems))) {
        return false;
    }

    const QModelIndexList indexes = restoreIndexes(data->data(QString::fromLatin1(OrganiserItems)));
    if(indexes.empty()) {
        return false;
    }

    const auto indexGroups = determineTrackIndexGroups(indexes);

    if(row < 0) {
        row = rowCount(parent);
    }

    for(const auto& group : indexGroups) {
        const auto children = std::views::transform(std::as_const(group),
                                                    [this](const QModelIndex& index) { return itemForIndex(index); });

        auto* sourceParent       = children.front()->parent();
        const QModelIndex source = indexOfItem(sourceParent);
        auto* targetParent       = itemForIndex(parent);

        int currRow{row};
        const int firstRow = children.front()->row();
        const int lastRow  = static_cast<int>(firstRow + children.size()) - 1;

        if(source == parent && currRow >= firstRow && currRow <= lastRow + 1) {
            continue;
        }

        beginMoveRows(source, firstRow, lastRow, parent, row);

        for(PlaylistOrganiserItem* childItem : children) {
            childItem->resetRow();
            const int oldRow = childItem->row();
            if(oldRow < currRow) {
                targetParent->insertChild(currRow, childItem);
                sourceParent->removeChild(oldRow);

                if(source != parent) {
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

        endMoveRows();

        row = currRow;
    }

    rootItem()->resetChildren();

    return true;
}
} // namespace Fooyin

#include "moc_playlistorganisermodel.cpp"
