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

#include "librarytreemodel.h"

#include "internalguisettings.h"
#include "librarytreepopulator.h"

#include <core/constants.h>
#include <core/coresettings.h>
#include <gui/coverprovider.h>
#include <gui/guiconstants.h>
#include <utils/datastream.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QIODevice>
#include <QIcon>
#include <QMimeData>
#include <QPalette>
#include <QSize>
#include <QThread>

#include <ranges>
#include <set>
#include <stack>

using namespace Qt::StringLiterals;

namespace {
bool cmpItemsReverse(Fooyin::LibraryTreeItem* pItem1, Fooyin::LibraryTreeItem* pItem2)
{
    Fooyin::LibraryTreeItem* item1{pItem1};
    Fooyin::LibraryTreeItem* item2{pItem2};

    while(item1->parent() != item2->parent()) {
        if(item1->parent() == item2) {
            return true;
        }
        if(item2->parent() == item1) {
            return false;
        }
        if(item1->parent()->row() >= 0) {
            item1 = item1->parent();
        }
        if(item2->parent()->row() >= 0) {
            item2 = item2->parent();
        }
    }
    if(item1->row() == item2->row()) {
        return false;
    }
    return item1->row() > item2->row();
};

struct cmpItems
{
    bool operator()(Fooyin::LibraryTreeItem* pItem1, Fooyin::LibraryTreeItem* pItem2) const
    {
        return cmpItemsReverse(pItem1, pItem2);
    }
};

Fooyin::LibraryTreeItem* treeItem(const QModelIndex& index)
{
    return static_cast<Fooyin::LibraryTreeItem*>(index.internalPointer());
}
} // namespace

namespace Fooyin {
LibraryTreeSortModel::LibraryTreeSortModel(QObject* parent)
    : QSortFilterProxyModel{parent}
{ }

bool LibraryTreeSortModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const auto* leftItem  = treeItem(left);
    const auto* rightItem = treeItem(right);

    if(!leftItem || !rightItem) {
        return QSortFilterProxyModel::lessThan(left, right);
    }

    if(leftItem->level() == -1) {
        return sortOrder() == Qt::AscendingOrder;
    }

    if(rightItem->level() == -1) {
        return sortOrder() != Qt::AscendingOrder;
    }

    const auto cmp = m_collator.compare(leftItem->title(), rightItem->title());

    if(cmp == 0) {
        return false;
    }

    return cmp < 0;
}

class LibraryTreeModelPrivate
{
public:
    explicit LibraryTreeModelPrivate(LibraryTreeModel* self, LibraryManager* libraryManager,
                                     std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings);

    void updateSummary();

    void removeTracks(const TrackList& tracks);
    void mergeTrackParents(const TrackIdNodeMap& parents);

    void batchFinished(PendingTreeData data);

    void traverseTree(const QModelIndex& index, Fooyin::TrackIds& trackIds);
    QByteArray saveTracks(const QModelIndexList& indexes);

    void updatePendingNodes(const PendingTreeData& data);
    void populateModel(PendingTreeData& data);
    void beginReset();

    LibraryTreeModel* m_self;
    std::shared_ptr<AudioLoader> m_audioLoader;
    SettingsManager* m_settings;

    CoverProvider m_coverProvider;

    QString m_grouping;
    bool m_loaded{false};
    bool m_resetting{false};

    QThread m_populatorThread;
    LibraryTreePopulator m_populator;

    LibraryTreeItem m_summaryNode;
    NodeKeyMap m_pendingNodes;
    ItemKeyMap m_nodes;
    TrackIdNodeMap m_trackParents;
    std::unordered_set<Md5Hash> m_addedNodes;
    bool m_addingTracks{false};

    TrackList m_tracksPendingRemoval;

    Player::PlayState m_playingState;
    QString m_parentNode;
    QString m_playingPath;
    int m_rowHeight{0};
    QColor m_playingColour{QApplication::palette().highlight().color()};
    CoverProvider::ThumbnailSize m_iconSize;
};

LibraryTreeModelPrivate::LibraryTreeModelPrivate(LibraryTreeModel* self, LibraryManager* libraryManager,
                                                 std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings)
    : m_self{self}
    , m_audioLoader{std::move(audioLoader)}
    , m_settings{settings}
    , m_coverProvider{m_audioLoader, m_settings}
    , m_populator{libraryManager}
    , m_iconSize{
          CoverProvider::findThumbnailSize(m_settings->value<Settings::Gui::Internal::LibTreeIconSize>().toSize())}
{
    m_playingColour.setAlpha(90);

    m_populator.moveToThread(&m_populatorThread);
}

