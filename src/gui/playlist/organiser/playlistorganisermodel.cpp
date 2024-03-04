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

#include <core/playlist/playlisthandler.h>
#include <gui/guiconstants.h>

#include <QIODevice>
#include <QMimeData>

#include <stack>

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
    static const QString prefix = QStringLiteral("Group.");
    return prefix + title;
}

QString playlistKey(const QString& name)
{
    static const QString prefix = QStringLiteral("Playlist.");
    return prefix + name;
}
} // namespace

namespace Fooyin {
struct PlaylistOrganiserModel::Private
{
    PlaylistOrganiserModel* self;

    PlaylistHandler* playlistHandler;

    std::unordered_map<QString, PlaylistOrganiserItem> nodes;

    explicit Private(PlaylistOrganiserModel* self_, PlaylistHandler* playlistHandler_)
        : self{self_}
        , playlistHandler{playlistHandler_}
    { }

    QByteArray saveIndexes(const QModelIndexList& indexes) const
    {
        QByteArray result;
        QDataStream stream(&result, QIODevice::WriteOnly);

        for(const QModelIndex& index : indexes) {
            if(!index.isValid()) {
                continue;
            }
            auto* item        = self->itemForIndex(index);
            const QString key = item->type() == PlaylistOrganiserItem::GroupItem ? groupKey(item->title())
                                                                                 : playlistKey(item->title());
            stream << key;
        }
        return result;
    }

    QModelIndexList restoreIndexes(QByteArray data)
    {
        QModelIndexList result;
        QDataStream stream(&data, QIODevice::ReadOnly);

        while(!stream.atEnd()) {
            QString key;
            stream >> key;
            if(nodes.contains(key)) {
                result.push_back(self->indexOfItem(&nodes.at(key)));
            }
        }
        return result;
    }

