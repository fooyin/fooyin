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

#include "filtermodel.h"

#include "filterfwd.h"
#include "filteritem.h"
#include "filterpopulator.h"

#include <core/track.h>
#include <gui/guiconstants.h>

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QIODevice>
#include <QMimeData>
#include <QSize>
#include <QThread>

#include <set>
#include <utility>

namespace {
QByteArray saveTracks(const QModelIndexList& indexes)
{
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);

    Fooyin::TrackIds trackIds;
    trackIds.reserve(indexes.size());

    for(const QModelIndex& index : indexes) {
        const auto tracks = index.data(Fooyin::Filters::FilterItem::Tracks).value<Fooyin::TrackList>();
        std::ranges::transform(std::as_const(tracks), std::back_inserter(trackIds),
                               [](const Fooyin::Track& track) { return track.id(); });
    }

    stream << trackIds;

    return result;
}
} // namespace

namespace Fooyin::Filters {
struct FilterModel::Private
{
    FilterModel* self;

    bool resetting{false};

    QThread populatorThread;
    FilterPopulator populator;

    FilterItem allNode;
    ItemKeyMap nodes;
    TrackIdNodeMap trackParents;

    FilterColumnList columns;
    int sortColumn{0};
    Qt::SortOrder sortOrder{Qt::AscendingOrder};

    using ColumnAlignments = std::map<int, Qt::Alignment>;
    ColumnAlignments columnAlignments;

    int rowHeight{0};
    QFont font;
    QColor colour;

    TrackList tracksPendingRemoval;

    explicit Private(FilterModel* self_)
        : self{self_}
    {
        populator.moveToThread(&populatorThread);
    }

    void beginReset()
    {
        self->resetRoot();
        nodes.clear();
        trackParents.clear();

        allNode = FilterItem{QStringLiteral(""), {}, self->rootItem()};
        self->rootItem()->appendChild(&allNode);
    }

    void batchFinished(PendingTreeData data)
    {
        if(resetting) {
            self->beginResetModel();
            beginReset();
        }

        if(!tracksPendingRemoval.empty()) {
            self->removeTracks(tracksPendingRemoval);
        }

        populateModel(data);

        if(resetting) {
            self->endResetModel();
        }
        resetting = false;

        QMetaObject::invokeMethod(self, &FilterModel::modelUpdated);
    }

    int uniqueValues(int column)
    {
        std::set<QString> columnUniques;

        const auto children = allNode.children();

        for(FilterItem* item : children) {
            columnUniques.emplace(item->column(column));
        }

        return static_cast<int>(columnUniques.size());
    }

    void updateAllNode()
    {
        const int columnCount = self->columnCount({});

        QStringList nodeColumns;
        for(int column{0}; column < columnCount; ++column) {
            nodeColumns.emplace_back(
                QString{tr("All (%1 %2s)")}.arg(uniqueValues(column)).arg(columns.at(column).name.toLower()));
        }

        allNode.setColumns(nodeColumns);
    }

    void populateModel(PendingTreeData& data)
    {
        for(const auto& [key, item] : data.items) {
            if(nodes.contains(key)) {
                nodes.at(key).addTracks(item.tracks());
            }
            else {
                if(!resetting) {
                    const int row = allNode.childCount();
                    self->beginInsertRows(self->indexOfItem(&allNode), row, row);
                }

                FilterItem* child = &nodes.emplace(key, item).first->second;
                allNode.appendChild(child);

                if(!resetting) {
                    self->endInsertRows();
                }
            }
        }
        trackParents.merge(data.trackParents);

        allNode.sortChildren(sortColumn, sortOrder);
        updateAllNode();
    }
};

FilterModel::FilterModel(QObject* parent)
    : TreeModel{parent}
    , p{std::make_unique<Private>(this)}
{
    QObject::connect(&p->populator, &FilterPopulator::populated, this,
                     [this](const PendingTreeData& data) { p->batchFinished(data); });

    QObject::connect(&p->populator, &Worker::finished, this, [this]() {
        p->populator.stopThread();
        p->populatorThread.quit();
    });
}

FilterModel::~FilterModel()
{
    p->populator.stopThread();
    p->populatorThread.quit();
    p->populatorThread.wait();
}

int FilterModel::sortColumn() const
{
    return p->sortColumn;
};

Qt::SortOrder FilterModel::sortOrder() const
{
    return p->sortOrder;
}

void FilterModel::sortOnColumn(int column, Qt::SortOrder order)
{
    p->sortColumn = column;
    p->sortOrder  = order;

    emit layoutAboutToBeChanged();
    p->allNode.sortChildren(column, order);
    emit layoutChanged();
}

void FilterModel::setAppearance(const FilterOptions& options)
{
    p->font      = options.font;
    p->colour    = options.colour;
    p->rowHeight = options.rowHeight;
    emit dataChanged({}, {});
}

Qt::ItemFlags FilterModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(index);
    defaultFlags |= Qt::ItemIsDragEnabled;
    return defaultFlags;
}