void LibraryTreeModelPrivate::updateSummary()
{
    m_summaryNode.setTitle(u"All Music (%1)"_s.arg(m_self->rootItem()->childCount() - 1));
}

void LibraryTreeModelPrivate::removeTracks(const TrackList& tracks)
{
    std::set<LibraryTreeItem*, cmpItems> items;
    std::set<LibraryTreeItem*> pendingItems;

    for(const Track& track : tracks) {
        const int id = track.id();
        if(!m_trackParents.contains(id)) {
            continue;
        }

        const auto trackNodes = m_trackParents[id];
        for(const auto& node : trackNodes) {
            if(m_nodes.contains(node)) {
                LibraryTreeItem* item = &m_nodes[node];
                item->removeTrack(track);
                if(item->pending()) {
                    pendingItems.emplace(item);
                }
                else {
                    items.emplace(item);
                }
            }
        }
        m_trackParents.erase(id);
    }

    for(const LibraryTreeItem* item : pendingItems) {
        if(item->trackCount() == 0) {
            m_pendingNodes.erase(item->key());
            m_nodes.erase(item->key());
        }
    }

    for(LibraryTreeItem* item : items) {
        if(item->trackCount() == 0) {
            LibraryTreeItem* parent = item->parent();
            const int row           = item->row();
            m_self->beginRemoveRows(m_self->indexOfItem(parent), row, row);
            parent->removeChild(row);
            parent->resetChildren();
            m_self->endRemoveRows();
            m_nodes.erase(item->key());
        }
    }

    updateSummary();
}

