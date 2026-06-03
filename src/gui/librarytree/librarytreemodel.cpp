/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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
#include "librarytreescriptenvironment.h"

#include <core/coresettings.h>
#include <core/library/tracksort.h>
#include <core/scripting/scriptparser.h>
#include <gui/coverprovider.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/guiutils.h>
#include <gui/iconloader.h>
#include <gui/scripting/richtextutils.h>
#include <gui/scripting/scriptformatter.h>
#include <gui/trackmimedata.h>
#include <utils/datastream.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QApplication>
#include <QColor>
#include <QIODevice>
#include <QMimeData>
#include <QPalette>
#include <QSize>
#include <QThread>

#include <ranges>
#include <set>
#include <stack>

using namespace Qt::StringLiterals;

namespace {
QFont libraryTreeFont()
{
    return QApplication::font("Fooyin::LibraryTreeView");
}

using NodeParentMap = std::unordered_map<Fooyin::Md5Hash, Fooyin::Md5Hash>;

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

bool lessThanItems(const Fooyin::LibraryTreeItem* leftItem, const Fooyin::LibraryTreeItem* rightItem,
                   Qt::SortOrder sortOrder, const Fooyin::StringCollator& collator)
{
    if(!leftItem || !rightItem) {
        return false;
    }

    if(leftItem->level() == -1) {
        return sortOrder == Qt::AscendingOrder;
    }

    if(rightItem->level() == -1) {
        return sortOrder != Qt::AscendingOrder;
    }

    const QString& leftTitle  = leftItem->sortTitle().isEmpty() ? leftItem->title() : leftItem->sortTitle();
    const QString& rightTitle = rightItem->sortTitle().isEmpty() ? rightItem->title() : rightItem->sortTitle();
    const auto cmp            = collator.compare(leftTitle, rightTitle);

    if(cmp == 0) {
        return false;
    }

    return cmp < 0;
}

void appendItemTracks(const Fooyin::LibraryTreeItem* item, Fooyin::TrackList& tracks)
{
    const auto& itemTracks = item->tracks();
    tracks.insert(tracks.end(), itemTracks.cbegin(), itemTracks.cend());
}

QString tooltipText(const Fooyin::LibraryTreeItem* item)
{
    if(!item) {
        return {};
    }

    QString leftText        = item->richTitle().joinedText();
    const QString rightText = item->rightRichTitle().joinedText();

    if(leftText.isEmpty()) {
        return rightText.isEmpty() ? item->title() : rightText;
    }
    if(rightText.isEmpty()) {
        return leftText;
    }
    return leftText + u"\n"_s + rightText;
}

QByteArray saveTrackIds(const Fooyin::TrackList& tracks)
{
    QByteArray result;
    QDataStream stream{&result, QIODevice::WriteOnly};

    Fooyin::TrackIds trackIds;
    trackIds.reserve(tracks.size());

    for(const Fooyin::Track& track : tracks) {
        trackIds.push_back(track.id());
    }

    stream << trackIds;

    return result;
}

QMimeData* createTrackMimeData(Fooyin::TrackList tracks)
{
    QMimeData* mimeData{nullptr};

    const QByteArray trackIds = saveTrackIds(tracks);

    if(!tracks.empty()) {
        mimeData = new Fooyin::TrackMimeData(std::move(tracks));
    }
    else {
        mimeData = new QMimeData();
    }

    mimeData->setData(QString::fromLatin1(Fooyin::Constants::Mime::TrackIds), trackIds);
    return mimeData;
}

} // namespace

