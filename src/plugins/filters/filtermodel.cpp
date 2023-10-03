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

#include <QColor>
#include <QFont>
#include <QSize>
#include <QThread>
#include <utility>

namespace Fy::Filters {
struct FilterModel::Private : QObject
{
    FilterModel* self;

    bool resetting{false};

    QThread populatorThread;
    FilterPopulator populator;

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

    void sortNodes() const
    {
        emit self->layoutAboutToBeChanged({});
        self->rootItem()->sortChildren(sortOrder);
        emit self->layoutChanged({});
    }

    void beginReset()
    {
        self->resetRoot();
        nodes.clear();
        trackParents.clear();
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
                nodes[key] = item;

                FilterItem* child = &nodes.at(key);
                self->rootItem()->appendChild(child);
            }
        }
        trackParents.merge(data.trackParents);

        sortNodes();
    }
};

FilterModel::FilterModel(const FilterField& field, QObject* parent)
    : TableModel{parent}
    , p{std::make_unique<Private>(this, field)}
{
    QObject::connect(&p->populator, &FilterPopulator::populated, p.get(), &FilterModel::Private::batchFinished);

    QObject::connect(&p->populator, &Utils::Worker::finished, this, [this]() {
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
    p->sortNodes();
    emit layoutChanged();
}

void FilterModel::setAppearance(const FilterOptions& options)
{
    p->font      = options.font;
    p->colour    = options.colour;
    p->rowHeight = options.rowHeight;
    emit dataChanged({}, {});
}

QVariant FilterModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item = static_cast<FilterItem*>(index.internalPointer());

    switch(role) {
        case(Qt::DisplayRole): {
            const QString& name = item->title();
            return !name.isEmpty() ? name : "?";
        }
        case(FilterItemRole::Title): {
            return item->title();
        }
        case(FilterItemRole::Tracks): {
            return QVariant::fromValue(item->tracks());
        }
        case(FilterItemRole::Sorting): {
            return item->sortTitle();
        }
        case(FilterItemRole::AllNode): {
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

Qt::ItemFlags FilterModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }

    return TableModel::flags(index);
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

    roles.insert(+FilterItemRole::Title, "Title");
    roles.insert(+FilterItemRole::Sorting, "Sorting");

    return roles;
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

void FilterModel::addTracks(const Core::TrackList& tracks)
{
    p->populatorThread.start();

    QMetaObject::invokeMethod(&p->populator, [this, tracks] {
        p->populator.run(p->field.field, p->field.sortField, tracks);
    });
}

void FilterModel::updateTracks(const Core::TrackList& tracks)
{
    removeTracks(tracks);
    addTracks(tracks);
}

void FilterModel::removeTracks(const Core::TrackList& tracks)
{
    std::set<FilterItem*> items;

    for(const Core::Track& track : tracks) {
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

void FilterModel::reset(const FilterField& field, const Core::TrackList& tracks)
{
    if(p->populatorThread.isRunning()) {
        p->populator.stopThread();
    }
    else {
        p->populatorThread.start();
    }

    p->field = field;

    p->resetting = true;

    QMetaObject::invokeMethod(&p->populator, [this, tracks] {
        p->populator.run(p->field.field, p->field.sortField, tracks);
    });
}
} // namespace Fy::Filters
