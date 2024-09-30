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

#include "playlistmodel.h"

#include "guiutils.h"
#include "internalguisettings.h"
#include "playlistinteractor.h"
#include "playlistitem.h"
#include "playlistpopulator.h"
#include "playlistpreset.h"
#include "playlistscriptregistry.h"

#include <core/coresettings.h>
#include <core/library/musiclibrary.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlist.h>
#include <core/track.h>
#include <gui/coverprovider.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/widgets/autoheaderview.h>
#include <utils/crypto.h>
#include <utils/datastream.h>
#include <utils/modelutils.h>
#include <utils/settings/settingsmanager.h>
#include <utils/signalthrottler.h>
#include <utils/starrating.h>
#include <utils/utils.h>

#include <QApplication>
#include <QIODevice>
#include <QMimeData>

#include <queue>
#include <span>
#include <stack>

constexpr auto MimeModelId       = "application/x-playlistmodel-id";
constexpr auto MaxPlaylistTracks = 250;

namespace {
bool cmpItemsPlaylistItems(Fooyin::PlaylistItem* pItem1, Fooyin::PlaylistItem* pItem2, bool reverse = false)
{
    Fooyin::PlaylistItem* item1{pItem1};
    Fooyin::PlaylistItem* item2{pItem2};

    while(item1->parent() && item2->parent() && item1->parent() != item2->parent()) {
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

using ItemPtrSet = std::set<Fooyin::PlaylistItem*, cmpItemsReverse>;

int determineDropIndex(const QAbstractItemModel* model, const QModelIndex& parent, int row)
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
    const auto parentKey{Fooyin::UId::create()};
    auto* newParent = &nodes.emplace(parentKey, *parent).first->second;
    newParent->setKey(parentKey);
    newParent->resetRow();
    newParent->clearChildren();

    return newParent;
}

QModelIndexList optimiseSelection(QAbstractItemModel* model, const QModelIndexList& selection)
{
    std::queue<QModelIndex> stack;

    for(const QModelIndex index : selection) {
        stack.push(index);
    }

    QModelIndexList optimisedSelection;
    QModelIndexList selectedParents;

    while(!stack.empty()) {
        const QModelIndex current = stack.front();
        stack.pop();
        const QModelIndex parent = current.parent();

        if(selection.contains(parent) || selectedParents.contains(parent)) {
            continue;
        }

        bool allChildrenSelected{true};

        if(parent.isValid()) {
            const int rowCount = model->rowCount(parent);
            for(int row{0}; row < rowCount; ++row) {
                const QModelIndex child = model->index(row, 0, parent);
                if(!selection.contains(child) && !selectedParents.contains(child)) {
                    allChildrenSelected = false;
                    break;
                }
            }
        }

        if(!allChildrenSelected || !parent.isValid()) {
            optimisedSelection.append(current);
        }
        else if(!optimisedSelection.contains(parent)) {
            selectedParents.append(parent);
            stack.push(parent);
        }
    }

    return optimisedSelection;
}

ItemPtrSet optimiseSelection(const ItemPtrSet& selection)
{
    using Fooyin::PlaylistItem;

    std::queue<PlaylistItem*> stack;

    for(PlaylistItem* index : selection) {
        stack.push(index);
    }

    ItemPtrSet optimisedSelection;
    ItemPtrSet selectedParents;

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

Fooyin::TrackIndexRangeList determineTrackIndexGroups(const QModelIndexList& indexes, const QModelIndex& target = {},
                                                      int row = -1)
{
    using Fooyin::PlaylistItem;

    Fooyin::TrackIndexRangeList indexGroups;

    QModelIndexList sortedIndexes{indexes};
    std::ranges::sort(sortedIndexes, Fooyin::Utils::sortModelIndexes);

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

using IndexGroupsList = std::vector<QModelIndexList>;

IndexGroupsList determineIndexGroups(const QModelIndexList& indexes)
{
    IndexGroupsList indexGroups;

    QModelIndexList sortedIndexes{indexes};
    std::ranges::sort(sortedIndexes, Fooyin::Utils::sortModelIndexes);

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

struct SplitParent
{
    Fooyin::PlaylistItem* source;
    Fooyin::PlaylistItem* target;
    int firstRow{0};
    int finalRow{0};
    std::vector<Fooyin::PlaylistItem*> children;
};

QByteArray saveTracks(const QModelIndexList& indexes)
{
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);

    Fooyin::TrackIds trackIds;
    trackIds.reserve(indexes.size());

    std::ranges::transform(indexes, std::back_inserter(trackIds), [](const QModelIndex& index) {
        return index.data(Fooyin::PlaylistItem::Role::ItemData).value<Fooyin::Track>().id();
    });

    Fooyin::operator<<(stream, trackIds);

    return result;
}

Fooyin::QueueTracks savePlaylistTracks(const Fooyin::UId& playlistId, const QModelIndexList& indexes)
{
    Fooyin::QueueTracks tracks;

    for(const QModelIndex& index : indexes) {
        const auto track           = index.data(Fooyin::PlaylistItem::Role::ItemData).value<Fooyin::Track>();
        const auto indexInPlaylist = index.data(Fooyin::PlaylistItem::Role::Index).toInt();
        tracks.emplace_back(track, playlistId, indexInPlaylist);

        if(std::cmp_greater_equal(tracks.size(), MaxPlaylistTracks)) {
            break;
        }
    }

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

    Fooyin::UId playlistId;
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

    Fooyin::UId playlistId;
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

QStyleOptionViewItem::Position getIconPosition(const QString& text)
{
    const auto iconPos = text.indexOf(QLatin1String(Fooyin::PlayingIcon));

    if(iconPos <= 0) {
        return QStyleOptionViewItem::Left;
    }

    return QStyleOptionViewItem::Right;
}
} // namespace

namespace Fooyin {
PlaylistModel::PlaylistModel(PlaylistInteractor* playlistInteractor, CoverProvider* coverProvider,
                             SettingsManager* settings, QObject* parent)
    : TreeModel{parent}
    , m_library{playlistInteractor->library()}
    , m_settings{settings}
    , m_coverProvider{coverProvider}
    , m_id{UId::create()}
    , m_resetting{false}
    , m_playingColour{QApplication::palette().highlight().color()}
    , m_disabledColour{Qt::red}
    , m_populator{playlistInteractor->playerController()}
    , m_playlistLoaded{false}
    , m_pixmapPadding{settings->value<Settings::Gui::Internal::PlaylistImagePadding>()}
    , m_pixmapPaddingTop{settings->value<Settings::Gui::Internal::PlaylistImagePaddingTop>()}
    , m_starRatingSize{settings->value<Settings::Gui::StarRatingSize>()}
    , m_currentPlaylist{nullptr}
    , m_currentPlayState{Player::PlayState::Stopped}
{
    m_playingColour.setAlpha(90);
    m_disabledColour.setAlpha(50);

    m_populator.setUseVarious(m_settings->value<Settings::Core::UseVariousForCompilations>());
    m_populator.moveToThread(&m_populatorThread);
    m_populatorThread.start();

    m_settings->subscribe<Settings::Gui::Internal::PlaylistImagePadding>(this, [this](int padding) {
        m_pixmapPadding = padding;
        emit dataChanged({}, {}, {PlaylistItem::ImagePadding});
    });
    m_settings->subscribe<Settings::Gui::Internal::PlaylistImagePaddingTop>(this, [this](int padding) {
        m_pixmapPaddingTop = padding;
        emit dataChanged({}, {}, {PlaylistItem::ImagePaddingTop});
    });
    m_settings->subscribe<Settings::Gui::StarRatingSize>(this, [this](int size) {
        m_starRatingSize = size;
        emit dataChanged({}, {}, {Qt::DisplayRole});
    });

    auto updateColours = [this]() {
        m_playingColour = QApplication::palette().highlight().color();
        m_playingColour.setAlpha(90);
        emit dataChanged({}, {}, {Qt::BackgroundRole});
    };
    m_settings->subscribe<Settings::Gui::Theme>(this, updateColours);
    m_settings->subscribe<Settings::Gui::Style>(this, updateColours);

    m_settings->subscribe<Settings::Gui::IconTheme>(this, [this]() { emit dataChanged({}, {}, {Qt::DecorationRole}); });

    QObject::connect(&m_populator, &PlaylistPopulator::finished, this, [this]() {
        m_playlistLoaded = true;
        emit dataChanged({}, {});
        emit playlistLoaded();
    });

    QObject::connect(&m_populator, &PlaylistPopulator::populated, this,
                     [this](PendingData data) { populateModel(data); });

    QObject::connect(&m_populator, &PlaylistPopulator::populatedTrackGroup, this,
                     [this](PendingData data) { populateTrackGroup(data); });

    QObject::connect(&m_populator, &PlaylistPopulator::headersUpdated, this,
                     [this](ItemKeyMap data) { updateModel(data); });

    QObject::connect(&m_populator, &PlaylistPopulator::tracksUpdated, this,
                     [this](const ItemList& data) { updateTracks(data); });

    QObject::connect(m_coverProvider, &CoverProvider::coverAdded, this,
                     [this](const Track& track) { coverUpdated(track); });
}

PlaylistModel::~PlaylistModel()
{
    m_populator.stopThread();
    m_populatorThread.quit();
    m_populatorThread.wait();
}

Qt::ItemFlags PlaylistModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(index);
    const auto type            = index.data(PlaylistItem::Type).toInt();

    if(type == PlaylistItem::Track) {
        defaultFlags |= Qt::ItemIsDragEnabled;
        defaultFlags |= Qt::ItemNeverHasChildren;
    }
    else {
        defaultFlags &= ~Qt::ItemIsSelectable;
    }
    defaultFlags |= Qt::ItemIsDropEnabled;

    return defaultFlags;
}

QVariant PlaylistModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(orientation == Qt::Orientation::Vertical) {
        return {};
    }

    if(role == AutoHeaderView::SectionAlignment) {
        return columnAlignment(section).toInt();
    }

    if(role == Qt::DisplayRole) {
        if(!m_columns.empty()) {
            if(section >= 0 && section < static_cast<int>(m_columns.size())) {
                return m_columns.at(section).name;
            }
        }

        return m_headerText;
    }

    return {};
}

bool PlaylistModel::setHeaderData(int section, Qt::Orientation /*orientation*/, const QVariant& value, int role)
{
    if(role != AutoHeaderView::SectionAlignment) {
        return false;
    }

    if(section < 0 || section >= columnCount({})) {
        return {};
    }

    changeColumnAlignment(section, value.value<Qt::Alignment>());

    emit dataChanged({}, {}, {Qt::TextAlignmentRole});

    return true;
}

QVariant PlaylistModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item = itemForIndex(index);

    const PlaylistItem::ItemType type = item->type();

    if(role == Qt::TextAlignmentRole) {
        return QVariant{Qt::AlignVCenter | columnAlignment(index.column())};
    }

    if(role == PlaylistItem::Type) {
        return type;
    }

    if(role == PlaylistItem::Index) {
        return item->index();
    }

    if(role == PlaylistItem::BaseKey) {
        return QVariant::fromValue(item->baseKey());
    }

    if(role == PlaylistItem::SingleColumnMode) {
        return m_columns.empty();
    }

    switch(type) {
        case(PlaylistItem::Header):
            return headerData(item, index.column(), role);
        case(PlaylistItem::Track):
            return trackData(item, index, role);
        case(PlaylistItem::Subheader):
            return subheaderData(item, index.column(), role);
        default:
            break;
    }

    return {};
}

void PlaylistModel::fetchMore(const QModelIndex& parent)
{
    auto* parentItem = itemForIndex(parent);
    if(!m_pendingNodes.contains(parentItem->key())) {
        return;
    }

    auto& rows = m_pendingNodes.at(parentItem->key());

    const int row      = parentItem->childCount();
    const int rowCount = static_cast<int>(rows.size());

    const auto rowsToInsert = std::views::take(rows, rowCount);

    beginInsertRows(parent, row, row + rowCount - 1);
    for(const auto& pendingRow : rowsToInsert) {
        PlaylistItem& child = m_nodes.at(pendingRow);
        fetchChildren(parentItem, &child);
    }
    endInsertRows();

    rows.erase(rows.begin(), rows.begin() + rowCount);

    updateTrackIndexes();
}

bool PlaylistModel::canFetchMore(const QModelIndex& parent) const
{
    auto* item = itemForIndex(parent);
    return m_pendingNodes.contains(item->key()) && !m_pendingNodes.at(item->key()).empty();
}

bool PlaylistModel::hasChildren(const QModelIndex& parent) const
{
    if(!parent.isValid()) {
        return true;
    }

    if(parent.column() != 0) {
        return false;
    }

    return parent.data(PlaylistItem::Type).toInt() != PlaylistItem::ItemType::Track;
}

QHash<int, QByteArray> PlaylistModel::roleNames() const
{
    auto roles = QAbstractItemModel::roleNames();

    roles.insert(+PlaylistItem::Role::Id, "ID");
    roles.insert(+PlaylistItem::Role::Subtitle, "Subtitle");
    roles.insert(+PlaylistItem::Role::Path, "Path");
    roles.insert(+PlaylistItem::Role::ItemData, "ItemData");

    return roles;
}

int PlaylistModel::columnCount(const QModelIndex& /*parent*/) const
{
    if(!m_columns.empty()) {
        return static_cast<int>(m_columns.size());
    }
    return 1;
}

QStringList PlaylistModel::mimeTypes() const
{
    return {QString::fromLatin1(Constants::Mime::PlaylistItems), QString::fromLatin1(Constants::Mime::TrackIds),
            QString::fromLatin1(Constants::Mime::QueueTracks)};
}

bool PlaylistModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                    const QModelIndex& parent) const
{
    if((action == Qt::MoveAction || action == Qt::CopyAction)
       && (data->hasUrls() || data->hasFormat(QString::fromLatin1(Constants::Mime::PlaylistItems))
           || data->hasFormat(QString::fromLatin1(Constants::Mime::TrackIds)))) {
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
    storeMimeData(indexes, mimeData);
    return mimeData;
}

bool PlaylistModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                 const QModelIndex& parent)
{
    if(!canDropMimeData(data, action, row, column, parent)) {
        return false;
    }

    return prepareDrop(data, action, row, column, parent);
}

bool PlaylistModel::playlistIsLoaded() const
{
    return m_playlistLoaded;
}

bool PlaylistModel::haveTracks() const
{
    return (m_currentPlaylist && m_currentPlaylist->trackCount() > 0) || !m_trackParents.empty();
}

MoveOperation PlaylistModel::moveTracks(const MoveOperation& operation)
{
    MoveOperation reverseOperation;
    MoveOperationMap pendingGroups;

    const MoveOperationMap moveOpGroups = determineMoveOperationGroups(operation);

    tracksAboutToBeChanged();

    for(const auto& [index, moveGroups] : moveOpGroups | std::views::reverse) {
        const auto [targetItem, end] = itemForTrackIndex(index);

        const int targetRow            = end ? rowCount({}) : targetItem->row();
        PlaylistItem* targetParentItem = end ? rootItem() : targetItem->parent();
        QModelIndex targetParent       = indexOfItem(targetParentItem);
        int targetIndex{0};

        if(end) {
            targetIndex = targetItem->index() + 1;
        }
        else if(targetRow >= targetParentItem->childCount()) {
            targetIndex = targetParentItem->childCount();
        }
        else {
            targetIndex = targetParentItem->child(targetRow)->index();
        }

        int row = targetRow;

        for(const auto& children : moveGroups) {
            if(children.empty()) {
                continue;
            }

            PlaylistItem* sourceParentItem = children.front()->parent();
            if(!sourceParentItem) {
                continue;
            }

            const auto targetResult = findDropTarget(sourceParentItem, targetParentItem, row);

            targetParent     = targetResult.dropTarget();
            targetParentItem = itemForIndex(targetParent);

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

            const QModelIndex sourceParent = indexOfItem(sourceParentItem);

            beginMoveRows(sourceParent, firstRow, lastRow, targetParent, row);
            row = dropMoveRows(sourceParent, children, targetParent, row);
            endMoveRows();

            updateTrackIndexes();
            rootItem()->resetChildren();

            pendingGroups.emplace_back(reverseIndex, MoveOperationItemGroups{children});
        }
    }

    for(const auto& [index, groups] : pendingGroups) {
        for(const auto& children : groups) {
            QModelIndexList childIndexes;
            std::ranges::transform(children, std::back_inserter(childIndexes),
                                   [this](const PlaylistItem* child) { return indexOfItem(child); });
            const TrackIndexRangeList indexRanges = determineTrackIndexGroups(childIndexes);

            reverseOperation.emplace_back(index, indexRanges);
        }
    }

    cleanupHeaders();
    updateTrackIndexes();

    tracksChanged();

    return reverseOperation;
}

Qt::Alignment PlaylistModel::columnAlignment(int column) const
{
    if(column < 0 || std::cmp_greater_equal(column, m_columnAlignments.size())) {
        return Qt::AlignLeft;
    }

    return m_columnAlignments.at(column);
}

void PlaylistModel::changeColumnAlignment(int column, Qt::Alignment alignment)
{
    if(std::cmp_greater_equal(column, m_columnAlignments.size())) {
        m_columnAlignments.resize(column + 1, Qt::AlignLeft);
    }

    m_columnAlignments[column] = alignment;
}

void PlaylistModel::resetColumnAlignment(int column)
{
    if(column < 0 || std::cmp_greater_equal(column, m_columnAlignments.size())) {
        return;
    }

    m_columnAlignments.erase(m_columnAlignments.begin() + column);
}

void PlaylistModel::resetColumnAlignments()
{
    m_columnAlignments.clear();
}

void PlaylistModel::setFont(const QFont& font)
{
    QMetaObject::invokeMethod(&m_populator, [this, font]() { m_populator.setFont(font); });
}

void PlaylistModel::setPixmapColumnSize(int column, int size)
{
    if(std::cmp_greater_equal(column, m_columnSizes.size())) {
        m_columnSizes.resize(column + 1);
    }
    m_columnSizes[column] = size;
}

void PlaylistModel::setPixmapColumnSizes(const std::vector<int>& sizes)
{
    m_columnSizes = sizes;
}

void PlaylistModel::reset(const TrackList& tracks)
{
    m_populator.stopThread();

    m_playlistLoaded = false;
    m_resetting      = true;

    QMetaObject::invokeMethod(&m_populator, [this, tracks] {
        m_populator.setUseVarious(m_settings->value<Settings::Core::UseVariousForCompilations>());
        m_populator.run(m_currentPlaylist ? m_currentPlaylist->id() : UId{}, m_currentPreset, m_columns, tracks);
    });
}

void PlaylistModel::reset(const PlaylistPreset& preset, const PlaylistColumnList& columns, Playlist* playlist,
                          const TrackList& tracks)
{
    if(preset.isValid()) {
        m_currentPreset = preset;
    }

    m_columns       = columns;
    m_pixmapColumns = pixmapColumns();

    m_currentPlaylist = playlist;
    updateHeader(playlist);

    reset(tracks);
}

void PlaylistModel::reset(const PlaylistPreset& preset, const PlaylistColumnList& columns, Playlist* playlist)
{
    reset(preset, columns, playlist, playlist ? playlist->tracks() : TrackList{});
}

PlaylistTrack PlaylistModel::playingTrack() const
{
    return m_playingTrack;
}

void PlaylistModel::stopAfterTrack(const QModelIndex& index)
{
    if(std::exchange(m_stopAtIndex, index) == index) {
        m_stopAtIndex = QPersistentModelIndex{};
    }

    emit dataChanged(index, index, {Qt::DecorationRole});

    if(index.siblingAtColumn(0) == m_playingIndex) {
        m_settings->set<Settings::Core::StopAfterCurrent>(m_stopAtIndex.isValid());
    }
}

TrackIndexResult PlaylistModel::trackIndexAtPlaylistIndex(int index, bool fetch)
{
    if(m_trackIndexes.empty()) {
        return {};
    }

    if(index >= 0) {
        if(fetch) {
            while(!m_trackIndexes.contains(index) && canFetchMore({})) {
                fetchMore({});
            }
        }

        if(m_trackIndexes.contains(index)) {
            const auto key = m_trackIndexes.at(index);
            if(m_nodes.contains(key)) {
                auto& item = m_nodes.at(key);
                return {indexOfItem(&item), false};
            }
        }
    }

    // End of playlist - return last track index
    const auto lastIndex = static_cast<int>(m_trackIndexes.size()) - 1;
    const auto key       = m_trackIndexes.at(lastIndex);
    if(m_nodes.contains(key)) {
        auto& item = m_nodes.at(key);
        return {indexOfItem(&item), true};
    }

    return {};
}

QModelIndex PlaylistModel::indexAtPlaylistIndex(int index, bool fetch, bool includeEnd)
{
    const auto result = trackIndexAtPlaylistIndex(index, fetch);
    return !includeEnd && result.endOfPlaylist ? QModelIndex{} : result.index;
}

QModelIndexList PlaylistModel::indexesOfTrackId(int id)
{
    if(!m_trackParents.contains(id)) {
        return {};
    }

    QModelIndexList indexes;

    const auto parents = m_trackParents.at(id);

    for(const auto& parentKey : parents) {
        if(m_nodes.contains(parentKey)) {
            auto* parentItem = &m_nodes.at(parentKey);

            if(parentItem->type() == PlaylistItem::Track) {
                QModelIndex nodeIndex = indexOfItem(parentItem);
                while(!nodeIndex.isValid() && canFetchMore({})) {
                    fetchMore({});
                    nodeIndex = indexOfItem(parentItem);
                }
                if(nodeIndex.isValid()) {
                    indexes.emplace_back(nodeIndex);
                }
            }
        }
    }

    return indexes;
}

void PlaylistModel::insertTracks(const TrackGroups& tracks)
{
    if(m_currentPlaylist) {
        QMetaObject::invokeMethod(&m_populator, [this, tracks] {
            m_populator.runTracks(m_currentPlaylist->id(), m_currentPreset, m_columns, tracks);
        });
    }
}

void PlaylistModel::updateTracks(const std::vector<int>& indexes)
{
    TrackGroups groups;

    auto startOfSequence = indexes.cbegin();
    while(startOfSequence != indexes.cend()) {
        auto endOfSequence = std::adjacent_find(startOfSequence, indexes.cend(),
                                                [](const int lhs, const int rhs) { return rhs != lhs + 1; });
        if(endOfSequence != indexes.cend()) {
            std::advance(endOfSequence, 1);
        }

        const int first = *startOfSequence;

        for(auto it = startOfSequence; it != endOfSequence; ++it) {
            const auto& [index, end] = trackIndexAtPlaylistIndex(*it, true);
            if(!end) {
                m_indexesPendingRemoval.push_back(index);
                if(const auto track = m_currentPlaylist->track(*it)) {
                    groups[first].push_back(track.value());
                }
            }
        }

        startOfSequence = endOfSequence;
    }

    insertTracks(groups);
}

void PlaylistModel::refreshTracks(const std::vector<int>& indexes)
{
    TrackItemMap items;

    for(const int index : indexes) {
        const auto& [modelIndex, end] = trackIndexAtPlaylistIndex(index, true);
        if(!end) {
            if(const auto track = m_currentPlaylist->track(index)) {
                items.emplace(track.value(), *itemForIndex(modelIndex));
            }
        }
    }

    if(m_currentPlaylist) {
        QMetaObject::invokeMethod(&m_populator, [this, items] {
            m_populator.setUseVarious(m_settings->value<Settings::Core::UseVariousForCompilations>());
            m_populator.updateTracks(m_currentPlaylist->id(), m_currentPreset, m_columns, items);
        });
    }
}

void PlaylistModel::removeTracks(const QModelIndexList& indexes)
{
    tracksAboutToBeChanged();

    const auto indexesToRemove = optimiseSelection(this, indexes);

    const ParentChildRangesList indexGroups = determineRowGroups(indexesToRemove);

    for(const auto& [parent, groups] : indexGroups) {
        for(const auto& children : groups | std::views::reverse) {
            removePlaylistRows(children.first, children.count(), parent);
        }
    }

    cleanupHeaders();
    updateTrackIndexes();

    tracksChanged();
}

void PlaylistModel::removeTracks(const TrackGroups& groups)
{
    QModelIndexList rows;

    for(const auto& [index, tracks] : groups) {
        int currIndex   = index;
        const int total = index + static_cast<int>(tracks.size()) - 1;

        while(currIndex <= total) {
            auto [trackIndex, _] = trackIndexAtPlaylistIndex(currIndex, true);
            rows.push_back(trackIndex);
            currIndex++;
        }
    }

    removeTracks(rows);
}

void PlaylistModel::updateHeader(Playlist* playlist)
{
    if(playlist) {
        m_headerText = QStringLiteral("%1: %2 Tracks").arg(playlist->name()).arg(playlist->trackCount());
    }
}

bool PlaylistModel::removeColumn(int column)
{
    if(column < 0 || std::cmp_greater_equal(column, m_columns.size())) {
        return false;
    }

    beginRemoveColumns({}, column, column);

    m_columns.erase(m_columns.cbegin() + column);
    if(column < 0 || std::cmp_greater_equal(column, m_columnSizes.size())) {
        m_columnSizes.erase(m_columnSizes.cbegin() + column);
    }
    resetColumnAlignment(column);

    for(auto& [_, node] : m_nodes) {
        node.removeColumn(column);
    }

    endRemoveColumns();

    return true;
}

TrackGroups PlaylistModel::saveTrackGroups(const QModelIndexList& indexes)
{
    TrackGroups result;

    const IndexGroupsList indexGroups = determineIndexGroups(indexes);

    for(const auto& group : indexGroups) {
        const int index = group.front().data(PlaylistItem::Role::Index).toInt();
        std::ranges::transform(group, std::back_inserter(result[index]), [](const QModelIndex& trackIndex) {
            return trackIndex.data(Fooyin::PlaylistItem::Role::ItemData).value<Track>();
        });
    }

    return result;
}

void PlaylistModel::tracksAboutToBeChanged()
{
    if(!m_currentPlaylist) {
        return;
    }

    playingTrackChanged(m_playingTrack);
}

void PlaylistModel::tracksChanged()
{
    int playingIndex{-1};

    if(m_playingIndex.isValid()) {
        playingIndex = m_playingIndex.data(PlaylistItem::Index).toInt();
    }

    if(playingIndex >= 0) {
        m_playingTrack.indexInPlaylist = playingIndex;
    }
    else {
        playingIndex = m_playingTrack.indexInPlaylist;
    }

    emit playlistTracksChanged(playingIndex);
}

void PlaylistModel::playingTrackChanged(const PlaylistTrack& track)
{
    m_playingIndex = indexAtPlaylistIndex(track.indexInPlaylist, true);

    if(std::exchange(m_playingTrack, track) != track) {
        emit dataChanged({}, {}, {Qt::DecorationRole, Qt::BackgroundRole});
    }

    if(m_stopAtIndex.isValid()
       && m_playingIndex == m_stopAtIndex.sibling(m_stopAtIndex.row(), m_playingIndex.column())) {
        m_settings->set<Settings::Core::StopAfterCurrent>(true);
    }
}

void PlaylistModel::playStateChanged(Player::PlayState state)
{
    if(state == Player::PlayState::Stopped) {
        m_stopAtIndex = QPersistentModelIndex{};
    }
    if(std::exchange(m_currentPlayState, state) != state) {
        emit dataChanged({}, {}, {Qt::DecorationRole, Qt::BackgroundRole});
    }
}

void PlaylistModel::populateModel(PendingData& data)
{
    if(m_currentPlaylist && m_currentPlaylist->id() != data.playlistId) {
        return;
    }

    if(m_resetting) {
        beginResetModel();
        resetRoot();
        m_nodes.clear();
        m_pendingNodes.clear();
        m_trackParents.clear();
    }

    m_nodes.merge(data.items);
    mergeTrackParents(data.trackParents);

    if(m_resetting) {
        for(const auto& [parentKey, rows] : data.nodes) {
            auto* parent = parentKey.isNull() ? itemForIndex({}) : &m_nodes.at(parentKey);

            for(const auto& row : rows) {
                PlaylistItem* child = &m_nodes.at(row);
                parent->appendChild(child);
                child->setPending(false);
            }
        }
        updateTrackIndexes();
        endResetModel();
        m_resetting = false;
    }
    else {
        for(auto& [parentKey, rows] : data.nodes) {
            std::ranges::copy(rows, std::back_inserter(m_pendingNodes[parentKey]));
        }
    }
}

void PlaylistModel::populateTrackGroup(PendingData& data)
{
    if(m_currentPlaylist && m_currentPlaylist->id() != data.playlistId) {
        return;
    }

    tracksAboutToBeChanged();

    if(!m_indexesPendingRemoval.empty()) {
        const ParentChildRangesList indexGroups = determineRowGroups(m_indexesPendingRemoval);

        for(const auto& [parent, groups] : indexGroups) {
            for(const auto& children : groups | std::views::reverse) {
                removePlaylistRows(children.first, children.count(), parent);
            }
        }
        m_indexesPendingRemoval.clear();
    }

    if(m_nodes.empty()) {
        m_resetting = true;
        populateModel(data);
        tracksChanged();
        return;
    }

    handleTrackGroup(data);

    tracksChanged();
}

void PlaylistModel::updateModel(ItemKeyMap& data)
{
    if(m_resetting) {
        return;
    }

    for(auto& [key, header] : data) {
        m_nodes[key] = header;
        auto* node   = &m_nodes.at(key);
        node->setState(PlaylistItem::State::None);

        const QModelIndex headerIndex = indexOfItem(node);
        emit dataChanged(headerIndex, headerIndex, {});
    }
}

void PlaylistModel::updateTracks(const ItemList& tracks)
{
    if(m_resetting) {
        return;
    }

    for(const PlaylistItem& item : tracks) {
        if(m_nodes.contains(item.key())) {
            auto* node = &m_nodes.at(item.key());
            node->setData(item.data());
            node->setState(PlaylistItem::State::None);

            const QModelIndex trackIndex = indexOfItem(node);
            emit dataChanged(trackIndex, trackIndex.siblingAtColumn(columnCount(trackIndex) - 1), {});
        }
    }
}

void PlaylistModel::mergeTrackParents(const TrackIdNodeMap& parents)
{
    for(const auto& pair : parents) {
        const auto& [id, nodes] = pair;

        auto trackIt = m_trackParents.find(id);
        if(trackIt != m_trackParents.end()) {
            for(const auto& key : nodes) {
                if(m_nodes.contains(key)) {
                    trackIt->second.emplace_back(key);
                }
            }
        }
        else {
            m_trackParents.emplace(pair);
        }
    }
}

QVariant PlaylistModel::trackData(PlaylistItem* item, const QModelIndex& index, int role) const
{
    const int column = index.column();
    const auto track = std::get<PlaylistTrackItem>(item->data());

    const bool singleColumnMode = m_columns.empty();
    const bool isPlaying        = trackIsPlaying(track.track(), item->index());

    auto getCover = [this, &index, column](const Track::Cover type) -> QVariant {
        if(std::cmp_greater_equal(column, m_columnSizes.size())) {
            return {};
        }
        const int size          = m_columnSizes.at(column);
        const QModelIndex first = index.siblingAtRow(0);
        if(!first.isValid()) {
            return {};
        }
        if(const auto* firstSibling = itemForIndex(first)) {
            const auto firstTrack = std::get<PlaylistTrackItem>(firstSibling->data());
            return QVariant::fromValue(m_coverProvider->trackCoverThumbnail(firstTrack.track(), {size, size}, type));
        }
        return {};
    };

    if(role == Qt::DisplayRole && !m_columns.empty() && m_columns.at(column).field == QLatin1String{RatingEditor}) {
        return QVariant::fromValue(StarRating{track.track().rating(), 5, m_starRatingSize});
    }

    switch(role) {
        case(Qt::ToolTipRole): {
            if(!singleColumnMode) {
                return track.column(column).text.joinedText();
            }
            break;
        }
        case(PlaylistItem::Role::Column): {
            if(singleColumnMode) {
                break;
            }

            const QString field = m_columns.at(column).field;

            if(field == QLatin1String(FrontCover)) {
                return getCover(Track::Cover::Front);
            }
            if(field == QLatin1String(BackCover)) {
                return getCover(Track::Cover::Back);
            }
            if(field == QLatin1String(ArtistPicture)) {
                return getCover(Track::Cover::Artist);
            }
            if(field == QLatin1String(PlayingIcon) && isPlaying) {
                break;
            }

            return QVariant::fromValue(track.column(column).text);
        }
        case(PlaylistItem::Role::DecorationPosition): {
            if(singleColumnMode) {
                break;
            }
            const QString field = m_columns.at(column).field;
            return getIconPosition(field);
        }
        case(PlaylistItem::Role::TrackId):
            return track.track().id();
        case(PlaylistItem::Role::ImagePadding):
            return m_pixmapPadding;
        case(PlaylistItem::Role::ImagePaddingTop):
            return m_pixmapPaddingTop;
        case(PlaylistItem::Role::Left):
            return QVariant::fromValue(track.left().text);
        case(PlaylistItem::Role::Right):
            return QVariant::fromValue(track.right().text);
        case(PlaylistItem::Role::ItemData):
            return QVariant::fromValue<Track>(track.track());
        case(Qt::BackgroundRole): {
            if(!track.track().isEnabled()) {
                return m_disabledColour;
            }
            if(isPlaying) {
                return m_playingColour;
            }

            break;
        }
        case(Qt::SizeHintRole): {
            if(m_columns.empty()) {
                return track.size();
            }
            return track.size(column);
        }
        case(Qt::DecorationRole): {
            if(singleColumnMode || m_columns.at(column).field.contains(QLatin1String(PlayingIcon))) {
                if(!track.track().isEnabled()) {
                    return Utils::pixmapFromTheme(Constants::Icons::Close);
                }

                if(isPlaying) {
                    switch(m_currentPlayState) {
                        case(Player::PlayState::Playing):
                            return Utils::pixmapFromTheme(Constants::Icons::Play);
                        case(Player::PlayState::Paused):
                            return Utils::pixmapFromTheme(Constants::Icons::Pause);
                        case(Player::PlayState::Stopped):
                            break;
                    }
                }
                if(index == m_stopAtIndex) {
                    return Utils::pixmapFromTheme(Constants::Icons::Stop);
                }
            }
            break;
        }
        default:
            break;
    }

    return {};
}

QVariant PlaylistModel::headerData(PlaylistItem* item, int column, int role) const
{
    const auto& header = std::get<PlaylistContainerItem>(item->data());

    if(role == Qt::SizeHintRole) {
        return header.size();
    }

    if(column != 0) {
        return {};
    }

    switch(role) {
        case(PlaylistItem::Role::Title):
            return QVariant::fromValue(header.title().text);
        case(PlaylistItem::Role::Simple):
            return m_currentPreset.header.simple;
        case(PlaylistItem::Role::Subtitle):
            return QVariant::fromValue(header.subtitle().text);
        case(PlaylistItem::Role::Info):
            return QVariant::fromValue(header.info().text);
        case(PlaylistItem::Role::Right):
            return QVariant::fromValue(header.sideText().text);
        case(Qt::DecorationRole): {
            if(m_currentPreset.header.simple || !m_currentPreset.header.showCover) {
                return {};
            }
            return m_coverProvider->trackCoverThumbnail(header.tracks().front(), header.size(), Track::Cover::Front);
        }
        default:
            break;
    }

    return {};
}

QVariant PlaylistModel::subheaderData(PlaylistItem* item, int column, int role) const
{
    const auto& header = std::get<PlaylistContainerItem>(item->data());

    if(role == Qt::SizeHintRole) {
        return {};
    }

    if(column != 0) {
        return {};
    }

    switch(role) {
        case(PlaylistItem::Role::Title):
            return QVariant::fromValue(header.title().text);
        case(PlaylistItem::Role::Subtitle):
            return QVariant::fromValue(header.subtitle().text);
        default:
            break;
    }

    return {};
}

bool PlaylistModel::prepareDrop(const QMimeData* data, Qt::DropAction action, int row, int /*column*/,
                                const QModelIndex& parent)
{
    const int dropIndex = determineDropIndex(this, parent, row);

    if(data->hasUrls()) {
        emit filesDropped(data->urls(), dropIndex);
        return true;
    }

    bool sameModel{false};

    if(data->hasFormat(QString::fromLatin1(MimeModelId))) {
        QByteArray modelData = data->data(QString::fromLatin1(MimeModelId));
        QDataStream stream{&modelData, QIODevice::ReadOnly};

        UId modelId;
        stream >> modelId;

        sameModel = modelId == m_id;
    }

    const QByteArray playlistData = data->data(QString::fromLatin1(Constants::Mime::PlaylistItems));
    const bool samePlaylist       = sameModel && dropOnSamePlaylist(playlistData, m_currentPlaylist);

    if(samePlaylist && action == Qt::MoveAction) {
        const QModelIndexList indexes
            = restoreIndexes(this, data->data(QString::fromLatin1(Constants::Mime::PlaylistItems)), m_currentPlaylist);
        const TrackIndexRangeList indexRanges = determineTrackIndexGroups(indexes, parent, row);

        const int finalIndex = m_currentPlaylist->trackCount() - 1;
        const bool validMove = !std::ranges::all_of(indexRanges, [dropIndex, finalIndex](const auto& range) {
            return ((dropIndex >= range.first && dropIndex <= range.last + 1)
                    || (dropIndex == -1 && range.last >= finalIndex));
        });

        if(!validMove) {
            return false;
        }

        MoveOperation operation;
        operation.emplace_back(dropIndex, indexRanges);

        emit tracksMoved({operation});

        return true;
    }

    const auto tracks = Gui::tracksFromMimeData(m_library, data->data(QString::fromLatin1(Constants::Mime::TrackIds)));
    if(tracks.empty()) {
        return false;
    }

    const TrackGroups groups{{dropIndex, tracks}};
    emit tracksInserted(groups);

    return true;
}

PlaylistModel::DropTargetResult PlaylistModel::findDropTarget(PlaylistItem* source, PlaylistItem* target, int& row)
{
    DropTargetResult dropResult;

    if(source->baseKey() == target->baseKey()) {
        dropResult.fullMergeTarget = indexOfItem(target);
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
        dropResult = canBeMerged(currTarget, targetRow, sourceParents, -1);
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
        dropResult = canBeMerged(currTarget, targetRow, sourceParents, targetIsRoot ? 0 : 1);
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
                PlaylistItem* newParent = cloneParent(m_nodes, parent);

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
            const QModelIndex prevParent = indexOfItem(prevParentItem);

            insertPlaylistRows(prevParent, newParentRow, newParentRow, {parent.target});

            const QModelIndex oldParentIndex = indexOfItem(parent.source);
            const QModelIndex newParentIndex = indexOfItem(parent.target);
            movePlaylistRows(oldParentIndex, parent.firstRow, parent.finalRow, newParentIndex, 0, parent.children);

            prevParentItem = parent.target;
            newParentRow   = 0;
        }
    }

