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

#include "constants.h"
#include "fieldparser.h"
#include "filterfwd.h"
#include "filteritem.h"

#include <core/constants.h>

namespace Fy::Filters {
FilterModel::FilterModel(FilterField* field, QObject* parent)
    : QAbstractListModel{parent}
    , m_root{std::make_unique<FilterItem>()}
    , m_field{field}
    , m_parser{std::make_unique<Scripting::FieldParser>()}
{ }

void FilterModel::setField(FilterField* field)
{
    m_field = field;
}

QVariant FilterModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid() && !checkIndex(index)) {
        return {};
    }

    auto* item = static_cast<FilterItem*>(index.internalPointer());

    switch(role) {
        case(Qt::DisplayRole): {
            const QString& name = item->data(Filters::Constants::Role::Title).toString();
            return !name.isEmpty() ? name : "?";
        }
        case(Filters::Constants::Role::Title): {
            return item->data(Filters::Constants::Role::Title);
        }
        case(Filters::Constants::Role::Tracks): {
            return item->data(Filters::Constants::Role::Tracks);
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

    return QAbstractListModel::flags(index);
}

QVariant FilterModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)
    if(!m_field) {
        return {};
    }

    if(orientation == Qt::Orientation::Vertical) {
        return {};
    }

    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(role != Qt::DisplayRole) {
        return {};
    }

    return m_field->name;
}

QModelIndex FilterModel::index(int row, int column, const QModelIndex& parent) const
{
    if(!hasIndex(row, column, parent)) {
        return {};
    }

    FilterItem* parentItem = m_root.get();

    FilterItem* childItem = parentItem->child(row);
    if(childItem) {
        return createIndex(row, column, childItem);
    }
    return {};
}

QModelIndex FilterModel::parent(const QModelIndex& index) const
{
    Q_UNUSED(index)
    return {};
}

int FilterModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return m_root->childCount();
}

int FilterModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return m_root->columnCount();
}

void FilterModel::sort(int column, Qt::SortOrder order)
{
    Q_UNUSED(column)
    m_root->sortChildren(order);
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

// TODO: Implement methods to insert/delete rows
void FilterModel::reload(const Core::TrackPtrList& tracks)
{
    if(!m_field) {
        return;
    }
    beginResetModel();
    beginReset();
    setupModelData(tracks);
    endResetModel();
}

void FilterModel::beginReset()
{
    m_root.reset();
    m_nodes.clear();
    m_root = std::make_unique<FilterItem>();
}

void FilterModel::setupModelData(const Core::TrackPtrList& tracks)
{
    if(tracks.empty()) {
        return;
    }

    m_allNode = std::make_unique<FilterItem>("", m_root.get(), true);
    m_root->appendChild(m_allNode.get());

    const auto parsedField = m_parser->parse(m_field->field);
    const auto parsedSort  = m_parser->parse(m_field->sortField);

    for(Core::Track* track : tracks) {
        const QString field = m_parser->evaluate(parsedField, track);
        const QString sort  = m_parser->evaluate(parsedSort, track);
        if(field.isNull()) {
            continue;
        }
        if(field.contains(Core::Constants::Separator)) {
            const QStringList values = field.split(Core::Constants::Separator);
            const auto nodes         = createNodes(values, sort);
            for(FilterItem* node : nodes) {
                node->addTrack(track);
            }
        }
        else {
            createNode(field, sort)->addTrack(track);
        }
        m_allNode->addTrack(track);
    }
    m_allNode->changeTitle(QString("All (%1)").arg(m_nodes.size()));
}

FilterItem* FilterModel::createNode(const QString& title, const QString& sortTitle)
{
    FilterItem* filterItem;
    if(!m_nodes.count(title)) {
        filterItem = m_nodes.emplace(title, std::make_unique<FilterItem>(title, m_root.get())).first->second.get();
        filterItem->setSortTitle(sortTitle);
        m_root->appendChild(filterItem);
    }
    filterItem = m_nodes.at(title).get();
    return filterItem;
}

std::vector<FilterItem*> FilterModel::createNodes(const QStringList& titles, const QString& sortTitle)
{
    std::vector<FilterItem*> items;
    for(const QString& title : titles) {
        auto* filterItem = createNode(title, sortTitle);
        items.emplace_back(filterItem);
    }
    return items;
}
} // namespace Fy::Filters