namespace Fooyin {
LibraryTreeSortModel::LibraryTreeSortModel(QObject* parent)
    : QSortFilterProxyModel{parent}
{ }

void LibraryTreeSortModel::appendTracksForIndexes(const QModelIndexList& indexes, TrackList& tracks) const
{
    if(!qobject_cast<LibraryTreeModel*>(sourceModel())) {
        return;
    }
    const auto appendTracksForItem
        = [this, &tracks](auto&& self, const LibraryTreeSortModel* sortModel, LibraryTreeItem* item) -> void {
        if(!sortModel || !item) {
            return;
        }

        const auto* model = qobject_cast<const LibraryTreeModel*>(sortModel->sourceModel());
        auto children     = model->childItemsForTraversal(item->key());
        if(children.empty()) {
            appendItemTracks(item, tracks);
            return;
        }

        std::ranges::sort(children, [this](const LibraryTreeItem* left, const LibraryTreeItem* right) {
            return lessThanItems(left, right, sortOrder(), m_collator);
        });

        for(auto* child : children) {
            self(self, sortModel, child);
        }
    };

    for(const QModelIndex& index : indexes) {
        const QModelIndex sourceIndex = mapToSource(index);
        if(!sourceIndex.isValid()) {
            continue;
        }
        appendTracksForItem(appendTracksForItem, this, treeItem(sourceIndex));
    }
}

void LibraryTreeSortModel::setDragTracks(TrackList tracks)
{
    m_dragTracks = std::move(tracks);
}

QMimeData* LibraryTreeSortModel::mimeData(const QModelIndexList& indexes) const
{
    if(!m_dragTracks.empty()) {
        return createTrackMimeData(std::exchange(m_dragTracks, {}));
    }

    TrackList tracks;
    appendTracksForIndexes(indexes, tracks);
    return createTrackMimeData(tracks);
}

bool LibraryTreeSortModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const auto* leftItem  = treeItem(left);
    const auto* rightItem = treeItem(right);

    if(!leftItem || !rightItem) {
        return QSortFilterProxyModel::lessThan(left, right);
    }

    return lessThanItems(leftItem, rightItem, sortOrder(), m_collator);
}

class LibraryTreeModelPrivate
{
public:
    explicit LibraryTreeModelPrivate(LibraryTreeModel* self, LibraryManager* libraryManager,
                                     std::shared_ptr<AudioLoader> audioLoader, CoverRepository* coverRepository,
                                     SettingsManager* settings);

    void updateSummary();
    void ensureSummaryNodeVisible();
    void removeSummaryNode();
    void queueRichTitleUpdate(ItemKeyMap items);
    void startRichTitleUpdate(ItemKeyMap items);
    [[nodiscard]] int childCountForScript(const LibraryTreeItem& item) const;
    [[nodiscard]] int summaryChildCountForScript();

    void removeTracks(const TrackList& tracks);
    void mergeTrackParents(const TrackIdNodeMap& parents);

    void batchFinished(const PendingTreeDataPtr& data);

    void appendTracksForItem(LibraryTreeItem* item, TrackList& tracks);
    [[nodiscard]] std::vector<LibraryTreeItem*> childItemsForTraversal(LibraryTreeItem* parentItem);

    void addPendingNode(const Md5Hash& parentKey, const Md5Hash& childKey);
    void removePendingNode(const Md5Hash& childKey);
    bool ensureItemVisible(const Md5Hash& key);
    void ensureChildrenAvailable(LibraryTreeItem* parentItem) const;
    void updatePendingNodes(const PendingTreeData& data);
    void applyUpdatedItems(const ItemKeyMap& items);

    void populateModel(PendingTreeData& data);
    void beginReset();

    LibraryTreeModel* m_self;
    std::shared_ptr<AudioLoader> m_audioLoader;
    SettingsManager* m_settings;

    CoverProvider m_coverProvider;
    LibraryTreeGrouping m_grouping;
    bool m_loaded{false};
    bool m_resetting{false};

    QThread m_populatorThread;
    LibraryTreePopulator m_populator;

    LibraryTreeItem m_summaryNode;
    bool m_showSummaryNode{true};
    QString m_summaryNodeTitle;
    NodeKeyMap m_pendingNodes;
    ItemKeyMap m_nodes;
    NodeParentMap m_nodeParents;
    TrackIdNodeMap m_trackParents;
    std::unordered_set<Md5Hash> m_addedNodes;
    bool m_addingTracks{false};

    TrackList m_tracksPendingRemoval;
    ItemKeyMap m_pendingRichTitleUpdates;

    Player::PlayState m_playingState;
    QString m_parentNode;
    QString m_playingPath;
    int m_rowHeight{0};
    QColor m_playingColour{QApplication::palette().highlight().color()};
    ThumbnailSize m_iconSize;
};