    prevParentItem = currTarget;
    newParentRow   = row;

    // Create parents for dropped rows
    for(PlaylistItem* parent : sourceParents) {
        const QModelIndex prevParent = indexOfItem(prevParentItem);
        PlaylistItem* newParent      = parent->pending() ? parent : cloneParent(m_nodes, parent);

        if(!m_nodes.contains(newParent->key())) {
            newParent = &m_nodes.emplace(newParent->key(), *newParent).first->second;
        }

        insertPlaylistRows(prevParent, newParentRow, newParentRow, {newParent});

        prevParentItem = newParent;
        newParentRow   = 0;
    }

    if(!sourceParents.empty()) {
        row = 0;
    }

    if(dropResult.partMergeTarget.isValid()) {
        dropResult.partMergeTarget = indexOfItem(prevParentItem);
    }
    else {
        dropResult.target = indexOfItem(prevParentItem);
    }

    return dropResult;
}

PlaylistModel::DropTargetResult PlaylistModel::canBeMerged(PlaylistItem*& currTarget, int& targetRow,
                                                           PlaylistItemList& sourceParents, int targetOffset)
{
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
        return {.fullMergeTarget = indexOfItem(currTarget), .partMergeTarget = {}, .target = {}};
    }

    sourceParents = newSourceParents;
    return {.fullMergeTarget = {}, .partMergeTarget = indexOfItem(currTarget), .target = {}};
}

