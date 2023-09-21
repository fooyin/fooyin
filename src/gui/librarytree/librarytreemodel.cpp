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
#include "librarytreepopulator.h"

#include <QThread>

#include <ranges>

namespace Fy::Gui::Widgets {
struct LibraryTreeModel::Private : public QObject
{
    LibraryTreeModel* model;

    QString grouping;

    bool resetting{false};

    QThread populatorThread;
    LibraryTreePopulator populator;

    LibraryTreeItem allNode;
    NodeKeyMap pendingNodes;
    ItemKeyMap nodes;
    ItemKeyMap oldNodes;

    explicit Private(LibraryTreeModel* model)
        : model{model}
    {
        populator.moveToThread(&populatorThread);
    }

    void sortTree() const
    {
        model->rootItem()->sortChildren();
        model->rootItem()->resetChildren();
    }

    void batchFinished(PendingTreeData data)
    {
        if(resetting) {
            model->beginResetModel();
            beginReset();
        }

        populateModel(data);

        if(resetting) {
            model->endResetModel();
        }
        resetting = false;

        if(model->canFetchMore({})) {
            model->fetchMore({});
        }
    }

    void populateModel(PendingTreeData& data)
    {
        nodes.merge(data.items);

        allNode.addTracks(data.tracks);
        allNode.setTitle(QString{"All Music (%1)"}.arg(allNode.trackCount()));

        const QModelIndex allIndex = model->indexOfItem(&allNode);
        emit model->dataChanged(allIndex, allIndex, {Qt::DisplayRole});

        if(resetting) {
            for(const auto& [parentKey, rows] : data.nodes) {
                auto* parent = parentKey == "0" ? model->rootItem() : &nodes.at(parentKey);

                for(const QString& row : rows) {
                    LibraryTreeItem* child = &nodes.at(row);
                    parent->appendChild(child);
                }
            }
            sortTree();
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

        allNode = LibraryTreeItem{"", model->rootItem(), -1};
        model->rootItem()->appendChild(&allNode);
    }
};

LibraryTreeModel::LibraryTreeModel(QObject* parent)
    : TreeModel{parent}
    , p{std::make_unique<Private>(this)}
{
    QObject::connect(&p->populator, &LibraryTreePopulator::populated, p.get(),
                     &LibraryTreeModel::Private::batchFinished);

    QObject::connect(&p->populator, &Utils::Worker::finished, this, [this]() {
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

QVariant LibraryTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)

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

    auto* item = static_cast<LibraryTreeItem*>(index.internalPointer());

    if(role == Qt::DisplayRole) {
        const QString& name = item->title();
        return !name.isEmpty() ? name : "?";
    }

    const auto type = static_cast<LibraryTreeRole>(role);
    if(type == LibraryTreeRole::Title) {
        return item->title();
    }

    if(type == LibraryTreeRole::Tracks) {
        return QVariant::fromValue(item->tracks());
    }
    return {};
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

void LibraryTreeModel::changeGrouping(const LibraryTreeGrouping& grouping)
{
    p->grouping = grouping.script;
}

void LibraryTreeModel::reset(const Core::TrackList& tracks)
{
    if(tracks.empty()) {
        return;
    }

    if(p->populatorThread.isRunning()) {
        p->populator.stopThread();
    }
    else {
        p->populatorThread.start();
    }

    p->resetting = true;

    QMetaObject::invokeMethod(&p->populator, [this, tracks] {
        p->populator.run(p->grouping, tracks);
    });
}
} // namespace Fy::Gui::Widgets