LibraryTreeModelPrivate::LibraryTreeModelPrivate(LibraryTreeModel* self, LibraryManager* libraryManager,
                                                 std::shared_ptr<AudioLoader> audioLoader,
                                                 CoverRepository* coverRepository, SettingsManager* settings)
    : m_self{self}
    , m_audioLoader{std::move(audioLoader)}
    , m_settings{settings}
    , m_coverProvider{coverRepository}
    , m_populator{libraryManager, settings}
    , m_iconSize{
          CoverProvider::findThumbnailSize(m_settings->value<Settings::Gui::Internal::LibTreeIconSize>().toSize())}
{
    m_playingColour.setAlpha(90);

    m_populator.moveToThread(&m_populatorThread);
}

void LibraryTreeModelPrivate::updateSummary()
{
    if(!m_showSummaryNode) {
        return;
    }

    QString title{m_summaryNodeTitle};
    if(title.isEmpty()) {
        title = defaultLibraryTreeSummaryTitle();
    }

    LibraryTreeScriptEnvironment environment;
    environment.setNodeVariablePolicy(LibraryTreeNodePolicy::Value);
    environment.setNodeCounts(static_cast<int>(m_trackParents.size()), summaryChildCountForScript());

    ScriptParser parser;
    parser.addProvider(libraryTreeNodeVariableProvider());
    title = parser.evaluate(title, ScriptContext{.environment = &environment},
                            {.whitespaceMode = ScriptWhitespaceMode::Preserve});

    ScriptFormatter formatter;
    const RichText richTitle = trimRichText(formatter.evaluate(title));

    m_summaryNode.setTitle(richTitle.joinedText());
    m_summaryNode.setTitleSource(m_summaryNodeTitle);
    m_summaryNode.setRichTitle(richTitle);
}

void LibraryTreeModelPrivate::ensureSummaryNodeVisible()
{
    if(m_self->rootItem()->hasChild(&m_summaryNode)) {
        return;
    }

    m_self->beginInsertRows({}, 0, 0);
    m_self->rootItem()->insertChild(0, &m_summaryNode);
    m_self->endInsertRows();
}

void LibraryTreeModelPrivate::removeSummaryNode()
{
    if(!m_self->rootItem()->hasChild(&m_summaryNode)) {
        return;
    }

    const int row = m_summaryNode.row();
    m_self->beginRemoveRows({}, row, row);
    m_self->rootItem()->removeChild(row);
    m_self->endRemoveRows();
}

int LibraryTreeModelPrivate::childCountForScript(const LibraryTreeItem& item) const
{
    int childCount = item.childCount();
    if(const auto pendingIt = m_pendingNodes.find(item.key()); pendingIt != m_pendingNodes.end()) {
        childCount += static_cast<int>(pendingIt->second.size());
    }
    return childCount;
}

int LibraryTreeModelPrivate::summaryChildCountForScript()
{
    auto* root     = m_self->rootItem();
    int childCount = root->childCount();
    if(root->hasChild(&m_summaryNode)) {
        --childCount;
    }
    if(const auto pendingIt = m_pendingNodes.find(root->key()); pendingIt != m_pendingNodes.end()) {
        childCount += static_cast<int>(pendingIt->second.size());
    }
    return childCount;
}

void LibraryTreeModelPrivate::queueRichTitleUpdate(ItemKeyMap items)
{
    if(items.empty()) {
        return;
    }

    for(auto& [key, item] : items) {
        if(const auto nodeIt = m_nodes.find(key); nodeIt != m_nodes.end()) {
            item.setScriptChildCount(childCountForScript(nodeIt->second));
        }
    }

    if(m_populatorThread.isRunning() && m_populator.state() == Worker::Running) {
        for(auto& [key, item] : items) {
            m_pendingRichTitleUpdates.insert_or_assign(key, std::move(item));
        }
        return;
    }

    startRichTitleUpdate(std::move(items));
}

void LibraryTreeModelPrivate::startRichTitleUpdate(ItemKeyMap items)
{
    if(!m_populatorThread.isRunning()) {
        m_populatorThread.start();
    }

    const QFont font      = libraryTreeFont();
    const bool useVarious = m_settings->value<Settings::Core::UseVariousForCompilations>();
    QMetaObject::invokeMethod(&m_populator, [this, font, items = std::move(items), useVarious]() mutable {
        m_populator.setFont(font);
        m_populator.updateItems(std::move(items), useVarious);
    });
}

