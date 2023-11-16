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

#include "filtermodel.h"

#include "filterfwd.h"
#include "filteritem.h"
#include "filterpopulator.h"

#include <gui/guiconstants.h>

#include <QColor>
#include <QFont>
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

    Fooyin::TrackList tracks;
    tracks.reserve(indexes.size());

    for(const QModelIndex& index : indexes) {
        std::ranges::copy(index.data(Fooyin::Filters::FilterItem::Tracks).value<Fooyin::TrackList>(),
                          std::back_inserter(tracks));
    }

    stream << tracks;

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

    FilterField field;
    Qt::SortOrder sortOrder{Qt::AscendingOrder};

    int rowHeight{0};
    QFont font;
    QColor colour;

    Private(FilterModel* self, FilterField field)
        : self{self}
        , field{std::move(field)}
    {
        populator.moveToThread(&populatorThread);
    }

    void beginReset()
    {
        self->resetRoot();
        nodes.clear();
        trackParents.clear();

        allNode = FilterItem{QStringLiteral(""), QStringLiteral(""), self->rootItem(), true};
        self->rootItem()->appendChild(&allNode);
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
    }

    void populateModel(PendingTreeData& data)
    {
        for(const auto& [key, item] : data.items) {
            if(nodes.contains(key)) {
                nodes[key].addTracks(item.tracks());
            }
            else {
                nodes[key]        = item;
                FilterItem* child = &nodes.at(key);

                if(!resetting) {
                    const int row = self->rootItem()->childCount();
                    self->beginInsertRows({}, row, row);
                }

                self->rootItem()->appendChild(child);

                if(!resetting) {
                    self->endInsertRows();
                }
            }
        }
        trackParents.merge(data.trackParents);

        self->rootItem()->sortChildren(sortOrder);

        allNode.setTitle(QString{QStringLiteral("All (%1)")}.arg(self->rootItem()->childCount() - 1));
    }
};

FilterModel::FilterModel(const FilterField& field, QObject* parent)
    : TableModel{parent}
    , p{std::make_unique<Private>(this, field)}
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
};

Qt::SortOrder FilterModel::sortOrder() const
{
    return p->sortOrder;
}

void FilterModel::setSortOrder(Qt::SortOrder order)
{
    p->sortOrder = order;

    emit layoutAboutToBeChanged();
    rootItem()->sortChildren(order);
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

QVariant FilterModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    const auto* item = static_cast<FilterItem*>(index.internalPointer());

    switch(role) {
        case(Qt::DisplayRole): {
            const QString& name = item->title();
            return !name.isEmpty() ? name : QStringLiteral("?");
        }
        case(FilterItem::Title): {
            return item->title();
        }
        case(FilterItem::Tracks): {
            return QVariant::fromValue(item->tracks());
        }
        case(FilterItem::Sorting): {
            return item->sortTitle();
        }
        case(FilterItem::AllNode): {
            return item->isAllNode();
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

QVariant FilterModel::headerData(int /*section*/, Qt::Orientation orientation, int role) const
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

    return p->field.name;
}

QHash<int, QByteArray> FilterModel::roleNames() const
{
    auto roles = QAbstractItemModel::roleNames();

    roles.insert(+FilterItem::Title, "Title");
    roles.insert(+FilterItem::Sorting, "Sorting");

    return roles;
}

QStringList FilterModel::mimeTypes() const
{
    return {Constants::Mime::TrackList};
}

Qt::DropActions FilterModel::supportedDragActions() const
{
    return Qt::CopyAction;
}

QMimeData* FilterModel::mimeData(const QModelIndexList& indexes) const
{
    auto* mimeData = new QMimeData();
    mimeData->setData(Constants::Mime::TrackList, saveTracks(indexes));
    return mimeData;
}

// QModelIndexList FilterModel::match(const QModelIndex& start, int role, const QVariant& value, int hits,
//                                    Qt::MatchFlags flags) const
//{
//     if(role != Qt::DisplayRole) {
//         return QAbstractItemModel::match(start, role, value, hits, flags);
//     }

//    QModelIndexList indexes{};
//    for(int i = 0; i < rowCount(start); ++i) {
//        const auto child = index(i, 0, start);
//        const auto data  = child.data(role);
//        if(data.toInt() == value.toInt()) {
//            indexes.append(child);
//        }
//    }
//    return indexes;
//}

void FilterModel::addTracks(const TrackList& tracks)
{
    p->populatorThread.start();

    QMetaObject::invokeMethod(&p->populator,
                              [this, tracks] { p->populator.run(p->field.field, p->field.sortField, tracks); });
}

void FilterModel::updateTracks(const TrackList& tracks)
{
    removeTracks(tracks);
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
            FilterItem* parent = item->parent();
            const int row      = item->row();
            beginRemoveRows(indexOfItem(parent), row, row);
            parent->removeChild(row);
            parent->resetChildren();
            endRemoveRows();
            p->nodes.erase(item->title());
        }
    }
}

void FilterModel::reset(const FilterField& field, const TrackList& tracks)
{
    if(p->populatorThread.isRunning()) {
        p->populator.stopThread();
    }
    else {
        p->populatorThread.start();
    }

    p->field = field;

    p->resetting = true;

    QMetaObject::invokeMethod(&p->populator,
                              [this, tracks] { p->populator.run(p->field.field, p->field.sortField, tracks); });
}
} // namespace Fooyin::Filters