void PlaylistModel::handleTrackGroup(PendingData& data)
{
    updateTrackIndexes();

    auto cmpParentKeys = [data](const UId& key1, const UId& key2) {
        if(key1 == key2) {
            return false;
        }
        return std::ranges::find(data.containerOrder, key1) < std::ranges::find(data.containerOrder, key2);
    };
    using ParentItemMap = std::map<UId, PlaylistItemList, decltype(cmpParentKeys)>;
    std::map<int, ParentItemMap> itemData;

    auto nodeForKey = [this, &data](const UId& key) -> PlaylistItem* {
        if(key.isNull()) {
            return rootItem();
        }
        if(data.items.contains(key)) {
            return &data.items.at(key);
        }
        return nullptr;
    };

    for(const auto& [index, childKeys] : data.indexNodes) {
        ParentItemMap childrenMap(cmpParentKeys);
        for(const auto& childKey : childKeys) {
            if(PlaylistItem* child = nodeForKey(childKey)) {
                if(child->parent()) {
                    childrenMap[child->parent()->key()].push_back(child);
                }
            }
        }
        itemData.emplace(index, childrenMap);
    }

    for(const auto& [index, childGroups] : itemData) {
        for(const auto& [sourceParentKey, children] : childGroups | std::views::reverse) {
            auto* sourceParentItem = nodeForKey(sourceParentKey);
            if(!sourceParentItem) {
                continue;
            }

            const auto [beforeIndex, end] = trackIndexAtPlaylistIndex(index, true);
            QModelIndex targetParent{beforeIndex.parent()};
            int row = beforeIndex.row() + (end ? 1 : 0);

            PlaylistItem* targetParentItem = itemForIndex(targetParent);

            const auto targetResult = findDropTarget(sourceParentItem, targetParentItem, row);
            targetParent            = targetResult.dropTarget();

            const int total = row + static_cast<int>(children.size()) - 1;

            beginInsertRows(targetParent, row, total);
            dropInsertRows(children, targetParent, row);
            endInsertRows();

            rootItem()->resetChildren();
            updateTrackIndexes();
        }
    }

    mergeTrackParents(data.trackParents);
    cleanupHeaders();
}