void LibraryTreeModelPrivate::removeTracks(const TrackList& tracks)
{
    std::set<LibraryTreeItem*, cmpItems> items;
    std::set<LibraryTreeItem*> pendingItems;
    ItemKeyMap itemsToUpdate;

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
            removePendingNode(item->key());
            m_nodes.erase(item->key());
        }
        else {
            itemsToUpdate.insert_or_assign(item->key(), m_nodes.at(item->key()));
        }
    }

    for(auto* item : items) {
        if(item->trackCount() == 0) {
            auto* parent  = item->parent();
            const int row = item->row();

            m_self->beginRemoveRows(m_self->indexOfItem(parent), row, row);
            parent->removeChild(row);
            parent->resetChildren();
            m_self->endRemoveRows();

            removePendingNode(item->key());
            m_nodes.erase(item->key());
        }
        else {
            itemsToUpdate.insert_or_assign(item->key(), *item);
        }
    }

    queueRichTitleUpdate(std::move(itemsToUpdate));
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

void LibraryTreeModelPrivate::batchFinished(const PendingTreeDataPtr& data)
{
    if(m_resetting) {
        m_self->beginResetModel();
        beginReset();
    }

    if(!m_tracksPendingRemoval.empty()) {
        removeTracks(m_tracksPendingRemoval);
        m_tracksPendingRemoval.clear();
    }

    populateModel(*data);

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

void LibraryTreeModelPrivate::appendTracksForItem(LibraryTreeItem* item, TrackList& tracks)
{
    if(!item) {
        return;
    }

    const auto children = childItemsForTraversal(item);
    if(children.empty()) {
        appendItemTracks(item, tracks);
        return;
    }

    for(auto* child : children) {
        appendTracksForItem(child, tracks);
    }
}

std::vector<LibraryTreeItem*> LibraryTreeModelPrivate::childItemsForTraversal(LibraryTreeItem* parentItem)
{
    if(!parentItem) {
        return {};
    }

    const auto childCount = parentItem->childCount();

    std::vector<LibraryTreeItem*> children;
    children.reserve(static_cast<size_t>(childCount)
                     + (m_pendingNodes.contains(parentItem->key()) ? m_pendingNodes.at(parentItem->key()).size() : 0));

    for(int row{0}; row < childCount; ++row) {
        if(auto* child = parentItem->child(row)) {
            children.push_back(child);
        }
    }

    if(const auto pendingIt = m_pendingNodes.find(parentItem->key()); pendingIt != m_pendingNodes.end()) {
        for(const Md5Hash& childKey : pendingIt->second) {
            if(const auto nodeIt = m_nodes.find(childKey); nodeIt != m_nodes.end()) {
                children.push_back(&nodeIt->second);
            }
        }
    }

    return children;
}

void LibraryTreeModelPrivate::addPendingNode(const Md5Hash& parentKey, const Md5Hash& childKey)
{
    if(m_addedNodes.contains(childKey)) {
        return;
    }

    auto [it, _] = m_pendingNodes.try_emplace(parentKey);
    it->second.push_back(childKey);
    m_addedNodes.insert(childKey);
}

void LibraryTreeModelPrivate::removePendingNode(const Md5Hash& childKey)
{
    const auto parentIt = m_nodeParents.find(childKey);
    if(parentIt != m_nodeParents.end()) {
        if(auto pendingIt = m_pendingNodes.find(parentIt->second); pendingIt != m_pendingNodes.end()) {
            auto& rows = pendingIt->second;
            std::erase(rows, childKey);
            if(rows.empty()) {
                m_pendingNodes.erase(pendingIt);
            }
        }
    }

    m_addedNodes.erase(childKey);
    m_nodeParents.erase(childKey);
    m_pendingNodes.erase(childKey);
}

bool LibraryTreeModelPrivate::ensureItemVisible(const Md5Hash& key)
{
    const auto itemIt = m_nodes.find(key);
    if(itemIt == m_nodes.end()) {
        return false;
    }

    if(!itemIt->second.pending()) {
        return true;
    }

    const auto parentIt = m_nodeParents.find(key);
    if(parentIt == m_nodeParents.end()) {
        return false;
    }

    QModelIndex parentIndex;
    if(const Md5Hash& parentKey = parentIt->second; !parentKey.isEmpty()) {
        if(!ensureItemVisible(parentKey)) {
            return false;
        }
        parentIndex = m_self->indexOfItem(&m_nodes.at(parentKey));
        if(!parentIndex.isValid()) {
            return false;
        }
        ensureChildrenAvailable(&m_nodes.at(parentKey));
    }

    if(m_self->canFetchMore(parentIndex)) {
        m_self->fetchMore(parentIndex);
    }

    const auto visibleIt = m_nodes.find(key);
    return visibleIt != m_nodes.end() && !visibleIt->second.pending();
}

void LibraryTreeModelPrivate::ensureChildrenAvailable(LibraryTreeItem* parentItem) const
{
    if(!parentItem) {
        return;
    }

    if(const QModelIndex parentIndex = m_self->indexOfItem(parentItem); m_self->canFetchMore(parentIndex)) {
        m_self->fetchMore(parentIndex);
    }
}

void LibraryTreeModelPrivate::updatePendingNodes(const PendingTreeData& data)
{
    std::set<QModelIndex> nodesToCheck;

    for(const auto& [parentKey, rows] : data.nodes) {
        LibraryTreeItem* parent = parentKey.isEmpty() ? m_self->rootItem() : nullptr;
        if(!parent) {
            if(const auto it = m_nodes.find(parentKey); it != m_nodes.end()) {
                parent = &it->second;
            }
        }

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
                addPendingNode(parentKey, row);
            }
        }
    }

    for(const QModelIndex& index : nodesToCheck) {
        if(m_self->canFetchMore(index)) {
            m_self->fetchMore(index);
        }
    }
}