void LibraryTreeModelPrivate::mergeTrackParents(const TrackIdNodeMap& parents)
{
    for(const auto& pair : parents) {
        const auto& [id, children] = pair;

        auto trackIt = m_trackParents.find(id);
        if(trackIt != m_trackParents.end()) {
            for(const auto& key : children) {
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

void LibraryTreeModelPrivate::batchFinished(PendingTreeData data)
{
    if(m_resetting) {
        m_self->beginResetModel();
        beginReset();
    }

    if(!m_tracksPendingRemoval.empty()) {
        removeTracks(m_tracksPendingRemoval);
    }

    populateModel(data);

    if(m_resetting) {
        m_self->endResetModel();
    }
    m_resetting = false;

    while(m_self->canFetchMore({})) {
        m_self->fetchMore({});
    }

    if(m_addingTracks) {
        m_addingTracks = false;
    }
    else {
        QMetaObject::invokeMethod(m_self, &LibraryTreeModel::modelUpdated);
    }
}

void LibraryTreeModelPrivate::traverseTree(const QModelIndex& index, TrackIds& trackIds)
{
    if(!index.isValid()) {
        return;
    }

    const auto childCount = index.model()->rowCount(index);
    if(childCount == 0) {
        const auto tracks = index.data(Fooyin::LibraryTreeItem::Tracks).value<Fooyin::TrackList>();
        std::ranges::transform(std::as_const(tracks), std::back_inserter(trackIds),
                               [](const Fooyin::Track& track) { return track.id(); });
    }
    else {
        for(int i{0}; i < childCount; ++i) {
            traverseTree(m_self->index(i, 0, index), trackIds);
        }
    }
}

QByteArray LibraryTreeModelPrivate::saveTracks(const QModelIndexList& indexes)
{
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);

    TrackIds trackIds;

    for(const QModelIndex& index : indexes) {
        traverseTree(index, trackIds);
    }

    stream << trackIds;

    return result;
}

void LibraryTreeModelPrivate::updatePendingNodes(const PendingTreeData& data)
{
    std::set<QModelIndex> nodesToCheck;

    for(const auto& [parentKey, rows] : data.nodes) {
        auto* parent = parentKey.isEmpty()         ? m_self->rootItem()
                     : m_nodes.contains(parentKey) ? &m_nodes.at(parentKey)
                                                   : nullptr;
        if(!parent) {
            continue;
        }

        for(const auto& row : rows) {
            auto* node = m_nodes.contains(row) ? &m_nodes.at(row) : nullptr;

            if(node && node->pending() && !m_addedNodes.contains(row)) {
                if(!parent->pending() && !m_pendingNodes.contains(parentKey) && parent->childCount() > 0) {
                    // Parent is expanded/visible
                    nodesToCheck.emplace(m_self->indexOfItem(parent));
                }
                m_pendingNodes[parentKey].push_back(row);
                m_addedNodes.insert(row);
            }
        }
    }

    for(const QModelIndex& index : nodesToCheck) {
        if(m_self->canFetchMore(index)) {
            m_self->fetchMore(index);
        }
    }
}

void LibraryTreeModelPrivate::populateModel(PendingTreeData& data)
{
    for(const auto& [key, item] : data.items) {
        if(m_nodes.contains(key)) {
            auto& node = m_nodes.at(key);
            node.addTracks(item.tracks());
            node.sortTracks();
        }
        else {
            m_nodes[key] = item;
        }
    }
    mergeTrackParents(data.trackParents);

    const QModelIndex allIndex = m_self->indexOfItem(&m_summaryNode);
    emit m_self->dataChanged(allIndex, allIndex, {Qt::DisplayRole});

    if(m_resetting) {
        for(const auto& [parentKey, rows] : data.nodes) {
            auto* parent = parentKey.isEmpty() ? m_self->rootItem() : &m_nodes.at(parentKey);

            for(const auto& row : rows) {
                LibraryTreeItem* child = &m_nodes.at(row);
                parent->appendChild(child);
                child->setPending(false);
            }
        }
        m_self->rootItem()->resetChildren();
    }
    else {
        updatePendingNodes(data);
    }

    updateSummary();
}

void LibraryTreeModelPrivate::beginReset()
{
    m_self->resetRoot();
    m_nodes.clear();
    m_pendingNodes.clear();
    m_addedNodes.clear();

    m_summaryNode = LibraryTreeItem{u"All Music"_s, m_self->rootItem(), -1};
    m_self->rootItem()->appendChild(&m_summaryNode);
    updateSummary();
}

LibraryTreeModel::LibraryTreeModel(LibraryManager* libraryManager, const std::shared_ptr<AudioLoader>& audioLoader,
                                   SettingsManager* settings, QObject* parent)
    : TreeModel{parent}
    , p{std::make_unique<LibraryTreeModelPrivate>(this, libraryManager, audioLoader, settings)}
{
    QObject::connect(&p->m_populator, &LibraryTreePopulator::populated, this,
                     [this](const PendingTreeData& data) { p->batchFinished(data); });

    QObject::connect(&p->m_populator, &Worker::finished, this, [this]() {
        p->updateSummary();
        p->m_populator.stopThread();
        p->m_populatorThread.quit();
        if(!p->m_loaded) {
            p->m_loaded = true;
            emit modelLoaded();
        }
    });

    QObject::connect(&p->m_coverProvider, &CoverProvider::coverAdded, this, [this](const Track& track) {
        if(p->m_trackParents.contains(track.id())) {
            const auto items = p->m_trackParents.at(track.id());
            for(const auto& itemHash : items) {
                if(p->m_nodes.contains(itemHash)) {
                    const QModelIndex index = indexOfItem(&p->m_nodes.at(itemHash));
                    emit dataChanged(index, index, {Qt::DecorationRole});
                }
            }
        }
    });

    p->m_settings->subscribe<Settings::Gui::Internal::LibTreeIconSize>(this, [this](const auto& size) {
        p->m_iconSize = CoverProvider::findThumbnailSize(size.toSize());
        invalidateData();
    });
}

LibraryTreeModel::~LibraryTreeModel()
{
    p->m_populator.stopThread();
    p->m_populatorThread.quit();
    p->m_populatorThread.wait();
}

void LibraryTreeModel::resetPalette()
{
    p->m_playingColour = QApplication::palette().highlight().color();
    p->m_playingColour.setAlpha(90);
    invalidateData();
}

void LibraryTreeModel::setRowHeight(int height)
{
    p->m_rowHeight = height;
    emit dataUpdated({}, {});
}

void LibraryTreeModel::setPlayState(Player::PlayState state)
{
    p->m_playingState = state;
    emit dataUpdated({}, {}, {Qt::DecorationRole, Qt::BackgroundRole});
}

void LibraryTreeModel::setPlayingPath(const QString& parentNode, const QString& path)
{
    p->m_parentNode  = parentNode;
    p->m_playingPath = path;
    emit dataUpdated({}, {}, {Qt::DecorationRole, Qt::BackgroundRole});
}

Qt::ItemFlags LibraryTreeModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(index);
    defaultFlags |= Qt::ItemIsDragEnabled;
    return defaultFlags;
}

QVariant LibraryTreeModel::headerData(int /*section*/, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Orientation::Vertical) {
        return {};
    }

    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(role != Qt::DisplayRole) {
        return {};
    }

    return tr("Library Tree");
}

QVariant LibraryTreeModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    const auto* item = itemForIndex(index);

    if(p->m_playingState != Player::PlayState::Stopped) {
        const bool isPlayingTrack = item->childCount() == 0 && item->trackCount() == 1
                                 && item->tracks().front().uniqueFilepath() == p->m_playingPath
                                 && item->parent()->title() == p->m_parentNode;
        if(isPlayingTrack) {
            if(role == Qt::BackgroundRole) {
                return p->m_playingColour;
            }
            if(role == Qt::DecorationRole) {
                switch(p->m_playingState) {
                    case(Player::PlayState::Playing):
                        return Utils::pixmapFromTheme(Constants::Icons::Play);
                    case(Player::PlayState::Paused):
                        return Utils::pixmapFromTheme(Constants::Icons::Pause);
                    case(Player::PlayState::Stopped):
                        break;
                }
            }
        }
    }

    switch(role) {
        case(Qt::DisplayRole):
        case(Qt::ToolTipRole): {
            const QString& name = item->title();
            return !name.isEmpty() ? name : u"?"_s;
        }
        case(LibraryTreeItem::Title):
            return item->title();
        case(LibraryTreeItem::Level):
            return item->level();
        case(LibraryTreeItem::Key):
            return QVariant::fromValue(item->key());
        case(LibraryTreeItem::Tracks):
            return QVariant::fromValue(item->tracks());
        case(LibraryTreeItem::TrackCount):
            return item->trackCount();
        case(Qt::SizeHintRole): {
            if(p->m_rowHeight > 0) {
                return QSize{0, p->m_rowHeight};
            }
            break;
        }
        case(LibraryTreeItem::Role::DecorationPosition): {
            if(item->coverType()) {
                return item->coverPosition();
            }
            break;
        }
        case(Qt::DecorationRole): {
            if(item->trackCount() > 0) {
                if(const auto cover = item->coverType()) {
                    return p->m_coverProvider.trackCoverThumbnail(item->tracks().front(), p->m_iconSize, cover.value());
                }
            }
            break;
        }
        default:
            break;
    }

    return {};
}