void PlaylistModel::storeMimeData(const QModelIndexList& indexes, QMimeData* mimeData) const
{
    if(mimeData) {
        QByteArray modelId;
        QDataStream stream{&modelId, QIODevice::WriteOnly};
        stream << m_id;
        mimeData->setData(QString::fromLatin1(MimeModelId), modelId);

        QModelIndexList sortedIndexes{indexes};
        std::ranges::sort(sortedIndexes, Utils::sortModelIndexes);
        mimeData->setData(QString::fromLatin1(Constants::Mime::TrackIds), saveTracks(sortedIndexes));
        if(m_currentPlaylist) {
            mimeData->setData(QString::fromLatin1(Constants::Mime::PlaylistItems),
                              saveIndexes(sortedIndexes, m_currentPlaylist));
            mimeData->setData(QString::fromLatin1(Constants::Mime::QueueTracks),
                              Gui::queueTracksToMimeData(savePlaylistTracks(m_currentPlaylist->id(), sortedIndexes)));
        }
    }
}

int PlaylistModel::dropInsertRows(const PlaylistItemList& rows, const QModelIndex& target, int row)
{
    auto* targetParent = itemForIndex(target);
    if(!targetParent) {
        return row;
    }

    for(Fooyin::PlaylistItem* childItem : rows) {
        childItem->resetRow();
        auto* newChild = &m_nodes.emplace(childItem->key(), *childItem).first->second;

        targetParent->insertChild(row, newChild);
        newChild->setPending(false);
        row++;
    }
    targetParent->resetChildren();

    return row;
}

