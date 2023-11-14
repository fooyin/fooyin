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

#include "librarytreemodel.h"

#include "librarytreeappearance.h"
#include "librarytreepopulator.h"

#include <gui/guiconstants.h>

#include <QColor>
#include <QFont>
#include <QIODevice>
#include <QMimeData>
#include <QSize>
#include <QThread>

#include <queue>
#include <ranges>
#include <set>

using namespace Qt::Literals::StringLiterals;

namespace {
using Fy::Gui::Widgets::LibraryTreeItem;

bool cmpItemsReverse(LibraryTreeItem* pItem1, LibraryTreeItem* pItem2)
{
    LibraryTreeItem* item1{pItem1};
    LibraryTreeItem* item2{pItem2};

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
    bool operator()(LibraryTreeItem* pItem1, LibraryTreeItem* pItem2) const
    {
        return cmpItemsReverse(pItem1, pItem2);
    }
};

QByteArray saveTracks(const QModelIndexList& indexes)
{
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);

    Fy::Core::TrackList tracks;
    tracks.reserve(indexes.size());

    for(const QModelIndex& index : indexes) {
        std::ranges::copy(index.data(Fy::Gui::Widgets::LibraryTreeRole::Tracks).value<Fy::Core::TrackList>(),
                          std::back_inserter(tracks));
    }

    stream << tracks;

    return result;
}
} // namespace

namespace Fy::Gui::Widgets {

struct LibraryTreeModel::Private
{
    LibraryTreeModel* self;

    QString grouping;

    bool resetting{false};

    QThread populatorThread;
    LibraryTreePopulator populator;

    LibraryTreeItem allNode;
    NodeKeyMap pendingNodes;
    ItemKeyMap nodes;
    TrackIdNodeMap trackParents;
    std::unordered_set<QString> addedNodes;

    int rowHeight{0};
    QFont font;
    QColor colour;

    explicit Private(LibraryTreeModel* self)
        : self{self}
    {
        populator.moveToThread(&populatorThread);
    }

    void sortTree() const
    {
        self->rootItem()->sortChildren();
        self->rootItem()->resetChildren();
    }

    int totalTrackCount() const
    {
        std::set<int> ids;
        std::queue<LibraryTreeItem*> trackNodes;
        trackNodes.emplace(self->rootItem());

        while(!trackNodes.empty()) {
            LibraryTreeItem* node = trackNodes.front();
            trackNodes.pop();
            const auto nodeTracks = node->tracks();
            for(const Core::Track& track : nodeTracks) {
                ids.emplace(track.id());
            }
            const auto children = node->children();
            for(LibraryTreeItem* child : children) {
                trackNodes.emplace(child);
            }
        }
        return static_cast<int>(ids.size());
    }

    void updateAllNode()
    {
        allNode.setTitle(QString{u"All Music (%1)"_s}.arg(totalTrackCount()));
    }

    void batchFinished(PendingTreeData data)
    {
        if(resetting) {
            self->beginResetModel();
            beginReset();
        }

        populateModel(data);

        if(resetting) {
            self->endResetModel();
        }
        resetting = false;

        while(self->canFetchMore({})) {
            self->fetchMore({});
        }
    }

    void updatePendingNodes(const PendingTreeData& data)
    {
        std::set<QModelIndex> nodesToCheck;

        for(const auto& [parentKey, rows] : data.nodes) {
            auto* parent = parentKey == "0"_L1       ? self->rootItem()
                         : nodes.contains(parentKey) ? &nodes.at(parentKey)
                                                     : nullptr;
            if(!parent) {
                continue;
            }

            for(const QString& row : rows) {
                auto* node = nodes.contains(row) ? &nodes.at(row) : nullptr;

                if(node && node->pending() && !addedNodes.contains(row)) {
                    if(!parent->pending() && !pendingNodes.contains(parentKey) && parent->parent()) {
                        // Parent is expanded/visible
                        nodesToCheck.emplace(self->indexOfItem(parent));
                    }
                    pendingNodes[parentKey].push_back(row);
                    addedNodes.insert(row);
                }
            }
        }

        for(const QModelIndex& index : nodesToCheck) {
            if(self->canFetchMore(index)) {
                self->fetchMore(index);
            }
        }
    }

    void populateModel(PendingTreeData& data)
    {
        for(const auto& [key, item] : data.items) {
            if(nodes.contains(key)) {
                nodes[key].addTracks(item.tracks());
            }
            else {
                nodes[key] = item;
            }
        }
        trackParents.merge(data.trackParents);

        const QModelIndex allIndex = self->indexOfItem(&allNode);
        QMetaObject::invokeMethod(self, "dataChanged", Q_ARG(const QModelIndex&, allIndex),
                                  Q_ARG(const QModelIndex&, allIndex), Q_ARG(const QList<int>&, {Qt::DisplayRole}));

        if(resetting) {
            for(const auto& [parentKey, rows] : data.nodes) {
                auto* parent = parentKey == "0"_L1 ? self->rootItem() : &nodes.at(parentKey);

                for(const QString& row : rows) {
                    LibraryTreeItem* child = &nodes.at(row);
                    parent->appendChild(child);
                    child->setPending(false);
                }
            }
            sortTree();
        }
        else {
            updatePendingNodes(data);
        }
        updateAllNode();
    }