bool LibraryTreeModel::hasChildren(const QModelIndex& parent) const
{
    if(!parent.isValid()) {
        return true;
    }
    LibraryTreeItem* item = itemForIndex(parent);
    return item->childCount() > 0 || p->m_pendingNodes.contains(item->key());
}

void LibraryTreeModel::fetchMore(const QModelIndex& parent)
{
    auto* parentItem = itemForIndex(parent);
    auto& rows       = p->m_pendingNodes[parentItem->key()];

    const int row           = parentItem->childCount();
    const int totalRows     = static_cast<int>(rows.size());
    const int rowCount      = parent.isValid() ? totalRows : std::min(100, totalRows);
    const auto rowsToInsert = std::ranges::views::take(rows, rowCount);

    beginInsertRows(parent, row, row + rowCount - 1);
    for(const auto& pendingRow : rowsToInsert) {
        LibraryTreeItem* child = &p->m_nodes.at(pendingRow);
        parentItem->appendChild(child);
        child->setPending(false);
        p->m_addedNodes.erase(pendingRow);
    }
    endInsertRows();

    rootItem()->resetChildren();

    rows.erase(rows.begin(), rows.begin() + rowCount);

    if(rows.empty()) {
        p->m_pendingNodes.erase(parentItem->key());
    }
}

bool LibraryTreeModel::canFetchMore(const QModelIndex& parent) const
{
    auto* item = itemForIndex(parent);
    return p->m_pendingNodes.contains(item->key()) && !p->m_pendingNodes[item->key()].empty();
}

QStringList LibraryTreeModel::mimeTypes() const
{
    return {QString::fromLatin1(Constants::Mime::TrackIds)};
}

Qt::DropActions LibraryTreeModel::supportedDragActions() const
{
    return Qt::CopyAction;
}

QMimeData* LibraryTreeModel::mimeData(const QModelIndexList& indexes) const
{
    auto* mimeData = new QMimeData();
    mimeData->setData(QString::fromLatin1(Constants::Mime::TrackIds), p->saveTracks(indexes));
    return mimeData;
}