QVariant FilterModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role != Qt::TextAlignmentRole && role != Qt::DisplayRole) {
        return {};
    }

    if(orientation == Qt::Orientation::Vertical) {
        return {};
    }

    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(section < 0 || section >= static_cast<int>(p->columns.size())) {
        return tr("Filter");
    }

    return p->columns.at(section).name;
}

QVariant FilterModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    const auto* item = itemForIndex(index);
    const int col    = index.column();

    switch(role) {
        case(Qt::DisplayRole): {
            const QString& name = item->column(col);
            return !name.isEmpty() ? name : QStringLiteral("?");
        }
        case(FilterItem::Tracks): {
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
        case(Qt::TextAlignmentRole): {
            return QVariant::fromValue(Qt::AlignVCenter | columnAlignment(col));
        }
        default: {
            return {};
        }
    }
}

bool FilterModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(role != Qt::TextAlignmentRole) {
        return false;
    }

    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    p->columnAlignments[index.column()] = value.value<Qt::Alignment>();

    emit dataChanged({}, {}, {Qt::TextAlignmentRole});

    return true;
}

int FilterModel::columnCount(const QModelIndex& /*parent*/) const
{
    return static_cast<int>(p->columns.size());
}

QStringList FilterModel::mimeTypes() const
{
    return {QString::fromLatin1(Constants::Mime::TrackIds)};
}

Qt::DropActions FilterModel::supportedDragActions() const
{
    return Qt::CopyAction;
}

QMimeData* FilterModel::mimeData(const QModelIndexList& indexes) const
{
    auto* mimeData = new QMimeData();
    mimeData->setData(QString::fromLatin1(Constants::Mime::TrackIds), saveTracks(indexes));
    return mimeData;
}

Qt::Alignment FilterModel::columnAlignment(int column) const
{
    if(!p->columnAlignments.contains(column)) {
        return Qt::AlignLeft;
    }
    return p->columnAlignments.at(column);
}

void FilterModel::changeColumnAlignment(int column, Qt::Alignment alignment)
{
    p->columnAlignments[column] = alignment;
}

QModelIndexList FilterModel::indexesForValues(const QStringList& values, int column) const
{
    QModelIndexList indexes;

    const auto rows = p->allNode.children();
    for(const auto& child : rows) {
        if(values.contains(child->column(column))) {
            indexes.append(indexOfItem(child));
        }
    }

    return indexes;
}

void FilterModel::addTracks(const TrackList& tracks)
{
    TrackList tracksToAdd;
    std::ranges::copy_if(tracks, std::back_inserter(tracksToAdd),
                         [this](const Track& track) { return !p->trackParents.contains(track.id()); });

    if(tracksToAdd.empty()) {
        return;
    }

    p->populatorThread.start();

    QStringList columns;
    std::ranges::transform(p->columns, std::back_inserter(columns), [](const auto& column) { return column.field; });

    QMetaObject::invokeMethod(&p->populator, [this, columns, tracksToAdd] { p->populator.run(columns, tracksToAdd); });
}

void FilterModel::updateTracks(const TrackList& tracks)
{
    TrackList tracksToUpdate;
    std::ranges::copy_if(tracks, std::back_inserter(tracksToUpdate),
                         [this](const Track& track) { return p->trackParents.contains(track.id()); });

    if(tracksToUpdate.empty()) {
        return;
    }

    p->tracksPendingRemoval = tracks;
    addTracks(tracks);
}

void FilterModel::removeTracks(const TrackList& tracks)
{
    std::set<FilterItem*> items;

    for(const Track& track : tracks) {
        const int id = track.id();
        if(p->trackParents.contains(id)) {
            const auto trackNodes = p->trackParents[id];
            for(const QString& node : trackNodes) {
                FilterItem* item = &p->nodes[node];
                item->removeTrack(track);
                items.emplace(item);
            }
            p->trackParents.erase(id);
        }
    }

    for(FilterItem* item : items) {
        if(item->trackCount() == 0) {
            const QModelIndex parentIndex = indexOfItem(&p->allNode);
            const int row                 = item->row();
            beginRemoveRows(parentIndex, row, row);
            p->allNode.removeChild(row);
            p->allNode.resetChildren();
            endRemoveRows();
            p->nodes.erase(item->key());
        }
    }

    p->updateAllNode();
}

void FilterModel::reset(const FilterColumnList& columns, const TrackList& tracks)
{
    if(p->populatorThread.isRunning()) {
        p->populator.stopThread();
    }
    else {
        p->populatorThread.start();
    }

    p->columns = columns;

    p->resetting = true;

    QStringList fields;
    std::ranges::transform(p->columns, std::back_inserter(fields), [](const auto& column) { return column.field; });

    QMetaObject::invokeMethod(&p->populator, [this, fields, tracks] { p->populator.run(fields, tracks); });
}
} // namespace Fooyin::Filters