void LibraryTreeModelPrivate::applyUpdatedItems(const ItemKeyMap& items)
{
    QModelIndexList changedIndexes;

    for(const auto& [key, item] : items) {
        auto nodeIt = m_nodes.find(key);
        if(nodeIt == m_nodes.end()) {
            continue;
        }

        auto& node = nodeIt->second;
        node.setScriptChildCount(item.scriptChildCount());
        if(node.richTitle() == item.richTitle() && node.rightRichTitle() == item.rightRichTitle()) {
            continue;
        }

        node.setRichTitles(item.richTitle(), item.rightRichTitle());
        if(!node.pending()) {
            changedIndexes.emplace_back(m_self->indexOfItem(&node));
        }
    }

    for(const QModelIndex& index : changedIndexes) {
        Q_EMIT m_self->dataChanged(index, index,
                                   {Qt::DisplayRole, Qt::ToolTipRole, Qt::SizeHintRole, LibraryTreeItem::RichTitle,
                                    LibraryTreeItem::RightRichTitle});
    }
}

void LibraryTreeModelPrivate::populateModel(PendingTreeData& data)
{
    QModelIndexList changedIndexes;
    ItemKeyMap itemsToUpdate;
    std::vector<Md5Hash> childCountItemsToUpdate;

    for(auto& [key, item] : data.items) {
        const bool itemUsesChildCount = usesLibraryTreeChildCount(item.titleSource());

        if(m_nodes.contains(key)) {
            auto& node = m_nodes.at(key);

            node.setTitleSource(item.titleSource());

            if(node.sortTitle() != item.sortTitle()) {
                node.setSortTitle(item.sortTitle());
                if(!node.pending()) {
                    changedIndexes.emplace_back(m_self->indexOfItem(&node));
                }
            }

            if(!itemUsesChildCount
               && (node.richTitle() != item.richTitle() || node.rightRichTitle() != item.rightRichTitle())) {
                node.setRichTitles(item.richTitle(), item.rightRichTitle());
                if(!node.pending()) {
                    changedIndexes.emplace_back(m_self->indexOfItem(&node));
                }
            }

            node.addTracks(item.tracks());
            node.setScriptChildCount(item.scriptChildCount());
            itemsToUpdate.insert_or_assign(key, node);
        }
        else {
            m_nodes.emplace(key, std::move(item));
            if(itemUsesChildCount) {
                childCountItemsToUpdate.push_back(key);
            }
        }
    }
    mergeTrackParents(data.trackParents);

    for(const auto& [parentKey, rows] : data.nodes) {
        for(const auto& row : rows) {
            m_nodeParents.insert_or_assign(row, parentKey);
        }
    }

    const QModelIndex allIndex = m_self->indexOfItem(&m_summaryNode);
    Q_EMIT m_self->dataChanged(allIndex, allIndex, {Qt::DisplayRole});

    for(const QModelIndex& index : changedIndexes) {
        Q_EMIT m_self->dataChanged(index, index,
                                   {Qt::DisplayRole, Qt::ToolTipRole, Qt::SizeHintRole, LibraryTreeItem::RichTitle,
                                    LibraryTreeItem::RightRichTitle});
    }

    if(m_resetting) {
        for(const auto& [parentKey, rows] : data.nodes) {
            auto* parent = parentKey.isEmpty() ? m_self->rootItem() : &m_nodes.at(parentKey);

            for(const auto& row : rows) {
                LibraryTreeItem* child = &m_nodes.at(row);
                if(parentKey.isEmpty()) {
                    parent->appendChild(child);
                    child->setPending(false);
                }
                else {
                    addPendingNode(parentKey, row);
                }
            }
        }
        m_self->rootItem()->resetChildren();
    }
    else {
        updatePendingNodes(data);
    }

    for(const auto& key : childCountItemsToUpdate) {
        if(const auto nodeIt = m_nodes.find(key); nodeIt != m_nodes.end()) {
            itemsToUpdate.insert_or_assign(key, nodeIt->second);
        }
    }

    applyUpdatedItems(data.updatedItems);
    queueRichTitleUpdate(std::move(itemsToUpdate));
    updateSummary();
}