QModelIndexList LibraryTreeModel::findIndexes(const QStringList& values) const
{
    QModelIndexList indexes;

    std::stack<LibraryTreeItem*> stack;
    stack.emplace(rootItem());

    while(!stack.empty()) {
        auto* item = stack.top();
        stack.pop();

        const auto children = item->children();
        for(auto* child : children) {
            if(values.contains(child->title()) && !child->pending()) {
                indexes.append(indexOfItem(child));
            }
            stack.push(child);
        }
    }

    return indexes;
}

QModelIndexList LibraryTreeModel::indexesForTracks(const TrackList& tracks) const
{
    if(tracks.empty()) {
        return {};
    }

    QModelIndexList parents;

    for(const Track& track : tracks) {
        const int id = track.id();
        if(!p->m_trackParents.contains(id)) {
            continue;
        }

        const auto trackNodes = p->m_trackParents[id];
        for(const auto& node : trackNodes) {
            if(p->m_nodes.contains(node) && !p->m_nodes[node].pending()) {
                parents.emplace_back(indexOfItem(&p->m_nodes[node]));
            }
        }
    }

    return parents;
}

void LibraryTreeModel::addTracks(const TrackList& tracks)
{
    TrackList tracksToAdd;
    std::ranges::copy_if(tracks, std::back_inserter(tracksToAdd),
                         [this](const Track& track) { return !p->m_trackParents.contains(track.id()); });

    if(tracksToAdd.empty()) {
        return;
    }

    p->m_addingTracks = true;
    p->m_populatorThread.start();

    QMetaObject::invokeMethod(&p->m_populator, [this, tracksToAdd] {
        p->m_populator.run(p->m_grouping, tracksToAdd,
                           p->m_settings->value<Settings::Core::UseVariousForCompilations>());
    });
}

void LibraryTreeModel::updateTracks(const TrackList& tracks)
{
    TrackList tracksToUpdate;
    std::ranges::copy_if(tracks, std::back_inserter(tracksToUpdate),
                         [this](const Track& track) { return p->m_trackParents.contains(track.id()); });

    if(tracksToUpdate.empty()) {
        emit modelUpdated();
        addTracks(tracks);
        return;
    }

    p->m_tracksPendingRemoval = tracksToUpdate;
    p->m_addingTracks         = false;
    p->m_populatorThread.start();

    QMetaObject::invokeMethod(&p->m_populator, [this, tracksToUpdate] {
        p->m_populator.run(p->m_grouping, tracksToUpdate,
                           p->m_settings->value<Settings::Core::UseVariousForCompilations>());
    });

    addTracks(tracks);
}

void LibraryTreeModel::refreshTracks(const TrackList& tracks)
{
    for(const Track& track : tracks) {
        if(!p->m_trackParents.contains(track.id())) {
            continue;
        }

        const auto parents = p->m_trackParents.at(track.id());
        for(const auto& parent : parents) {
            if(p->m_nodes.contains(parent)) {
                p->m_nodes.at(parent).replaceTrack(track);
            }
        }
    }
}

void LibraryTreeModel::removeTracks(const TrackList& tracks)
{
    p->removeTracks(tracks);
}

void LibraryTreeModel::changeGrouping(const LibraryTreeGrouping& grouping)
{
    p->m_grouping = grouping.script;
}

void LibraryTreeModel::reset(const TrackList& tracks)
{
    if(p->m_populatorThread.isRunning()) {
        p->m_populator.stopThread();
    }
    else {
        p->m_populatorThread.start();
    }

    if(tracks.empty()) {
        beginResetModel();
        p->beginReset();
        endResetModel();
        emit modelLoaded();
        return;
    }

    p->m_resetting = true;

    QMetaObject::invokeMethod(&p->m_populator, [this, tracks] {
        p->m_populator.run(p->m_grouping, tracks, p->m_settings->value<Settings::Core::UseVariousForCompilations>());
    });
}

QModelIndex LibraryTreeModel::indexForKey(const Md5Hash& key)
{
    while(!p->m_nodes.contains(key) && canFetchMore({})) {
        fetchMore({});
    }

    if(p->m_nodes.contains(key)) {
        return indexOfItem(&p->m_nodes.at(key));
    }

    return {};
}

void LibraryTreeModel::invalidateData()
{
    beginResetModel();
    endResetModel();
}
} // namespace Fooyin

#include "moc_librarytreemodel.cpp"