int PlaylistModel::dropMoveRows(const QModelIndex& source, const PlaylistItemList& rows, const QModelIndex& target,
                                int row)
{
    int currRow{row};
    auto* targetParent = itemForIndex(target);
    if(!targetParent) {
        return currRow;
    }

    auto* sourceParent = itemForIndex(source);
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

int PlaylistModel::dropCopyRows(const QModelIndex& source, const PlaylistItemList& rows, const QModelIndex& target,
                                int row)
{
    return dropCopyRowsRecursive(source, rows, target, row);
}

int PlaylistModel::dropCopyRowsRecursive(const QModelIndex& source, const PlaylistItemList& rows,
                                         const QModelIndex& target, int row)
{
    int currRow{row};
    auto* targetParent = itemForIndex(target);
    if(!targetParent) {
        return currRow;
    }

    auto* sourceParent = itemForIndex(source);
    for(Fooyin::PlaylistItem* childItem : rows) {
        childItem->resetRow();
        const auto newKey{UId::create()};
        auto* newChild = &m_nodes.emplace(newKey, *childItem).first->second;
        newChild->clearChildren();
        newChild->setKey(newKey);

        targetParent->insertChild(currRow, newChild);

        const QModelIndex childSourceIndex = indexOfItem(childItem);
        const QModelIndex childTargetIndex = indexOfItem(newChild);

        dropCopyRowsRecursive(childSourceIndex, childItem->children(), childTargetIndex, 0);

        ++currRow;
    }

    sourceParent->resetChildren();
    targetParent->resetChildren();

    return currRow;
}

bool PlaylistModel::insertPlaylistRows(const QModelIndex& target, int firstRow, int lastRow,
                                       const PlaylistItemList& children)
{
    const int diff = firstRow == lastRow ? 1 : lastRow - firstRow + 1;
    if(static_cast<int>(children.size()) != diff) {
        return false;
    }

    auto* parent = itemForIndex(target);

    beginInsertRows(target, firstRow, lastRow);
    for(PlaylistItem* child : children) {
        parent->insertChild(firstRow, child);
        child->setPending(false);
        firstRow += 1;
    }
    endInsertRows();

    return true;
}

bool PlaylistModel::movePlaylistRows(const QModelIndex& source, int firstRow, int lastRow, const QModelIndex& target,
                                     int row, const PlaylistItemList& children)
{
    const int diff = firstRow == lastRow ? 1 : lastRow - firstRow + 1;
    if(static_cast<int>(children.size()) != diff) {
        return false;
    }

    if(beginMoveRows(source, firstRow, lastRow, target, row)) {
        dropMoveRows(source, children, target, row);
        endMoveRows();
        return true;
    }

    return false;
}

bool PlaylistModel::removePlaylistRows(int row, int count, const QModelIndex& parent)
{
    auto* parentItem = itemForIndex(parent);
    if(!parentItem) {
        return false;
    }

    const int numRows = rowCount(parent);
    if(row < 0 || (row + count - 1) >= numRows) {
        return false;
    }

    int lastRow = row + count - 1;
    beginRemoveRows(parent, row, lastRow);
    while(lastRow >= row) {
        auto* childItem = parentItem->child(lastRow);
        parentItem->removeChild(lastRow);
        deleteNodes(childItem);
        --lastRow;
    }
    parentItem->resetChildren();
    endRemoveRows();

    return true;
}

bool PlaylistModel::removePlaylistRows(int row, int count, PlaylistItem* parent)
{
    if(!parent) {
        return false;
    }

    const int numRows = parent->childCount();
    if(row < 0 || (row + count - 1) >= numRows) {
        return false;
    }

    const QModelIndex parentIndex = indexOfItem(parent);
    int lastRow                   = row + count - 1;

    beginRemoveRows(parentIndex, row, lastRow);
    while(lastRow >= row) {
        auto* childItem = parent->child(lastRow);
        parent->removeChild(lastRow);
        deleteNodes(childItem);
        --lastRow;
    }
    parent->resetChildren();
    endRemoveRows();

    return true;
}

void PlaylistModel::fetchChildren(PlaylistItem* parent, PlaylistItem* child)
{
    const auto key = child->key();

    if(m_pendingNodes.contains(key)) {
        auto& childRows = m_pendingNodes.at(key);

        for(const auto& childRow : childRows) {
            PlaylistItem& childItem = m_nodes.at(childRow);
            fetchChildren(child, &childItem);
        }
        m_pendingNodes.erase(key);
    }

    parent->appendChild(child);
}

void PlaylistModel::cleanupHeaders()
{
    removeEmptyHeaders();
    mergeHeaders();
    updateHeaders();
}

void PlaylistModel::removeEmptyHeaders()
{
    ItemPtrSet headersToRemove;
    for(auto& [_, node] : m_nodes) {
        if(node.state() == PlaylistItem::State::Delete) {
            headersToRemove.emplace(&node);
        }
    }

    if(headersToRemove.empty()) {
        return;
    }

    const ItemPtrSet topLevelHeaders = optimiseSelection(headersToRemove);

    for(PlaylistItem* item : topLevelHeaders) {
        const QModelIndex index = indexOfItem(item);
        removePlaylistRows(index.row(), 1, index.parent());
    }
}

void PlaylistModel::mergeHeaders()
{
    std::deque<PlaylistItem*> headers;
    headers.emplace_back(rootItem());

    auto addChildren = [&headers](PlaylistItem* parent) {
        const auto children = parent->children();
        for(PlaylistItem* child : children) {
            headers.emplace_back(child);
        }
    };

    while(!headers.empty()) {
        const PlaylistItem* parent = headers.front();
        headers.pop_front();

        if(!parent || parent->childCount() < 1) {
            continue;
        }

        int row{0};
        const int childCount = parent->childCount();

        while(row < childCount) {
            PlaylistItem* leftSibling  = parent->child(row);
            PlaylistItem* rightSibling = parent->child(row + 1);

            if(leftSibling) {
                headers.emplace_back(leftSibling);
                addChildren(leftSibling);
            }

            if(!leftSibling || !rightSibling) {
                ++row;
                continue;
            }

            const QModelIndex leftIndex  = indexOfItem(leftSibling);
            const QModelIndex rightIndex = indexOfItem(rightSibling);

            const bool bothHeaders
                = leftSibling->type() != PlaylistItem::Track && rightSibling->type() != PlaylistItem::Track;

            if(bothHeaders && leftIndex != rightIndex && leftSibling->baseKey() == rightSibling->baseKey()) {
                const auto rightChildren = rightSibling->children();
                const int targetRow      = leftSibling->childCount();
                const int lastRow        = rightSibling->childCount() - 1;

                if(movePlaylistRows(rightIndex, 0, lastRow, leftIndex, targetRow, rightChildren)) {
                    std::erase(headers, rightSibling);
                    removePlaylistRows(rightSibling->row(), 1, rightIndex.parent());
                }
            }
            else {
                ++row;
            }
        }
    }

    rootItem()->resetChildren();
}

void PlaylistModel::updateHeaders()
{
    ItemPtrSet items;

    for(auto& [_, header] : m_nodes) {
        if(header.state() == PlaylistItem::State::Update) {
            PlaylistItem* currHeader{&header};
            while(currHeader && currHeader->type() != PlaylistItem::Root) {
                if(currHeader->childCount() > 0) {
                    items.emplace(currHeader);
                }
                currHeader = currHeader->parent();
            }
        }
    }

    ItemList updatedHeaders;

    for(PlaylistItem* header : items) {
        PlaylistItem updatedHeader = *header;
        updateHeaderChildren(&updatedHeader);
        updatedHeaders.emplace_back(updatedHeader);
    }

    QMetaObject::invokeMethod(&m_populator, [this, updatedHeaders]() { m_populator.updateHeaders(updatedHeaders); });
}

void PlaylistModel::updateTrackIndexes()
{
    std::stack<PlaylistItem*> trackNodes;
    trackNodes.push(rootItem());
    int index{0};

    m_trackIndexes.clear();

    while(!trackNodes.empty()) {
        PlaylistItem* node = trackNodes.top();
        trackNodes.pop();

        if(!node) {
            continue;
        }

        if(node->type() == PlaylistItem::Track) {
            m_trackIndexes.emplace(index, node->key());
            node->setIndex(index++);
        }

        const auto children = node->children();
        for(PlaylistItem* child : children | std::views::reverse) {
            trackNodes.push(child);
        }
    }
}

void PlaylistModel::deleteNodes(PlaylistItem* node)
{
    if(!node) {
        return;
    }

    const auto children = node->children();
    for(PlaylistItem* child : children) {
        deleteNodes(child);
    }

    m_nodes.erase(node->key());
}

std::vector<int> PlaylistModel::pixmapColumns() const
{
    std::vector<int> columns;

    for(int i{0}; const auto& column : m_columns) {
        if(column.isPixmap) {
            columns.emplace_back(i);
        }
        ++i;
    }

    return columns;
}

void PlaylistModel::coverUpdated(const Track& track)
{
    if(!m_trackParents.contains(track.id())) {
        return;
    }

    const auto parents   = m_trackParents.at(track.id());
    const bool hasPixmap = !m_pixmapColumns.empty();

    for(const auto& parentKey : parents) {
        if(m_nodes.contains(parentKey)) {
            auto* parentItem = &m_nodes.at(parentKey);

            if(parentItem->type() == PlaylistItem::Header) {
                const QModelIndex nodeIndex = indexOfItem(parentItem);
                emit dataChanged(nodeIndex, nodeIndex.siblingAtColumn(columnCount(nodeIndex) - 1),
                                 {Qt::DecorationRole});
            }
            else if(hasPixmap && parentItem->type() == PlaylistItem::Track) {
                const QModelIndex nodeIndex = indexOfItem(parentItem);
                for(const int col : m_pixmapColumns) {
                    emit dataChanged(nodeIndex.siblingAtColumn(col),
                                     nodeIndex.sibling(rowCount(nodeIndex.parent()) - 1, col), {PlaylistItem::Column});
                }
            }
        }
    }
}

bool PlaylistModel::trackIsPlaying(const Track& track, int index) const
{
    return m_currentPlayState != Player::PlayState::Stopped && m_currentPlaylist
        && m_playingTrack.playlistId == m_currentPlaylist->id() && m_playingTrack.track.id() == track.id()
        && m_playingTrack.indexInPlaylist == index;
}

ParentChildRangesList PlaylistModel::determineRowGroups(const QModelIndexList& indexes)
{
    ParentChildRangesList indexGroups;

    QModelIndexList sortedIndexes{indexes};
    std::ranges::sort(sortedIndexes, Utils::sortModelIndexes);

    auto startOfSequence = sortedIndexes.cbegin();
    while(startOfSequence != sortedIndexes.cend()) {
        auto endOfSequence
            = std::adjacent_find(startOfSequence, sortedIndexes.cend(), [](const auto& lhs, const auto& rhs) {
                  return lhs.parent() != rhs.parent() || rhs.row() != lhs.row() + 1;
              });
        if(endOfSequence != sortedIndexes.cend()) {
            std::advance(endOfSequence, 1);
        }

        TrackIndexRange indexRange;
        indexRange.first = startOfSequence->row();
        indexRange.last  = std::prev(endOfSequence)->row();

        auto* parent = itemForIndex(startOfSequence->parent());
        auto it = std::ranges::find_if(indexGroups, [&parent](const auto& range) { return range.parent == parent; });
        if(it != indexGroups.end()) {
            it->ranges.push_back(indexRange);
        }
        else {
            indexGroups.emplace_back(parent, std::vector<TrackIndexRange>{indexRange});
        }

        startOfSequence = endOfSequence;
    }

    return indexGroups;
}

PlaylistModel::MoveOperationMap PlaylistModel::determineMoveOperationGroups(const MoveOperation& operation, bool merge)
{
    MoveOperationMap result;

    for(const auto& [index, tracks] : operation) {
        for(const auto& range : tracks) {
            Fooyin::PlaylistItemList rows;
            rows.reserve(range.count());

            for(int trackIndex{range.first}; trackIndex <= range.last; ++trackIndex) {
                rows.push_back(itemForTrackIndex(trackIndex).item);
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

PlaylistModel::TrackItemResult PlaylistModel::itemForTrackIndex(int index)
{
    while(!m_trackIndexes.contains(index) && canFetchMore({})) {
        fetchMore({});
    }

    if(m_trackIndexes.contains(index)) {
        const auto& key = m_trackIndexes.at(index);
        if(m_nodes.contains(key)) {
            return {&m_nodes.at(key), false};
        }
    }

    // End of playlist - return last track item
    PlaylistItem* current = itemForIndex({});
    while(current->childCount() > 0) {
        current = current->child(current->childCount() - 1);
    }
    return {current, true};
}
} // namespace Fooyin

#include "moc_playlistmodel.cpp"