    void recurseSaveModel(QDataStream& stream, PlaylistOrganiserItem* parent)
    {
        int itemType;
        stream >> itemType;

        PlaylistOrganiserItem* currParent{parent};

        if(itemType == static_cast<int>(PlaylistOrganiserItem::PlaylistItem)) {
            int playlistId;
            stream >> playlistId;
            if(Playlist* playlist = playlistHandler->playlistById(playlistId)) {
                auto* item = &nodes.emplace(playlistKey(playlist->name()), PlaylistOrganiserItem{playlist, currParent})
                                  .first->second;
                currParent->appendChild(item);
            }
        }
        else if(itemType == static_cast<int>(PlaylistOrganiserItem::GroupItem)) {
            QString title;
            stream >> title;
            auto* item = &nodes.emplace(groupKey(title), PlaylistOrganiserItem{title, currParent}).first->second;
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

    QString findUniqueName(const QString& name) const
    {
        return Utils::findUniqueString(name, nodes, [](const auto& pair) { return pair.second.title(); });
    }

    void deleteNodes(PlaylistOrganiserItem* node)
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

        if(node->type() == PlaylistOrganiserItem::GroupItem) {
            nodes.erase(groupKey(node->title()));
        }
        else if(node->type() == PlaylistOrganiserItem::PlaylistItem) {
            nodes.erase(playlistKey(node->title()));
            playlistHandler->removePlaylist(node->playlist()->id());
        }
    }
};

PlaylistOrganiserModel::PlaylistOrganiserModel(PlaylistHandler* playlistHandler)
    : p{std::make_unique<Private>(this, playlistHandler)}
{ }

PlaylistOrganiserModel::~PlaylistOrganiserModel() = default;

void PlaylistOrganiserModel::populate()
{
    beginResetModel();
    resetRoot();
    p->nodes.clear();

    const PlaylistList playlists = p->playlistHandler->playlists();

    for(const auto& playlist : playlists) {
        auto* item = &p->nodes.emplace(playlistKey(playlist->name()), PlaylistOrganiserItem{playlist, rootItem()})
                          .first->second;
        rootItem()->appendChild(item);
    }

    endResetModel();
}

void PlaylistOrganiserModel::populateMissing()
{
    const PlaylistList playlists = p->playlistHandler->playlists();

    for(const auto& playlist : playlists) {
        const QString key = playlistKey(playlist->name());
        if(!p->nodes.contains(key)) {
            const int row = rowCount({});

            beginInsertRows({}, row, row);
            auto* item = &p->nodes.emplace(key, PlaylistOrganiserItem{playlist, rootItem()}).first->second;
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
            stream << item->playlist()->id();
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
    p->nodes.clear();

    while(!stream.atEnd()) {
        p->recurseSaveModel(stream, rootItem());
    }

    endResetModel();

    return true;
}

QModelIndex PlaylistOrganiserModel::createGroup(const QModelIndex& parent)
{
    auto* parentItem    = itemForIndex(parent);
    const QString title = p->findUniqueName(QStringLiteral("New Group"));
    auto* item          = &p->nodes.emplace(groupKey(title), PlaylistOrganiserItem{title, parentItem}).first->second;

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
        = &p->nodes.emplace(playlistKey(playlist->name()), PlaylistOrganiserItem{playlist, parentItem}).first->second;

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
        = &p->nodes.emplace(playlistKey(playlist->name()), PlaylistOrganiserItem{playlist, rootItem()}).first->second;
    rootItem()->appendChild(item);
    endInsertRows();
}

void PlaylistOrganiserModel::playlistRenamed(Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    const auto playlistIt = std::ranges::find_if(std::as_const(p->nodes), [playlist](const auto& pair) {
        return pair.second.type() == PlaylistOrganiserItem::PlaylistItem
            && pair.second.playlist()->id() == playlist->id();
    });

    if(playlistIt != p->nodes.end()) {
        auto playlistItem  = p->nodes.extract(playlistIt);
        playlistItem.key() = playlistKey(playlist->name());

        auto* item = &p->nodes.insert(std::move(playlistItem)).position->second;
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
    if(!p->nodes.contains(key)) {
        return;
    }

    const auto* item = &p->nodes.at(key);
    const int row    = item->row();
    auto* parent     = item->parent();

    beginRemoveRows(indexOfItem(parent), row, row);
    parent->removeChild(row);
    parent->resetChildren();
    endRemoveRows();

    p->nodes.erase(key);
}

QModelIndex PlaylistOrganiserModel::indexForPlaylist(Playlist* playlist)
{
    if(!playlist) {
        return {};
    }

    const QString key = playlistKey(playlist->name());
    if(p->nodes.contains(key)) {
        return indexOfItem(&p->nodes.at(key));
    }
    return {};
}

Qt::ItemFlags PlaylistOrganiserModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(index);

    const auto type = index.data(PlaylistOrganiserItem::ItemType).toInt();

    if(index.isValid()) {
        defaultFlags |= Qt::ItemIsDragEnabled | Qt::ItemIsEditable;
    }
    if(type == PlaylistOrganiserItem::PlaylistItem) {
        defaultFlags |= Qt::ItemNeverHasChildren;
    }
    else {
        defaultFlags |= Qt::ItemIsDropEnabled;
    }

    return defaultFlags;
}

bool PlaylistOrganiserModel::hasChildren(const QModelIndex& parent) const
{
    return parent.data(PlaylistOrganiserItem::ItemType).toInt() != PlaylistOrganiserItem::PlaylistItem;
}

QVariant PlaylistOrganiserModel::data(const QModelIndex& index, int role) const
{
    if(role != Qt::DisplayRole && role != Qt::EditRole && role != PlaylistOrganiserItem::ItemType
       && role != PlaylistOrganiserItem::PlaylistData) {
        return {};
    }

    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item      = itemForIndex(index);
    const auto type = item->type();

    if(role == PlaylistOrganiserItem::ItemType) {
        return QVariant::fromValue(item->type());
    }

    if(role == PlaylistOrganiserItem::PlaylistData) {
        return QVariant::fromValue(item->playlist());
    }

    if(type == PlaylistOrganiserItem::PlaylistItem) {
        return item->title();
    }
    if(type == PlaylistOrganiserItem::GroupItem) {
        if(role == Qt::DisplayRole) {
            return QString{QStringLiteral("%1 [%2]")}.arg(item->title()).arg(item->childCount());
        }
        return item->title();
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
            p->playlistHandler->renamePlaylist(item->playlist()->id(), name);
        }
    }
    else if(type == PlaylistOrganiserItem::GroupItem) {
        const QString oldTitle = item->title();
        const QString title    = value.toString();
        if(oldTitle != title) {
            const QString newTitle = p->findUniqueName(title);
            item->setTitle(newTitle);

            auto groupItem  = p->nodes.extract(groupKey(oldTitle));
            groupItem.key() = groupKey(newTitle);
            p->nodes.insert(std::move(groupItem));
        }
    }

    emit dataChanged(index, index, {Qt::DisplayRole});

    return true;
}

QStringList PlaylistOrganiserModel::mimeTypes() const
{
    return {QString::fromLatin1(OrganiserItems)};
}

bool PlaylistOrganiserModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                             const QModelIndex& parent) const
{
    if(action == Qt::MoveAction && data->hasFormat(QString::fromLatin1(OrganiserItems))) {
        return true;
    }
    return QAbstractItemModel::canDropMimeData(data, action, row, column, parent);
}

Qt::DropActions PlaylistOrganiserModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

Qt::DropActions PlaylistOrganiserModel::supportedDragActions() const
{
    return Qt::MoveAction;
}

QMimeData* PlaylistOrganiserModel::mimeData(const QModelIndexList& indexes) const
{
    auto* mimeData = new QMimeData();
    mimeData->setData(QString::fromLatin1(OrganiserItems), p->saveIndexes(indexes));
    return mimeData;
}

bool PlaylistOrganiserModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                          const QModelIndex& parent)
{
    if(!canDropMimeData(data, action, row, column, parent)) {
        return false;
    }

    const QModelIndexList indexes = p->restoreIndexes(data->data(QString::fromLatin1(OrganiserItems)));

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

void PlaylistOrganiserModel::removeItems(const QModelIndexList& indexes)
{
    if(indexes.empty()) {
        return;
    }

    const auto indexGroups = determineTrackIndexGroups(indexes);

    for(const auto& group : indexGroups) {
        const auto children = std::views::transform(std::as_const(group),
                                                    [this](const QModelIndex& index) { return itemForIndex(index); });

        const QModelIndex parent = group.constFirst().parent();
        auto* parentItem         = children.front()->parent();

        const int firstRow = children.front()->row();
        int lastRow        = static_cast<int>(firstRow + children.size()) - 1;

        beginRemoveRows(parent, firstRow, lastRow);
        while(lastRow >= firstRow) {
            parentItem->removeChild(lastRow);
            --lastRow;
        }
        parentItem->resetChildren();
        endRemoveRows();

        for(auto* child : children) {
            p->deleteNodes(child);
        }
    }
}
} // namespace Fooyin

#include "moc_playlistorganisermodel.cpp"
