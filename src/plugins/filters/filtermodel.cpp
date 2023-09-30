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

#include <core/constants.h>
#include <core/scripting/scriptparser.h>

#include <utils/helpers.h>

#include <QSize>

namespace Fy::Filters {
struct FilterModel::Private
{
    FilterModel* model;

    bool resetting{false};

    FilterItem allNode;
    std::unordered_map<QString, FilterItem> nodes;
    std::unordered_map<int, std::vector<QString>> trackParents;

    FilterField* field;
    Qt::SortOrder sortOrder{Qt::AscendingOrder};

    int rowHeight{0};
    int fontSize{0};

    Core::Scripting::Registry registry;
    Core::Scripting::Parser parser{&registry};

    Private(FilterModel* model, FilterField* field)
        : model{model}
        , field{field}
    { }

    void beginReset()
    {
        model->resetRoot();
        nodes.clear();

        allNode = FilterItem{"", "", model->rootItem(), true};
        model->rootItem()->appendChild(&allNode);
    }

    FilterItem* createNode(const QString& title, const QString& sortTitle)
    {
        FilterItem* filterItem;
        if(!nodes.contains(title)) {
            filterItem = &nodes.emplace(title, FilterItem{title, sortTitle, model->rootItem()}).first->second;
            if(resetting) {
                model->rootItem()->appendChild(filterItem);
            }
            else {
                const int row = model->rootItem()->childCount();
                model->beginInsertRows({}, row, row);
                model->rootItem()->appendChild(filterItem);
                model->endInsertRows();
            }
        }
        filterItem = &nodes.at(title);
        return filterItem;
    }

    std::vector<FilterItem*> createNodes(const QStringList& titles, const QString& sortTitle)
    {
        std::vector<FilterItem*> items;
        for(const QString& title : titles) {
            auto* filterItem = createNode(title, sortTitle);
            items.emplace_back(filterItem);
        }
        return items;
    }

    void addTrackToNode(const Core::Track& track, FilterItem* node)
    {
        node->addTrack(track);
        trackParents[track.id()].push_back(node->title());
    }

    void populateModel(const Core::TrackList& tracks)
    {
        if(tracks.empty()) {
            return;
        }

        const auto parsedField = parser.parse(field->field);
        const auto parsedSort  = parser.parse(field->sortField);

        for(const Core::Track& track : tracks) {
            if(!track.enabled()) {
                continue;
            }
            const QString field = parser.evaluate(parsedField, track);
            const QString sort  = parser.evaluate(parsedSort, track);
            if(field.isNull()) {
                continue;
            }
            if(field.contains(Core::Constants::Separator)) {
                const QStringList values = field.split(Core::Constants::Separator);
                const auto nodes         = createNodes(values, sort);
                for(FilterItem* node : nodes) {
                    addTrackToNode(track, node);
                }
            }
            else {
                FilterItem* node = createNode(field, sort);
                addTrackToNode(track, node);
            }
            allNode.addTrack(track);
        }
    }
};

FilterModel::FilterModel(FilterField* field, QObject* parent)
    : TableModel{parent}
    , p{std::make_unique<Private>(this, field)}
{ }

FilterModel::~FilterModel() = default;

Qt::SortOrder FilterModel::sortOrder() const
{
    return p->sortOrder;
}

void FilterModel::setField(FilterField* field)
{
    p->field = field;
}

void FilterModel::setSortOrder(Qt::SortOrder order)
{
    p->sortOrder = order;
}

void FilterModel::setRowHeight(int height)
{
    p->rowHeight = height;
    emit layoutChanged({}, {});
}

void FilterModel::setFontSize(int size)
{
    p->fontSize = size;
    emit dataChanged({}, {}, {Qt::FontRole});
}

QVariant FilterModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item = static_cast<FilterItem*>(index.internalPointer());

    switch(role) {
        case(Qt::DisplayRole): {
            if(item->isAllNode()) {
                return QString("All (%1)").arg(p->nodes.size());
            }
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
        case(Qt::SizeHintRole): {
            return QSize{0, p->rowHeight};
        }
        case(Qt::FontRole): {
            return p->fontSize;
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

    if(!p->field || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    return p->field->name;
}

QHash<int, QByteArray> FilterModel::roleNames() const
{
    auto roles = QAbstractItemModel::roleNames();

    roles.insert(+FilterItemRole::Title, "Title");
    roles.insert(+FilterItemRole::Sorting, "Sorting");

    return roles;
}

void FilterModel::sortFilter()
{
    emit layoutAboutToBeChanged({});
    rootItem()->sortChildren(p->sortOrder);
    emit layoutChanged({});
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

void FilterModel::reload(const Core::TrackList& tracks)
{
    if(!p->field) {
        return;
    }

    p->resetting = true;

    beginResetModel();
    p->beginReset();
    p->populateModel(tracks);
    endResetModel();

    p->resetting = false;
}

void FilterModel::addTracks(const Core::TrackList& tracks)
{
    p->populateModel(tracks);
    sortFilter();
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

    //    p->updateAllNode();
}
} // namespace Fy::Filters