    void beginReset()
    {
        self->resetRoot();
        nodes.clear();
        pendingNodes.clear();
        addedNodes.clear();

        allNode = LibraryTreeItem{u"All Music"_s, self->rootItem(), -1};
        self->rootItem()->appendChild(&allNode);
    }
};

LibraryTreeModel::LibraryTreeModel(QObject* parent)
    : TreeModel{parent}
    , p{std::make_unique<Private>(this)}
{
    QObject::connect(&p->populator, &LibraryTreePopulator::populated, this,
                     [this](const PendingTreeData& data) { p->batchFinished(data); });

    QObject::connect(&p->populator, &Utils::Worker::finished, this, [this]() {
        p->updateAllNode();
        p->populator.stopThread();
        p->populatorThread.quit();
    });
}

LibraryTreeModel::~LibraryTreeModel()
{
    p->populator.stopThread();
    p->populatorThread.quit();
    p->populatorThread.wait();
}

void LibraryTreeModel::setAppearance(const LibraryTreeAppearance& options)
{
    p->font      = options.font;
    p->colour    = options.colour;
    p->rowHeight = options.rowHeight;
    emit dataChanged({}, {});
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

    return "Library Tree";
}

QVariant LibraryTreeModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    const auto* item = static_cast<LibraryTreeItem*>(index.internalPointer());

    switch(role) {
        case(Qt::DisplayRole): {
            const QString& name = item->title();
            return !name.isEmpty() ? name : u"?"_s;
        }
        case(LibraryTreeRole::Title): {
            return item->title();
        }
        case(LibraryTreeRole::Level): {
            return item->level();
        }
        case(LibraryTreeRole::Tracks): {
            return QVariant::fromValue(item->tracks());
        }
        case(Qt::SizeHintRole): {
            return QSize{0, p->rowHeight};
        }
        case(Qt::FontRole): {
            return p->font;
        }
        case(Qt::ForegroundRole): {
            return p->colour;
        }
        default: {
            return {};
        }
    }
}

bool LibraryTreeModel::hasChildren(const QModelIndex& parent) const
{
    if(!parent.isValid()) {
        return true;
    }
    LibraryTreeItem* item = itemForIndex(parent);
    return item->childCount() > 0 || p->pendingNodes.contains(item->key());
}

void LibraryTreeModel::fetchMore(const QModelIndex& parent)
{
    auto* parentItem = itemForIndex(parent);
    auto& rows       = p->pendingNodes[parentItem->key()];

    const int row           = parentItem->childCount();
    const int totalRows     = static_cast<int>(rows.size());
    const int rowCount      = parent.isValid() ? totalRows : std::min(100, totalRows);
    const auto rowsToInsert = std::ranges::views::take(rows, rowCount);

    beginInsertRows(parent, row, row + rowCount - 1);
    for(const QString& pendingRow : rowsToInsert) {
        LibraryTreeItem* child = &p->nodes.at(pendingRow);
        parentItem->appendChild(child);
        child->setPending(false);
        p->addedNodes.erase(pendingRow);
    }
    endInsertRows();

    emit layoutAboutToBeChanged();
    p->sortTree();
    emit layoutChanged();

    rows.erase(rows.begin(), rows.begin() + rowCount);

    if(rows.empty()) {
        p->pendingNodes.erase(parentItem->key());
    }
}

bool LibraryTreeModel::canFetchMore(const QModelIndex& parent) const
{
    auto* item = itemForIndex(parent);
    return p->pendingNodes.contains(item->key()) && !p->pendingNodes[item->key()].empty();
}

QStringList LibraryTreeModel::mimeTypes() const
{
    return {Constants::Mime::TrackList};
}

Qt::DropActions LibraryTreeModel::supportedDragActions() const
{
    return Qt::CopyAction;
}

QMimeData* LibraryTreeModel::mimeData(const QModelIndexList& indexes) const
{
    auto* mimeData = new QMimeData();
    mimeData->setData(Constants::Mime::TrackList, saveTracks(indexes));
    return mimeData;
}

void LibraryTreeModel::addTracks(const Core::TrackList& tracks)
{
    p->populatorThread.start();

    QMetaObject::invokeMethod(&p->populator, [this, tracks] { p->populator.run(p->grouping, tracks); });
}

void LibraryTreeModel::updateTracks(const Core::TrackList& tracks)
{
    removeTracks(tracks);
    addTracks(tracks);
}

void LibraryTreeModel::removeTracks(const Core::TrackList& tracks)
{
    std::set<LibraryTreeItem*, cmpItems> items;
    std::set<LibraryTreeItem*> pendingItems;

    for(const Core::Track& track : tracks) {
        const int id = track.id();
        if(p->trackParents.contains(id)) {
            const auto trackNodes = p->trackParents[id];
            for(const QString& node : trackNodes) {
                LibraryTreeItem* item = &p->nodes[node];
                item->removeTrack(track);
                if(item->pending()) {
                    pendingItems.emplace(item);
                }
                else {
                    items.emplace(item);
                }
            }
            p->trackParents.erase(id);
        }
    }

    for(const LibraryTreeItem* item : pendingItems) {
        if(item->trackCount() == 0) {
            p->pendingNodes.erase(item->key());
            p->nodes.erase(item->key());
        }
    }

    for(LibraryTreeItem* item : items) {
        if(item->trackCount() == 0) {
            LibraryTreeItem* parent = item->parent();
            const int row           = item->row();
            beginRemoveRows(indexOfItem(parent), row, row);
            parent->removeChild(row);
            parent->resetChildren();
            endRemoveRows();
            p->nodes.erase(item->key());
        }
    }

    p->updateAllNode();
}

void LibraryTreeModel::changeGrouping(const LibraryTreeGrouping& grouping)
{
    p->grouping = grouping.script;
}

void LibraryTreeModel::reset(const Core::TrackList& tracks)
{
    if(p->populatorThread.isRunning()) {
        p->populator.stopThread();
    }
    else {
        p->populatorThread.start();
    }

    p->resetting = true;

    QMetaObject::invokeMethod(&p->populator, [this, tracks] { p->populator.run(p->grouping, tracks); });
}
} // namespace Fy::Gui::Widgets

#include "moc_librarytreemodel.cpp"