void LibraryTreeModelPrivate::beginReset()
{
    m_self->resetRoot();
    m_nodes.clear();
    m_pendingNodes.clear();
    m_nodeParents.clear();
    m_trackParents.clear();
    m_addedNodes.clear();

    m_summaryNode = LibraryTreeItem{LibraryTreeModel::tr("All Music"), m_self->rootItem(), -1};
    if(m_showSummaryNode) {
        m_self->rootItem()->appendChild(&m_summaryNode);
    }
    updateSummary();
}

LibraryTreeModel::LibraryTreeModel(LibraryManager* libraryManager, const std::shared_ptr<AudioLoader>& audioLoader,
                                   CoverRepository* coverRepository, SettingsManager* settings, QObject* parent)
    : TreeModel{parent}
    , p{std::make_unique<LibraryTreeModelPrivate>(this, libraryManager, audioLoader, coverRepository, settings)}
{
    qRegisterMetaType<PendingTreeDataPtr>("Fooyin::PendingTreeDataPtr");

    QObject::connect(&p->m_populator, &LibraryTreePopulator::populated, this,
                     [this](const PendingTreeDataPtr& data) { p->batchFinished(data); });

    QObject::connect(&p->m_populator, &Worker::finished, this, [this]() {
        p->updateSummary();

        if(!p->m_pendingRichTitleUpdates.empty()) {
            ItemKeyMap items = std::move(p->m_pendingRichTitleUpdates);
            p->m_pendingRichTitleUpdates.clear();
            p->startRichTitleUpdate(std::move(items));
            return;
        }

        p->m_populator.stopThread();
        p->m_populatorThread.quit();
        if(!p->m_loaded) {
            p->m_loaded = true;
            Q_EMIT modelLoaded();
        }
    });

    QObject::connect(&p->m_coverProvider, &CoverProvider::coverAdded, this, [this](const Track& track) {
        if(p->m_trackParents.contains(track.id())) {
            const auto items = p->m_trackParents.at(track.id());
            for(const auto& itemHash : items) {
                if(p->m_nodes.contains(itemHash)) {
                    const QModelIndex index = indexOfItem(&p->m_nodes.at(itemHash));
                    Q_EMIT dataChanged(index, index, {Qt::DecorationRole});
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

    p->queueRichTitleUpdate(p->m_nodes);
    invalidateData();
}

void LibraryTreeModel::setRowHeight(int height)
{
    p->m_rowHeight = height;
    Q_EMIT dataUpdated({}, {});
}

void LibraryTreeModel::setSummaryNodeConfig(bool show, const QString& title)
{
    const bool visibilityChanged = p->m_showSummaryNode != show;
    p->m_showSummaryNode         = show;
    p->m_summaryNodeTitle        = title;

    if(show) {
        if(visibilityChanged) {
            p->ensureSummaryNodeVisible();
        }
        p->updateSummary();
        const QModelIndex index = indexOfItem(&p->m_summaryNode);
        if(index.isValid()) {
            Q_EMIT dataChanged(index, index,
                               {Qt::DisplayRole, Qt::ToolTipRole, Qt::SizeHintRole, LibraryTreeItem::RichTitle,
                                LibraryTreeItem::RightRichTitle});
        }
    }
    else if(visibilityChanged) {
        p->removeSummaryNode();
    }
}

void LibraryTreeModel::setPlayState(Player::PlayState state)
{
    p->m_playingState = state;
    Q_EMIT dataUpdated({}, {}, {Qt::DecorationRole, Qt::BackgroundRole});
}

void LibraryTreeModel::setPlayingPath(const QString& parentNode, const QString& path)
{
    p->m_parentNode  = parentNode;
    p->m_playingPath = path;
    Q_EMIT dataUpdated({}, {}, {Qt::DecorationRole, Qt::BackgroundRole});
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
        return Qt::AlignCenter;
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
        const QString parentTitle
            = item->parent() && !item->parent()->title().isEmpty() ? item->parent()->title() : u"?"_s;
        const bool isPlayingTrack = item->childCount() == 0 && item->trackCount() == 1
                                 && item->tracks().front().uniqueFilepath() == p->m_playingPath
                                 && parentTitle == p->m_parentNode;
        if(isPlayingTrack) {
            if(role == Qt::BackgroundRole) {
                return p->m_playingColour;
            }
            if(role == Qt::DecorationRole) {
                switch(p->m_playingState) {
                    case Player::PlayState::Playing:
                        return Gui::pixmapFromTheme(Constants::Icons::Play);
                    case Player::PlayState::Paused:
                        return Gui::pixmapFromTheme(Constants::Icons::Pause);
                    case Player::PlayState::Stopped:
                        break;
                }
            }
        }
    }

    switch(role) {
        case Qt::DisplayRole: {
            const QString& name = item->title();
            return !name.isEmpty() ? name : u"?"_s;
        }
        case Qt::ToolTipRole:
            return tooltipText(item);
        case LibraryTreeItem::Title:
            return item->title();
        case LibraryTreeItem::RichTitle:
            return item->richTitle();
        case LibraryTreeItem::RightRichTitle:
            return item->rightRichTitle();
        case LibraryTreeItem::Level:
            return item->level();
        case LibraryTreeItem::Key:
            return QVariant::fromValue(item->key());
        case LibraryTreeItem::Tracks:
            return QVariant::fromValue(item->tracks());
        case LibraryTreeItem::TrackCount:
            return item->trackCount();
        case Qt::SizeHintRole: {
            if(p->m_rowHeight > 0) {
                return QSize{0, p->m_rowHeight};
            }
            break;
        }
        case LibraryTreeItem::Role::DecorationPosition: {
            if(item->coverType()) {
                return item->coverPosition();
            }
            break;
        }
        case Qt::DecorationRole: {
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

    const auto* item = itemForIndex(parent);
    return item->childCount() > 0 || p->m_pendingNodes.contains(item->key());
}

void LibraryTreeModel::fetchMore(const QModelIndex& parent)
{
    auto* parentItem        = itemForIndex(parent);
    const Md5Hash parentKey = parentItem->key();
    const auto pendingIt    = p->m_pendingNodes.find(parentKey);
    if(pendingIt == p->m_pendingNodes.end() || pendingIt->second.empty()) {
        return;
    }

    auto& rows = pendingIt->second;

    const int row           = parentItem->childCount();
    const int totalRows     = static_cast<int>(rows.size());
    const int rowCount      = totalRows;
    const auto rowsToInsert = std::ranges::views::take(rows, rowCount);

    beginInsertRows(parent, row, row + rowCount - 1);
    for(const auto& pendingRow : rowsToInsert) {
        LibraryTreeItem* child = &p->m_nodes.at(pendingRow);
        parentItem->appendChild(child);
        child->setPending(false);
        p->m_addedNodes.erase(pendingRow);
    }
    endInsertRows();

    rows.erase(rows.begin(), rows.begin() + rowCount);

    if(rows.empty()) {
        p->m_pendingNodes.erase(parentKey);
    }
}

bool LibraryTreeModel::canFetchMore(const QModelIndex& parent) const
{
    auto* item = itemForIndex(parent);
    if(const auto pendingIt = p->m_pendingNodes.find(item->key()); pendingIt != p->m_pendingNodes.end()) {
        return !pendingIt->second.empty();
    }

    return false;
}

void LibraryTreeModel::ensureChildrenAvailable(const QModelIndex& parent)
{
    if(!parent.isValid()) {
        while(canFetchMore({})) {
            fetchMore({});
        }
        return;
    }

    p->ensureChildrenAvailable(itemForIndex(parent));
}

std::vector<LibraryTreeItem*> LibraryTreeModel::childItemsForTraversal(const Md5Hash& key) const
{
    if(key.isEmpty() || !p->m_nodes.contains(key)) {
        return {};
    }

    return p->childItemsForTraversal(&p->m_nodes.at(key));
}

void LibraryTreeModel::appendTracksForIndexes(const QModelIndexList& indexes, TrackList& tracks) const
{
    for(const QModelIndex& index : indexes) {
        p->appendTracksForItem(itemForIndex(index), tracks);
    }
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
    TrackList tracks;
    int totalTrackCount{0};

    for(const QModelIndex& index : indexes) {
        totalTrackCount += index.data(LibraryTreeItem::TrackCount).toInt();
    }

    if(totalTrackCount > 0) {
        tracks.reserve(static_cast<size_t>(totalTrackCount));
    }

    appendTracksForIndexes(indexes, tracks);
    return mimeDataForTracks(tracks);
}

QMimeData* LibraryTreeModel::mimeDataForTracks(const TrackList& tracks) const
{
    return createTrackMimeData(Gui::sortTracksForLibraryViewerPlaylist(p->m_settings, tracks));
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

QModelIndexList LibraryTreeModel::indexesForKeys(const std::vector<Md5Hash>& keys)
{
    if(keys.empty()) {
        return {};
    }

    QModelIndexList indexes;
    indexes.reserve(static_cast<qsizetype>(keys.size()));

    for(const auto& key : keys) {
        p->ensureItemVisible(key);

        const auto it = p->m_nodes.find(key);
        if(it == p->m_nodes.end() || it->second.pending()) {
            continue;
        }

        const QModelIndex index = indexOfItem(&it->second);
        if(index.isValid()) {
            indexes.append(index);
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

    QMetaObject::invokeMethod(&p->m_populator, [this, tracksToAdd = std::move(tracksToAdd)] {
        p->m_populator.setFont(libraryTreeFont());
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
        Q_EMIT modelUpdated();
        addTracks(tracks);
        return;
    }

    p->m_tracksPendingRemoval = tracksToUpdate;
    p->m_addingTracks         = false;
    p->m_populatorThread.start();

    QMetaObject::invokeMethod(&p->m_populator, [this, tracksToUpdate = std::move(tracksToUpdate)] {
        p->m_populator.setFont(libraryTreeFont());
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
    p->m_grouping = grouping;
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
        Q_EMIT modelLoaded();
        return;
    }

    p->m_resetting = true;
    p->m_trackParents.clear();
    p->m_tracksPendingRemoval.clear();

    QMetaObject::invokeMethod(&p->m_populator, [this, tracks] {
        p->m_populator.setFont(libraryTreeFont());
        p->m_populator.run(p->m_grouping, tracks, p->m_settings->value<Settings::Core::UseVariousForCompilations>());
    });
}

QModelIndex LibraryTreeModel::indexForKey(const Md5Hash& key)
{
    if(p->ensureItemVisible(key)) {
        return indexOfItem(&p->m_nodes.at(key));
    }

    return {};
}
} // namespace Fooyin

#include "moc_librarytreemodel.cpp"
